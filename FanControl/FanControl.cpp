// A background application to force a Lenovo laptop's fan to its highest speed.
// It runs in the system tray and restores normal fan speed upon exit.
// Requires Lenovo Energy/ACPI driver.

#include <Windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include "resource.h"

//================================================================================
// 1. Application-level Globals & Constants
//================================================================================
constexpr WCHAR APP_WINDOW_CLASS[] = L"__FanControlHiddenWindow";
constexpr WCHAR APP_MUTEX_NAME[] = L"Global\\FanControlSingletonMutex";
constexpr UINT  WM_TRAY_ICON_MSG = WM_APP + 1;

HANDLE g_hMutex = NULL;

//================================================================================
// 2. Fan Control Interface (High-Level API)
//================================================================================
enum class FanMode {
    Unknown,
    Normal,
    Fast
};

bool SetFanMode(FanMode mode);
FanMode GetFanMode();

//================================================================================
// 3. Fan Control Implementation (Low-Level Driver Communication)
//================================================================================
namespace FanControllerImpl {
    constexpr WCHAR DRIVER_PATH[] = L"\\\\.\\EnergyDrv";

    // IOCTL codes for Lenovo's Energy Management driver.
    constexpr DWORD IOCTL_WRITE_MODE = 0x831020C0;
    constexpr DWORD IOCTL_READ_STATE = 0x831020C4;

    // Driver's return value for the normal/automatic fan state.
    constexpr DWORD DRIVER_STATE_VALUE_FOR_NORMAL = 3;
}

bool SetFanMode(FanMode mode) {
    if (mode == FanMode::Unknown) {
        return false;
    }

    HANDLE hDriver = CreateFileW(
        FanControllerImpl::DRIVER_PATH,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDriver == INVALID_HANDLE_VALUE) {
        return false;
    }

    // The driver expects a 12-byte buffer: {6, 1, mode_flag}.
    // mode_flag: 0 for Normal/Auto, 1 for Fast/Dust-Cleaning.
    DWORD inBuffer[3] = { 6, 1, (mode == FanMode::Fast) ? 1UL : 0UL };

    BOOL result = DeviceIoControl(
        hDriver,
        FanControllerImpl::IOCTL_WRITE_MODE,
        inBuffer,
        sizeof(inBuffer),
        NULL,
        0,
        NULL,
        NULL
    );

    CloseHandle(hDriver);
    return result;
}

FanMode GetFanMode() {
    HANDLE hDriver = CreateFileW(
        FanControllerImpl::DRIVER_PATH,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDriver == INVALID_HANDLE_VALUE) {
        return FanMode::Unknown;
    }

    DWORD inBuffer = 14;
    DWORD outBuffer = 0;
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDriver,
        FanControllerImpl::IOCTL_READ_STATE,
        &inBuffer,
        sizeof(inBuffer),
        &outBuffer,
        sizeof(outBuffer),
        &bytesReturned,
        NULL
    );

    CloseHandle(hDriver);

    if (!result || bytesReturned != sizeof(outBuffer)) {
        return FanMode::Unknown;
    }

    return (outBuffer == FanControllerImpl::DRIVER_STATE_VALUE_FOR_NORMAL) ? FanMode::Normal : FanMode::Fast;
}

//================================================================================
// 4. Application Logic
//================================================================================
void RestoreNormalFanSpeedOnExit() {
    for (int i = 0; i < 3; ++i) {
        if (SetFanMode(FanMode::Normal) && GetFanMode() == FanMode::Normal) {
            return;
        }
        Sleep(250);
    }
    SetFanMode(FanMode::Normal);
}

void Cleanup() {
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
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

void ProcessFanLogic() {
    // Periodically re-assert fast mode in case it was reset by the system.
    if (GetFanMode() == FanMode::Normal) {
        if (!SetFanMode(FanMode::Fast)) {
            PostQuitMessage(1);
        }
    }
}

void RunMainLoop() {
    if (!SetFanMode(FanMode::Fast)) {
        MessageBox(NULL, L"Could not set initial fan speed. Is the driver installed?", L"Error", MB_OK | MB_ICONERROR);
        PostQuitMessage(1);
        return;
    }

    bool keepRunning = true;
    while (keepRunning) {
        DWORD waitResult = MsgWaitForMultipleObjects(
            0,
            NULL,
            FALSE,
            2000,
            QS_ALLINPUT
        );

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
        }
        else if (waitResult == WAIT_TIMEOUT) {
            ProcessFanLogic();
        }
    }
}

bool InitializeApplication(HINSTANCE hInstance) {
    g_hMutex = CreateMutex(NULL, TRUE, APP_MUTEX_NAME);
    if (g_hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"Another instance of FanControl is already running.", L"Error", MB_OK | MB_ICONERROR);
        if (g_hMutex) CloseHandle(g_hMutex);
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
    return hWnd != NULL;
}

//================================================================================
// 5. Main Entry Point
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