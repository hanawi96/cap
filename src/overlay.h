#pragma once
#include <windows.h>

namespace ScreenCapture {

class Overlay {
public:
    Overlay();
    ~Overlay();
    
    // Show overlay and wait for user selection
    bool Show();
    
    // Get selected region
    RECT GetSelectedRegion() const { return m_selectedRect; }
    
private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void OnPaint(HWND hwnd);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnKeyDown(WPARAM key);
    
    HWND m_hwnd;
    RECT m_selectedRect;
    POINT m_startPoint;
    POINT m_currentPoint;
    POINT m_windowOffset;  // Offset of overlay window
    bool m_isSelecting;
    bool m_isComplete;
    
    // Screenshot backbuffer (captured once at start)
    HDC m_hdcScreenshot;
    HBITMAP m_hbmScreenshot;
    HBITMAP m_hbmOldScreenshot;
    
    // Drawing backbuffer (for current frame)
    HDC m_hdcBackbuffer;
    HBITMAP m_hbmBackbuffer;
    HBITMAP m_hbmOldBackbuffer;
    int m_backbufferWidth;
    int m_backbufferHeight;
    
    // Cached GDI objects
    HPEN m_hPenBorder;
    HBRUSH m_hBrushDim;  // Semi-transparent overlay
    
    // Last drawn rectangle for selective redraw
    RECT m_lastRect;
    RECT m_prevRect;  // Previous rect before last update
    
    // Frame rate limiting
    DWORD m_lastPaintTime;
};

} // namespace ScreenCapture
