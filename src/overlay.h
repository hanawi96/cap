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
    
    // Persistent backbuffer for flicker-free rendering
    HDC m_hdcBackbuffer;
    HBITMAP m_hbmBackbuffer;
    HBITMAP m_hbmOldBackbuffer;
    int m_backbufferWidth;
    int m_backbufferHeight;
    
    // Cached GDI objects for ultra-fast rendering (no create/delete per frame)
    HBRUSH m_hBrushBlack;
    HPEN m_hPenBorder;
    
    // Frame rate limiting for smooth 120fps rendering
    DWORD m_lastPaintTime;
};

} // namespace ScreenCapture
