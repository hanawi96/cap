#pragma once
#include <windows.h>
#include <string>

namespace ScreenCapture {

enum class CaptureMode {
    FullScreen,
    ActiveWindow,
    Region
};

// Capture full screen
bool CaptureFullScreen();

// Capture active window
bool CaptureActiveWindow();

// Capture specific region
bool CaptureRegion(const RECT& rect);

// Internal: Capture screen area to HBITMAP
HBITMAP CaptureScreenArea(int x, int y, int width, int height);

// Save captured bitmap
bool SaveCapture(HBITMAP hBitmap, const std::wstring& prefix);

} // namespace ScreenCapture
