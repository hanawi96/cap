#pragma once
#include <windows.h>
#include <string>

namespace ScreenCapture {

class PreviewWindow {
public:
    PreviewWindow();
    ~PreviewWindow();
    
    // Show preview of captured image
    void Show(HBITMAP hBitmap, const std::wstring& filename);
    
private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HWND hwnd);
    void OnClose(HWND hwnd);
    
    HWND m_hwnd;
    HBITMAP m_hBitmap;
    std::wstring m_filename;
    int m_imageWidth;
    int m_imageHeight;
    
    static const wchar_t* WINDOW_CLASS;
    static bool s_classRegistered;
};

} // namespace ScreenCapture
