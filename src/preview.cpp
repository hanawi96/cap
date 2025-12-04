#include "preview.h"
#include <shellapi.h>
#include <stdio.h>

namespace ScreenCapture {

const wchar_t* PreviewWindow::WINDOW_CLASS = L"ScreenCapturePreview";
bool PreviewWindow::s_classRegistered = false;

PreviewWindow::PreviewWindow() 
    : m_hwnd(NULL), m_hBitmap(NULL), m_imageWidth(0), m_imageHeight(0) {
}

PreviewWindow::~PreviewWindow() {
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
    }
}

LRESULT CALLBACK PreviewWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewWindow* pThis = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (PreviewWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        // Set a timer to auto-close after 5 seconds
        SetTimer(hwnd, 1, 5000, NULL);
    } else {
        pThis = (PreviewWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (pThis) {
        switch (msg) {
            case WM_PAINT:
                pThis->OnPaint(hwnd);
                return 0;
                
            case WM_CLOSE:
                pThis->OnClose(hwnd);
                return 0;
            
            case WM_TIMER:
                // Auto-close after timer expires
                if (wParam == 1) {
                    KillTimer(hwnd, 1);
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                }
                return 0;
                
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                }
                return 0;
                
            case WM_LBUTTONDBLCLK:
                // Double click to open in default viewer
                ShellExecuteW(NULL, L"open", pThis->m_filename.c_str(), NULL, NULL, SW_SHOW);
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void PreviewWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    if (m_hBitmap) {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int clientWidth = clientRect.right;
        int clientHeight = clientRect.bottom;
        
        // Calculate scaling to fit window
        float scaleX = (float)clientWidth / m_imageWidth;
        float scaleY = (float)clientHeight / m_imageHeight;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        
        int displayWidth = (int)(m_imageWidth * scale);
        int displayHeight = (int)(m_imageHeight * scale);
        int offsetX = (clientWidth - displayWidth) / 2;
        int offsetY = (clientHeight - displayHeight) / 2;
        
        // Draw background (use stock brush - faster)
        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &clientRect, hBrush);
        DeleteObject(hBrush);
        
        // Draw image with high-quality scaling
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, m_hBitmap);
        
        // Use HALFTONE for better quality, SetBrushOrgEx for proper alignment
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        
        StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
                   hdcMem, 0, 0, m_imageWidth, m_imageHeight, SRCCOPY);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        
        // Draw filename at bottom (cache text to avoid string operations)
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect = { 10, clientHeight - 80, clientWidth - 10, clientHeight - 10 };
        
        wchar_t info[512];
        swprintf_s(info, L"Image size: %dx%d pixels\nĐã lưu: %s\n\nNhấn ESC để đóng | Double-click để mở",
                   m_imageWidth, m_imageHeight, m_filename.c_str());
        
        DrawTextW(hdc, info, -1, &textRect, DT_LEFT | DT_WORDBREAK);
    }
    
    EndPaint(hwnd, &ps);
}

void PreviewWindow::OnClose(HWND hwnd) {
    if (m_hwnd) {
        DestroyWindow(hwnd);
        m_hwnd = NULL;
    }
    // Self-delete after window is destroyed
    delete this;
}

void PreviewWindow::Show(HBITMAP hBitmap, const std::wstring& filename) {
    if (!hBitmap) return;
    
    // Register window class if needed
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = WINDOW_CLASS;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.style = CS_DBLCLKS;
        
        if (RegisterClassExW(&wc)) {
            s_classRegistered = true;
        }
    }
    
    // Get image dimensions
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    m_imageWidth = bmp.bmWidth;
    m_imageHeight = bmp.bmHeight;
    m_filename = filename;
    
    // DEBUG: Write to log file
    FILE* f = _wfopen(L"debug_preview.txt", L"a");
    if (f) {
        fwprintf(f, L"=== Preview Window ===\n");
        fwprintf(f, L"Bitmap size: %dx%d\n", m_imageWidth, m_imageHeight);
        fwprintf(f, L"Filename: %s\n\n", filename.c_str());
        fclose(f);
    }
    
    // Fast bitmap clone using CopyImage
    m_hBitmap = (HBITMAP)CopyImage(hBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    
    // Calculate window size (max 80% of screen)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int maxWidth = (int)(screenWidth * 0.8);
    int maxHeight = (int)(screenHeight * 0.8);
    
    int windowWidth = m_imageWidth;
    int windowHeight = m_imageHeight;
    
    if (windowWidth > maxWidth || windowHeight > maxHeight) {
        float scale = (float)maxWidth / windowWidth;
        float scaleY = (float)maxHeight / windowHeight;
        if (scaleY < scale) scale = scaleY;
        
        windowWidth = (int)(windowWidth * scale);
        windowHeight = (int)(windowHeight * scale);
    }
    
    // Center window
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    // Create window
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        WINDOW_CLASS,
        L"Screen Capture - Preview",
        WS_POPUP | WS_VISIBLE,
        x, y, windowWidth, windowHeight,
        NULL, NULL, GetModuleHandle(NULL), this
    );
    
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        SetForegroundWindow(m_hwnd);
    }
}

} // namespace ScreenCapture
