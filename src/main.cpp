#include <windows.h>
#include <dwmapi.h>
#include "hotkeys.h"
#include "capture.h"
#include "overlay.h"
#include "tray.h"
#include "utils.h"
#include <stdio.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

// DPI awareness function (for older MinGW)
typedef BOOL (WINAPI *SetProcessDPIAwareFunc)(void);

using namespace ScreenCapture;

static const wchar_t* WINDOW_CLASS = L"ScreenCaptureMainWindow";
static TrayIcon* g_trayIcon = nullptr;

// Debug logging
static void MainLog(const wchar_t* format, ...) {
    FILE* f = _wfopen(L"debug_main.txt", L"a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        va_list args;
        va_start(args, format);
        vfwprintf(f, format, args);
        va_end(args);
        
        fwprintf(f, L"\n");
        fflush(f);
        fclose(f);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            MainLog(L"WM_CREATE: hwnd=%p", hwnd);
            if (!RegisterHotkeys(hwnd)) {
                MessageBoxW(hwnd, L"Không thể đăng ký phím nóng!", L"Lỗi", MB_ICONERROR);
                return -1;
            }
            
            g_trayIcon = new TrayIcon();
            if (!g_trayIcon->Create(hwnd)) {
                MessageBoxW(hwnd, L"Không thể tạo tray icon!", L"Lỗi", MB_ICONERROR);
                return -1;
            }
            MainLog(L"  Initialization complete");
            break;
            
        case WM_HOTKEY:
            MainLog(L"WM_HOTKEY: id=%d", wParam);
            HandleHotkey((int)wParam);
            MainLog(L"WM_HOTKEY: HandleHotkey returned");
            break;
            
        case TrayIcon::WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                g_trayIcon->ShowContextMenu(hwnd);
            }
            break;
            
        case WM_COMMAND:
            MainLog(L"WM_COMMAND: id=%d", LOWORD(wParam));
            switch (LOWORD(wParam)) {
                case TrayIcon::MENU_CAPTURE_FULLSCREEN:
                    CaptureFullScreen();
                    break;
                    
                case TrayIcon::MENU_CAPTURE_WINDOW:
                    CaptureActiveWindow();
                    break;
                    
                case TrayIcon::MENU_CAPTURE_REGION: {
                    MainLog(L"  MENU_CAPTURE_REGION: Creating overlay...");
                    Overlay overlay;
                    MainLog(L"  Calling overlay.Show()...");
                    if (overlay.Show()) {
                        RECT rect = overlay.GetSelectedRegion();
                        MainLog(L"  Overlay returned true, rect: L=%d T=%d R=%d B=%d", rect.left, rect.top, rect.right, rect.bottom);
                        CaptureRegion(rect);
                        MainLog(L"  CaptureRegion returned");
                    } else {
                        MainLog(L"  Overlay returned false (cancelled)");
                    }
                    MainLog(L"  MENU_CAPTURE_REGION completed");
                    break;
                }
                    
                case TrayIcon::MENU_OPEN_FOLDER: {
                    std::wstring dir = GetSaveDirectory();
                    EnsureDirectoryExists(dir);
                    ShellExecuteW(NULL, L"open", dir.c_str(), NULL, NULL, SW_SHOW);
                    break;
                }
                    
                case TrayIcon::MENU_EXIT:
                    MainLog(L"  MENU_EXIT: Posting quit message");
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_DESTROY:
            MainLog(L"WM_DESTROY received!");
            UnregisterHotkeys(hwnd);
            if (g_trayIcon) {
                delete g_trayIcon;
                g_trayIcon = nullptr;
            }
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MainLog(L"=== Application Starting ===");
    
    // Set DPI awareness to get correct screen coordinates
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32) {
        SetProcessDPIAwareFunc pSetProcessDPIAware = 
            (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (pSetProcessDPIAware) {
            pSetProcessDPIAware();
        }
        FreeLibrary(hUser32);
    }
    
    // Prevent multiple instances
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"ScreenCaptureAppMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MainLog(L"Another instance already running, exiting");
        MessageBoxW(NULL, L"Ứng dụng đã đang chạy!", L"Thông báo", MB_ICONINFORMATION);
        return 0;
    }
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS;
    
    if (!RegisterClassExW(&wc)) {
        MainLog(L"Failed to register window class");
        MessageBoxW(NULL, L"Không thể đăng ký window class!", L"Lỗi", MB_ICONERROR);
        return 1;
    }
    
    // Create hidden window for message handling
    HWND hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        L"Screen Capture",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hwnd) {
        MainLog(L"Failed to create window");
        MessageBoxW(NULL, L"Không thể tạo window!", L"Lỗi", MB_ICONERROR);
        return 1;
    }
    
    MainLog(L"Main window created: hwnd=%p", hwnd);
    
    // Message loop
    MSG msg;
    MainLog(L"Entering message loop...");
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    MainLog(L"=== Message loop exited, wParam=%d ===", msg.wParam);
    
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    
    MainLog(L"=== Application Exiting ===\n");
    
    return (int)msg.wParam;
}
