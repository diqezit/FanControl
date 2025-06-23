// A background application to force a Lenovo laptop's fan to its highest speed.
// It runs in the system tray and restores normal fan speed upon exit.
// Requires Lenovo Energy/ACPI driver.

#include <Windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include <memory>
#include "resource.h"

//================================================================================
// Constants and Global Variables
//================================================================================
constexpr WCHAR APP_WINDOW_CLASS[] = L"__FanControlHiddenWindow";
constexpr WCHAR APP_MUTEX_NAME[] = L"Global\\FanControlSingletonMutex";
constexpr WCHAR APP_NAME[] = L"FanControl";
constexpr WCHAR REGISTRY_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr UINT  WM_TRAY_ICON_MSG = WM_APP + 1;
constexpr UINT_PTR TIMER_ID = 1;
constexpr DWORD FAST_MODE_RESET_INTERVAL = 8800;
constexpr int MAX_DRIVER_ERRORS = 5;

HANDLE g_hMutex = NULL;

enum class FanMode {
    Unknown,
    Normal,
    Fast
};

//================================================================================
// Fan Driver Interface
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

//================================================================================
// System Tray Icon Management
//================================================================================
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
        Update(hInstance, IDI_ICON1, L"FanControl (LMB - Menu, RMB - Exit)");
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

//================================================================================
// Windows Autostart Management
//================================================================================
class AutoStartManager
{
public:
    static bool IsEnabled()
    {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        WCHAR path[MAX_PATH];
        DWORD size = sizeof(path);
        DWORD type;
        LONG result = RegQueryValueEx(hKey, APP_NAME, NULL, &type, (LPBYTE)path, &size);
        RegCloseKey(hKey);

        return result == ERROR_SUCCESS && type == REG_SZ;
    }

    static bool Enable()
    {
        WCHAR path[MAX_PATH];
        if (GetModuleFileName(NULL, path, MAX_PATH) == 0) {
            return false;
        }

        return SetRegistryValue(path);
    }

    static bool Disable()
    {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        LONG result = RegDeleteValue(hKey, APP_NAME);
        RegCloseKey(hKey);

        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

private:
    static bool SetRegistryValue(const WCHAR* path)
    {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        LONG result = RegSetValueEx(hKey, APP_NAME, 0, REG_SZ, (LPBYTE)path, (lstrlen(path) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);

        return result == ERROR_SUCCESS;
    }
};

//================================================================================
// Fan Control Logic
//================================================================================
class FanController
{
public:
    FanController(FanDriver* driver, TrayIcon* icon)
        : m_driver(driver), m_icon(icon), m_lastActionTime(0), m_errorCounter(0) {
    }

    void ProcessFanLogic(HWND hWnd)
    {
        ULONGLONG currentTime = GetTickCount64();
        FanMode currentMode = m_driver->GetMode();

        if (HandleDriverError(currentMode, hWnd)) {
            return;
        }

        MaintainFastMode(currentMode, currentTime);
    }

private:
    bool HandleDriverError(FanMode mode, HWND hWnd)
    {
        if (mode != FanMode::Unknown) {
            if (m_errorCounter > 0) {
                m_errorCounter = 0;
                m_icon->Update(GetModuleHandle(NULL), IDI_ICON1, L"FanControl (LMB - Menu, RMB - Exit)");
            }
            return false;
        }

        m_errorCounter++;
        if (m_errorCounter == 1) {
            m_icon->Update(GetModuleHandle(NULL), IDI_ICON2, L"FanControl: Driver communication error!");
        }

        if (m_errorCounter >= MAX_DRIVER_ERRORS) {
            KillTimer(hWnd, TIMER_ID);
            MessageBox(hWnd, L"Failed to communicate with the fan driver multiple times. The application will now exit.",
                L"Critical Error", MB_OK | MB_ICONERROR);
            DestroyWindow(hWnd);
        }

        return true;
    }

    void MaintainFastMode(FanMode currentMode, ULONGLONG currentTime)
    {
        if (currentMode == FanMode::Fast) {
            if (currentTime - m_lastActionTime > FAST_MODE_RESET_INTERVAL) {
                m_driver->SetMode(FanMode::Normal);
                m_driver->SetMode(FanMode::Fast);
                m_lastActionTime = currentTime;
            }
        }
        else if (currentMode == FanMode::Normal) {
            m_driver->SetMode(FanMode::Fast);
            m_lastActionTime = currentTime;
        }
    }

    FanDriver* m_driver;
    TrayIcon* m_icon;
    ULONGLONG m_lastActionTime;
    int m_errorCounter;
};

//================================================================================
// Application State and Globals
//================================================================================
std::unique_ptr<FanDriver> g_fanDriver;
std::unique_ptr<TrayIcon> g_trayIcon;
std::unique_ptr<FanController> g_fanController;

void ShowContextMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenu(hMenu, MF_STRING | (AutoStartManager::IsEnabled() ? MF_CHECKED : 0),
        IDM_AUTOSTART, L"Start with Windows");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hWnd);

    UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, hWnd, NULL);

    DestroyMenu(hMenu);

    switch (cmd) {
    case IDM_AUTOSTART:
        if (AutoStartManager::IsEnabled()) {
            AutoStartManager::Disable();
        }
        else {
            AutoStartManager::Enable();
        }
        break;
    case IDM_EXIT:
        DestroyWindow(hWnd);
        break;
    }
}

void RestoreNormalFanSpeedOnExit()
{
    if (!g_fanDriver) return;

    for (int i = 0; i < 3; ++i) {
        if (g_fanDriver->SetMode(FanMode::Normal) && g_fanDriver->GetMode() == FanMode::Normal) {
            return;
        }
        Sleep(100);
    }
    g_fanDriver->SetMode(FanMode::Normal);
}

void Cleanup()
{
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }
}

//================================================================================
// Window Procedure
//================================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg) {
    case WM_CREATE:
        g_trayIcon = std::make_unique<TrayIcon>();
        if (!g_trayIcon->Create(hWnd, GetModuleHandle(NULL))) {
            MessageBox(NULL, L"Could not create tray icon.", L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        g_fanController = std::make_unique<FanController>(g_fanDriver.get(), g_trayIcon.get());
        g_fanDriver->SetMode(FanMode::Fast);
        SetTimer(hWnd, TIMER_ID, 200, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            g_fanController->ProcessFanLogic(hWnd);
        }
        return 0;

    case WM_TRAY_ICON_MSG:
        if (lParam == WM_LBUTTONUP) {
            ShowContextMenu(hWnd);
        }
        else if (lParam == WM_RBUTTONUP) {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        g_fanController.reset();
        g_trayIcon.reset();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

//================================================================================
// Application Initialization
//================================================================================
bool InitializeApplication(HINSTANCE hInstance)
{
    g_hMutex = CreateMutex(NULL, TRUE, APP_MUTEX_NAME);
    if (g_hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"Another instance of FanControl is already running.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    g_fanDriver = std::make_unique<FanDriver>();
    if (!g_fanDriver->IsInitialized()) {
        MessageBox(NULL, L"Could not open handle to fan driver.\nIs it installed and running correctly?",
            L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    WNDCLASS wc = {};
    wc.hInstance = hInstance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = APP_WINDOW_CLASS;
    if (!RegisterClass(&wc)) return false;

    if (g_fanDriver->GetMode() == FanMode::Unknown) {
        MessageBox(NULL, L"Could not communicate with fan driver.\nCheck permissions or driver status.",
            L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

//================================================================================
// Entry Point
//================================================================================
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
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