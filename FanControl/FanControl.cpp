// A background application to force a Lenovo laptop's fan to its highest speed.
// It runs in the system tray and restores normal fan speed upon exit.
// Requires Lenovo Energy/ACPI driver.

#include <Windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include <memory>
#include "resource.h"

//================================================================================
// 1. Application-level Globals & Constants
//================================================================================
constexpr WCHAR APP_WINDOW_CLASS[] = L"__FanControlHiddenWindow";
constexpr WCHAR APP_MUTEX_NAME[] = L"Global\\FanControlSingletonMutex";
constexpr UINT  WM_TRAY_ICON_MSG = WM_APP + 1;

HANDLE g_hMutex = NULL;

enum class FanMode {
    Unknown,
    Normal,
    Fast
};

//================================================================================
// 2. Fan Driver Abstraction Class (SRP, DRY, RAII)
//================================================================================
class FanDriver
{
public:
    FanDriver() : m_hDriver(INVALID_HANDLE_VALUE)
    {
        m_hDriver = CreateFileW(
            DRIVER_PATH,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }

    ~FanDriver()
    {
        if (m_hDriver != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hDriver);
        }
    }

    bool IsInitialized() const
    {
        return m_hDriver != INVALID_HANDLE_VALUE;
    }

    bool SetMode(FanMode mode)
    {
        if (!IsInitialized() || mode == FanMode::Unknown) {
            return false;
        }

        DWORD inBuffer[3] = { 6, 1, (mode == FanMode::Fast) ? 1UL : 0UL };

        return DeviceIoControl(
            m_hDriver,
            IOCTL_WRITE_MODE,
            inBuffer,
            sizeof(inBuffer),
            NULL,
            0,
            NULL,
            NULL
        ) != 0;
    }

    FanMode GetMode()
    {
        if (!IsInitialized()) {
            return FanMode::Unknown;
        }

        DWORD inBuffer = 14;
        DWORD outBuffer = 0;
        DWORD bytesReturned = 0;

        BOOL result = DeviceIoControl(
            m_hDriver,
            IOCTL_READ_STATE,
            &inBuffer,
            sizeof(inBuffer),
            &outBuffer,
            sizeof(outBuffer),
            &bytesReturned,
            NULL
        );

        if (!result || bytesReturned != sizeof(outBuffer)) {
            return FanMode::Unknown;
        }

        return (outBuffer == DRIVER_STATE_VALUE_FOR_FAST) ? FanMode::Fast : FanMode::Normal;
    }

private:
    // IOCTL codes for Lenovo's Energy Management driver.
    static constexpr WCHAR DRIVER_PATH[] = L"\\\\.\\EnergyDrv";
    static constexpr DWORD IOCTL_WRITE_MODE = 0x831020C0;
    static constexpr DWORD IOCTL_READ_STATE = 0x831020C4;
    static constexpr DWORD DRIVER_STATE_VALUE_FOR_FAST = 3;

    HANDLE m_hDriver;
};

std::unique_ptr<FanDriver> g_fanDriver;

//================================================================================
// 3. Application Logic
//================================================================================
void RestoreNormalFanSpeedOnExit() {
    if (!g_fanDriver) return;

    for (int i = 0; i < 3; ++i) {
        if (g_fanDriver->SetMode(FanMode::Normal) && g_fanDriver->GetMode() == FanMode::Normal) {
            return;
        }
        Sleep(100);
    }
    g_fanDriver->SetMode(FanMode::Normal);
}

void Cleanup() {
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }
}

bool InitializeTrayIcon(HWND hWnd, NOTIFYICONDATA* nid) {
    memset(nid, 0, sizeof(*nid));
    nid->cbSize = sizeof(*nid);
    nid->hWnd = hWnd;
    nid->uID = 0;
    nid->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid->uCallbackMessage = WM_TRAY_ICON_MSG;

    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    nid->hIcon = (hIcon) ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);

    lstrcpy(nid->szTip, L"FanControl (RMB to Exit)");

    return Shell_NotifyIcon(NIM_ADD, nid);
}

void ProcessFanLogic(ULONGLONG& lastActionTime)
{
    const DWORD FAST_MODE_RESET_INTERVAL = 8800;
    ULONGLONG currentTime = GetTickCount64();
    FanMode currentMode = g_fanDriver->GetMode();

    if (currentMode == FanMode::Fast) {
        if (currentTime - lastActionTime > FAST_MODE_RESET_INTERVAL) {
            g_fanDriver->SetMode(FanMode::Normal);
            g_fanDriver->SetMode(FanMode::Fast);
            lastActionTime = currentTime;
        }
    }
    else if (currentMode == FanMode::Normal) {
        g_fanDriver->SetMode(FanMode::Fast);
        lastActionTime = currentTime;
    }
}

void RunMainLoop() {
    const DWORD UI_LOOP_INTERVAL = 50;

    g_fanDriver->SetMode(FanMode::Fast);
    ULONGLONG lastActionTime = GetTickCount64();

    bool keepRunning = true;
    while (keepRunning) {
        DWORD waitResult = MsgWaitForMultipleObjects(0, NULL, FALSE, UI_LOOP_INTERVAL, QS_ALLINPUT);

        if (waitResult == WAIT_OBJECT_0) {
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    keepRunning = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (!keepRunning) continue;
        }

        ProcessFanLogic(lastActionTime);
    }
}

LRESULT CALLBACK WndProc(
    HWND hWnd,
    UINT iMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    static NOTIFYICONDATA nid;

    switch (iMsg) {
    case WM_CREATE:
        if (!InitializeTrayIcon(hWnd, &nid)) {
            MessageBox(NULL, L"Could not create tray icon.", L"Error", MB_OK | MB_ICONERROR);
            PostQuitMessage(1);
        }
        return 0;
    case WM_TRAY_ICON_MSG:
        if (lParam == WM_RBUTTONUP) {
            PostQuitMessage(0);
        }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        if (nid.hIcon) DestroyIcon(nid.hIcon);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

bool InitializeApplication(HINSTANCE hInstance) {
    g_hMutex = CreateMutex(NULL, TRUE, APP_MUTEX_NAME);
    if (g_hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"Another instance of FanControl is already running.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    g_fanDriver = std::make_unique<FanDriver>();
    if (!g_fanDriver->IsInitialized()) {
        MessageBox(NULL, L"Could not open handle to fan driver.\nIs it installed and running correctly?", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    WNDCLASS wc = {};
    wc.hInstance = hInstance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = APP_WINDOW_CLASS;
    if (!RegisterClass(&wc)) return false;

    HWND hWnd = CreateWindow(
        APP_WINDOW_CLASS,
        L"FanControl Hidden Window",
        0, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );
    if (!hWnd) return false;

    if (g_fanDriver->GetMode() == FanMode::Unknown) {
        MessageBox(NULL, L"Could not communicate with fan driver.\nCheck permissions or driver status.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

//================================================================================
// 4. Main Entry Point
//================================================================================
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    if (!InitializeApplication(hInstance)) {
        Cleanup();
        return 1;
    }

    __try {
        RunMainLoop();
    }
    __finally {
        RestoreNormalFanSpeedOnExit();
        Cleanup();
    }

    return 0;
}