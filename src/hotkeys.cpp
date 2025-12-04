#include "hotkeys.h"
#include "capture.h"
#include "overlay.h"
#include <stdio.h>

namespace ScreenCapture {

// Debug logging helper
static void DebugLog(const wchar_t* format, ...) {
    FILE* f = _wfopen(L"debug_hotkeys.txt", L"a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        va_list args;
        va_start(args, format);
        vfwprintf(f, format, args);
        va_end(args);
        
        fwprintf(f, L"\n");
        fclose(f);
    }
}

bool RegisterHotkeys(HWND hwnd) {
    DebugLog(L"RegisterHotkeys called, hwnd=%p", hwnd);
    
    // PrintScreen - Full screen
    if (!RegisterHotKey(hwnd, HOTKEY_FULLSCREEN, 0, VK_SNAPSHOT)) {
        DebugLog(L"  ERROR: Failed to register HOTKEY_FULLSCREEN");
        return false;
    }
    
    // Ctrl + PrintScreen - Active window
    if (!RegisterHotKey(hwnd, HOTKEY_WINDOW, MOD_CONTROL, VK_SNAPSHOT)) {
        DebugLog(L"  ERROR: Failed to register HOTKEY_WINDOW");
        UnregisterHotKey(hwnd, HOTKEY_FULLSCREEN);
        return false;
    }
    
    // Shift + PrintScreen - Region
    if (!RegisterHotKey(hwnd, HOTKEY_REGION, MOD_SHIFT, VK_SNAPSHOT)) {
        DebugLog(L"  ERROR: Failed to register HOTKEY_REGION");
        UnregisterHotKey(hwnd, HOTKEY_FULLSCREEN);
        UnregisterHotKey(hwnd, HOTKEY_WINDOW);
        return false;
    }
    
    DebugLog(L"  All hotkeys registered successfully");
    return true;
}

void UnregisterHotkeys(HWND hwnd) {
    UnregisterHotKey(hwnd, HOTKEY_FULLSCREEN);
    UnregisterHotKey(hwnd, HOTKEY_WINDOW);
    UnregisterHotKey(hwnd, HOTKEY_REGION);
}

void HandleHotkey(int hotkeyId) {
    DebugLog(L"=== HandleHotkey: id=%d ===", hotkeyId);
    
    switch (hotkeyId) {
        case HOTKEY_FULLSCREEN:
            DebugLog(L"  HOTKEY_FULLSCREEN - calling CaptureFullScreen()");
            CaptureFullScreen();
            DebugLog(L"  CaptureFullScreen() returned");
            break;
            
        case HOTKEY_WINDOW:
            DebugLog(L"  HOTKEY_WINDOW - calling CaptureActiveWindow()");
            CaptureActiveWindow();
            DebugLog(L"  CaptureActiveWindow() returned");
            break;
            
        case HOTKEY_REGION: {
            DebugLog(L"  HOTKEY_REGION - creating Overlay");
            Overlay overlay;
            DebugLog(L"  Overlay created, calling Show()...");
            bool result = overlay.Show();
            DebugLog(L"  overlay.Show() returned: %d", result);
            
            if (result) {
                RECT rect = overlay.GetSelectedRegion();
                DebugLog(L"  Selected rect: L=%d T=%d R=%d B=%d", rect.left, rect.top, rect.right, rect.bottom);
                
                // Small delay to ensure overlay is fully closed
                DebugLog(L"  Sleeping 100ms...");
                Sleep(100);
                
                DebugLog(L"  Calling CaptureRegion()...");
                bool captureResult = CaptureRegion(rect);
                DebugLog(L"  CaptureRegion() returned: %d", captureResult);
            } else {
                DebugLog(L"  Overlay was cancelled (ESC pressed)");
            }
            break;
        }
    }
    
    DebugLog(L"=== HandleHotkey completed ===\n");
}

} // namespace ScreenCapture
