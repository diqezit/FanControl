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
constexpr UINT_PTR TIMER_ID = 1;

HANDLE g_hMutex = NULL;

enum class FanMode {
    Unknown,
    Normal,
    Fast
};

//================================================================================
// 2. Abstraction Classes
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

class TrayIcon
{
public:
    TrayIcon() { ZeroMemory(&m_nid, sizeof(m_nid)); }
    ~TrayIcon()
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        if (m_nid.hIcon) {
            DestroyIcon(m_nid.hIcon);
        }
    }

    bool Create(HWND hWnd, HINSTANCE hInstance)
    {
        m_nid.cbSize = sizeof(m_nid);
        m_nid.hWnd = hWnd;
        m_nid.uID = 0;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAY_ICON_MSG;
        Update(hInstance, IDI_ICON1, L"FanControl (RMB to Exit)");
        return Shell_NotifyIcon(NIM_ADD, &m_nid);
    }

    void Update(HINSTANCE hInstance, UINT iconId, const WCHAR* tip)
    {
        HICON hNewIcon = LoadIcon(hInstance, MAKEINTRESOURCE(iconId));
        if (hNewIcon) {
            if (m_nid.hIcon) {
                DestroyIcon(m_nid.hIcon);
            }
            m_nid.hIcon = hNewIcon;
        }
        lstrcpy(m_nid.szTip, tip);
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

private:
    NOTIFYICONDATA m_nid;
};

std::unique_ptr<FanDriver> g_fanDriver;
std::unique_ptr<TrayIcon>  g_trayIcon;

//================================================================================
// 3. Application Logic and Window Procedure
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

void ProcessFanLogic(ULONGLONG& lastActionTime, int& errorCounter, HWND hWnd)
{
    const DWORD FAST_MODE_RESET_INTERVAL = 8800;
    const int MAX_ERRORS = 5;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    ULONGLONG currentTime = GetTickCount64();
    FanMode currentMode = g_fanDriver->GetMode();

    if (currentMode == FanMode::Unknown) {
        errorCounter++;
        if (errorCounter == 1) {
            g_trayIcon->Update(hInstance, IDI_ICON2, L"FanControl: Driver communication error!");
        }
        if (errorCounter >= MAX_ERRORS) {
            KillTimer(hWnd, TIMER_ID);
            MessageBox(hWnd, L"Failed to communicate with the fan driver multiple times. The application will now exit.", L"Critical Error", MB_OK | MB_ICONERROR);
            DestroyWindow(hWnd);
        }
        return;
    }

    if (errorCounter > 0) {
        errorCounter = 0;
        g_trayIcon->Update(hInstance, IDI_ICON1, L"FanControl: Max Speed Active");
    }

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

LRESULT OnCreate(HWND hWnd) {
    g_trayIcon = std::make_unique<TrayIcon>();
    if (!g_trayIcon->Create(hWnd, GetModuleHandle(NULL))) {
        MessageBox(NULL, L"Could not create tray icon.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    g_fanDriver->SetMode(FanMode::Fast);
    SetTimer(hWnd, TIMER_ID, 200, NULL);
    return 0;
}

void OnTimer(HWND hWnd, ULONGLONG& lastActionTime, int& errorCounter) {
    ProcessFanLogic(lastActionTime, errorCounter, hWnd);
}

void OnTrayIcon(HWND hWnd, LPARAM lParam) {
    if (lParam == WM_RBUTTONUP) {
        DestroyWindow(hWnd);
    }
}

LRESULT CALLBACK WndProc(
    HWND hWnd,
    UINT iMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    static ULONGLONG lastActionTime = 0;
    static int errorCounter = 0;

    switch (iMsg) {
    case WM_CREATE:
        return OnCreate(hWnd);
    case WM_TIMER:
        if (wParam == TIMER_ID) {
            OnTimer(hWnd, lastActionTime, errorCounter);
        }
        return 0;
    case WM_TRAY_ICON_MSG:
        OnTrayIcon(hWnd, lParam);
        return 0;
    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        g_trayIcon.reset();
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

    HWND hWnd = CreateWindow(
        APP_WINDOW_CLASS,
        L"FanControl Hidden Window",
        0, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        Cleanup();
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RestoreNormalFanSpeedOnExit();
    Cleanup();

    return static_cast<int>(msg.wParam);
}