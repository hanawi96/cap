#include "capture.h"
#include "utils.h"
#include "preview.h"
#include <dwmapi.h>
#include <thread>
#include <mmsystem.h>
#include <stdio.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

namespace ScreenCapture {

// Debug logging helper
static void DebugLog(const wchar_t* format, ...) {
    FILE* f = _wfopen(L"debug_capture.txt", L"a");
    if (f) {
        // Timestamp
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

HBITMAP CaptureScreenArea(int x, int y, int width, int height) {
    DebugLog(L"CaptureScreenArea: x=%d, y=%d, w=%d, h=%d", x, y, width, height);
    
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        DebugLog(L"  ERROR: GetDC(NULL) failed");
        return NULL;
    }
    DebugLog(L"  GetDC OK");
    
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        DebugLog(L"  ERROR: CreateCompatibleDC failed");
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    DebugLog(L"  CreateCompatibleDC OK");
    
    // Create DIB section for faster access
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    
    if (hBitmap) {
        DebugLog(L"  CreateDIBSection OK, pBits=%p", pBits);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        // Use BitBlt with CAPTUREBLT flag to capture layered windows
        BOOL result = BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY | CAPTUREBLT);
        DebugLog(L"  BitBlt result=%d", result);
        
        SelectObject(hdcMem, hOldBitmap);
    } else {
        DebugLog(L"  ERROR: CreateDIBSection failed, GetLastError=%d", GetLastError());
    }
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    DebugLog(L"  Returning hBitmap=%p", hBitmap);
    return hBitmap;
}

bool SaveCapture(HBITMAP hBitmap, const std::wstring& prefix) {
    DebugLog(L"SaveCapture: hBitmap=%p, prefix=%s", hBitmap, prefix.c_str());
    
    if (!hBitmap) {
        DebugLog(L"  ERROR: hBitmap is NULL");
        return false;
    }
    
    std::wstring dir = GetSaveDirectory();
    DebugLog(L"  Save directory: %s", dir.c_str());
    
    if (!EnsureDirectoryExists(dir)) {
        DebugLog(L"  ERROR: Failed to create directory");
        DeleteObject(hBitmap);
        return false;
    }
    DebugLog(L"  Directory exists/created OK");
    
    std::wstring filename = dir + L"\\" + prefix + L"_" + GetTimestamp() + L".png";
    DebugLog(L"  Filename: %s", filename.c_str());
    
    // Get the directory where the exe is located
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    DebugLog(L"  Exe directory: %s", exeDir.c_str());
    
    // Try multiple sound file locations
    std::wstring soundPaths[] = {
        exeDir + L"\\sound.wav",           // Same folder as exe
        exeDir + L"\\..\\..\\src\\sound.wav", // When running from build/Release
        L"src\\sound.wav"                  // Current directory
    };
    
    bool soundPlayed = false;
    for (const auto& soundPath : soundPaths) {
        DebugLog(L"  Trying sound: %s", soundPath.c_str());
        if (GetFileAttributesW(soundPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            DebugLog(L"    Found! Playing...");
            BOOL playResult = PlaySoundW(soundPath.c_str(), NULL, SND_FILENAME | SND_ASYNC);
            DebugLog(L"    PlaySound result: %d", playResult);
            soundPlayed = true;
            break;
        }
    }
    if (!soundPlayed) {
        DebugLog(L"  WARNING: No sound file found");
    }
    
    // Show preview window (will self-delete when closed)
    DebugLog(L"  Creating PreviewWindow...");
    PreviewWindow* preview = new PreviewWindow();
    DebugLog(L"  PreviewWindow created at %p", preview);
    
    DebugLog(L"  Calling preview->Show()...");
    preview->Show(hBitmap, filename);
    DebugLog(L"  preview->Show() returned");
    
    // Save async using QueueUserWorkItem (faster than std::thread)
    struct SaveContext {
        HBITMAP hBitmap;
        wchar_t filename[MAX_PATH];
    };
    
    SaveContext* ctx = new SaveContext();
    ctx->hBitmap = hBitmap;
    wcscpy_s(ctx->filename, MAX_PATH, filename.c_str());
    
    DebugLog(L"  Queueing async save...");
    BOOL queueResult = QueueUserWorkItem([](PVOID param) -> DWORD {
        SaveContext* ctx = (SaveContext*)param;
        SaveBitmapToPNG(ctx->hBitmap, ctx->filename);
        DeleteObject(ctx->hBitmap);
        delete ctx;
        return 0;
    }, ctx, WT_EXECUTEDEFAULT);
    DebugLog(L"  QueueUserWorkItem result: %d", queueResult);
    
    DebugLog(L"  SaveCapture completed successfully");
    return true;
}

bool CaptureFullScreen() {
    DebugLog(L"=== CaptureFullScreen ===");
    RECT rect = GetVirtualScreenRect();
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    HBITMAP hBitmap = CaptureScreenArea(rect.left, rect.top, width, height);
    return SaveCapture(hBitmap, L"FullScreen");
}

bool CaptureActiveWindow() {
    DebugLog(L"=== CaptureActiveWindow ===");
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        DebugLog(L"  ERROR: No foreground window");
        return false;
    }
    
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        DebugLog(L"  ERROR: GetWindowRect failed");
        return false;
    }
    
    // Adjust for DWM shadow (Windows 10/11)
    RECT frame = {};
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(RECT)))) {
        rect = frame;
    }
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    if (width <= 0 || height <= 0) {
        DebugLog(L"  ERROR: Invalid dimensions w=%d h=%d", width, height);
        return false;
    }
    
    HBITMAP hBitmap = CaptureScreenArea(rect.left, rect.top, width, height);
    return SaveCapture(hBitmap, L"Window");
}

bool CaptureRegion(const RECT& rect) {
    DebugLog(L"=== CaptureRegion ===");
    DebugLog(L"  Rect: L=%d T=%d R=%d B=%d", rect.left, rect.top, rect.right, rect.bottom);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    DebugLog(L"  Size: %dx%d", width, height);
    
    if (width <= 0 || height <= 0) {
        DebugLog(L"  ERROR: Invalid dimensions");
        return false;
    }
    
    HBITMAP hBitmap = CaptureScreenArea(rect.left, rect.top, width, height);
    return SaveCapture(hBitmap, L"Region");
}

} // namespace ScreenCapture
