#include "overlay.h"
#include "utils.h"
#include <stdio.h>
#include <algorithm>

using std::min;
using std::max;

namespace ScreenCapture {

static const wchar_t* OVERLAY_CLASS = L"ScreenCaptureOverlay";

// Debug logging helper
static void DebugLog(const wchar_t* format, ...) {
    FILE* f = _wfopen(L"debug_overlay_detail.txt", L"a");
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

Overlay::Overlay() 
    : m_hwnd(NULL)
    , m_isSelecting(false)
    , m_isComplete(false)
    , m_hdcBackbuffer(NULL)
    , m_hbmBackbuffer(NULL)
    , m_hbmOldBackbuffer(NULL)
    , m_backbufferWidth(0)
    , m_backbufferHeight(0)
    , m_hBrushBlack(NULL)
    , m_hPenBorder(NULL)
    , m_lastPaintTime(0) {
    m_selectedRect = {};
    m_startPoint = {};
    m_currentPoint = {};
    m_windowOffset = {};
    DebugLog(L"Overlay constructor called");
}

Overlay::~Overlay() {
    DebugLog(L"Overlay destructor called");
    
    // Cleanup cached GDI objects
    if (m_hBrushBlack) {
        DeleteObject(m_hBrushBlack);
        m_hBrushBlack = NULL;
    }
    if (m_hPenBorder) {
        DeleteObject(m_hPenBorder);
        m_hPenBorder = NULL;
    }
    
    // Cleanup backbuffer
    if (m_hdcBackbuffer) {
        if (m_hbmOldBackbuffer) {
            SelectObject(m_hdcBackbuffer, m_hbmOldBackbuffer);
        }
        if (m_hbmBackbuffer) {
            DeleteObject(m_hbmBackbuffer);
        }
        DeleteDC(m_hdcBackbuffer);
    }
    
    if (m_hwnd) {
        DebugLog(L"  Destroying window in destructor");
        DestroyWindow(m_hwnd);
    }
    
    DebugLog(L"Overlay destructor completed");
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Overlay* overlay = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        overlay = (Overlay*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)overlay);
        DebugLog(L"WndProc: WM_CREATE");
    } else {
        overlay = (Overlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (overlay) {
        switch (msg) {
            case WM_PAINT:
                overlay->OnPaint(hwnd);
                return 0;
            case WM_MOUSEMOVE:
                overlay->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_LBUTTONDOWN:
                DebugLog(L"WndProc: WM_LBUTTONDOWN at (%d,%d)", LOWORD(lParam), HIWORD(lParam));
                overlay->OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_LBUTTONUP:
                DebugLog(L"WndProc: WM_LBUTTONUP at (%d,%d)", LOWORD(lParam), HIWORD(lParam));
                overlay->OnLButtonUp(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_KEYDOWN:
                DebugLog(L"WndProc: WM_KEYDOWN key=%d", wParam);
                overlay->OnKeyDown(wParam);
                return 0;
            case WM_DESTROY:
                DebugLog(L"WndProc: WM_DESTROY");
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Overlay::Show() {
    DebugLog(L"=== Overlay::Show() START ===");
    
    HINSTANCE hInstance = GetModuleHandle(NULL);
    DebugLog(L"  hInstance=%p", hInstance);
    
    // Register window class with CS_OWNDC for better performance
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.lpszClassName = OVERLAY_CLASS;
    wc.style = CS_OWNDC;  // Own DC for better paint performance
    
    ATOM classAtom = RegisterClassExW(&wc);
    DebugLog(L"  RegisterClassExW: atom=%d, error=%d", classAtom, GetLastError());
    
    // Get virtual screen dimensions
    RECT screenRect = GetVirtualScreenRect();
    DebugLog(L"  Virtual screen: L=%d T=%d R=%d B=%d", screenRect.left, screenRect.top, screenRect.right, screenRect.bottom);
    
    // Save window offset for coordinate conversion
    m_windowOffset.x = screenRect.left;
    m_windowOffset.y = screenRect.top;
    
    int width = screenRect.right - screenRect.left;
    int height = screenRect.bottom - screenRect.top;
    DebugLog(L"  Creating window: size=%dx%d", width, height);
    
    // Create layered window with composited flag for double buffering
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
        OVERLAY_CLASS,
        L"",
        WS_POPUP,
        screenRect.left, screenRect.top,
        width, height,
        NULL, NULL, hInstance, this
    );
    
    if (!m_hwnd) {
        DebugLog(L"  ERROR: CreateWindowExW failed, error=%d", GetLastError());
        return false;
    }
    DebugLog(L"  Window created: hwnd=%p", m_hwnd);
    
    // Create persistent backbuffer (once)
    HDC hdcScreen = GetDC(m_hwnd);
    if (!hdcScreen) {
        DebugLog(L"  ERROR: GetDC failed");
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
        return false;
    }
    
    m_backbufferWidth = width;
    m_backbufferHeight = height;
    
    m_hdcBackbuffer = CreateCompatibleDC(hdcScreen);
    m_hbmBackbuffer = CreateCompatibleBitmap(hdcScreen, m_backbufferWidth, m_backbufferHeight);
    m_hbmOldBackbuffer = (HBITMAP)SelectObject(m_hdcBackbuffer, m_hbmBackbuffer);
    
    ReleaseDC(m_hwnd, hdcScreen);
    DebugLog(L"  Backbuffer created: %dx%d", m_backbufferWidth, m_backbufferHeight);
    
    // Cache GDI objects
    m_hBrushBlack = CreateSolidBrush(RGB(0, 0, 0));
    m_hPenBorder = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
    m_lastPaintTime = 0;
    
    // Pre-fill backbuffer with black background using cached brush
    RECT fullRect = { 0, 0, m_backbufferWidth, m_backbufferHeight };
    FillRect(m_hdcBackbuffer, &fullRect, m_hBrushBlack);
    
    // Set transparency
    SetLayeredWindowAttributes(m_hwnd, 0, 128, LWA_ALPHA);
    DebugLog(L"  Transparency set");
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    DebugLog(L"  Window shown, entering message loop...");
    
    // Simple message loop - process ALL messages to avoid issues
    MSG msg;
    int loopCount = 0;
    while (!m_isComplete) {
        loopCount++;
        
        // Use GetMessage for proper blocking (not busy-wait)
        BOOL bRet = GetMessage(&msg, m_hwnd, 0, 0);
        
        if (bRet == 0) {
            // WM_QUIT received
            DebugLog(L"  GetMessage returned 0 (WM_QUIT)");
            break;
        } else if (bRet == -1) {
            // Error
            DebugLog(L"  GetMessage returned -1 (error=%d)", GetLastError());
            break;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Log every 100 iterations
        if (loopCount % 100 == 0) {
            DebugLog(L"  Message loop iteration %d, isComplete=%d", loopCount, m_isComplete);
        }
    }
    
    DebugLog(L"  Message loop exited, loopCount=%d, isComplete=%d", loopCount, m_isComplete);
    DebugLog(L"  Selected rect: L=%d T=%d R=%d B=%d", m_selectedRect.left, m_selectedRect.top, m_selectedRect.right, m_selectedRect.bottom);
    
    // Cleanup
    DebugLog(L"  Destroying overlay window...");
    DestroyWindow(m_hwnd);
    m_hwnd = NULL;
    UnregisterClassW(OVERLAY_CLASS, hInstance);
    
    bool result = m_isComplete && (m_selectedRect.right > m_selectedRect.left);
    DebugLog(L"=== Overlay::Show() END, returning %d ===\n", result);
    
    return result;
}

void Overlay::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    if (!m_hdcBackbuffer || !m_hBrushBlack) {
        EndPaint(hwnd, &ps);
        return;
    }
    
    // Clear entire backbuffer with semi-transparent black (faster than selective clear)
    RECT fullRect = { 0, 0, m_backbufferWidth, m_backbufferHeight };
    FillRect(m_hdcBackbuffer, &fullRect, m_hBrushBlack);
    
    // Draw selection rectangle if selecting
    if (m_isSelecting) {
        int left = min(m_startPoint.x, m_currentPoint.x);
        int top = min(m_startPoint.y, m_currentPoint.y);
        int right = max(m_startPoint.x, m_currentPoint.x);
        int bottom = max(m_startPoint.y, m_currentPoint.y);
        
        // Draw border using fast line drawing (smoother than Rectangle())
        HPEN hOldPen = (HPEN)SelectObject(m_hdcBackbuffer, m_hPenBorder);
        
        // Top line
        MoveToEx(m_hdcBackbuffer, left, top, NULL);
        LineTo(m_hdcBackbuffer, right, top);
        // Right line
        LineTo(m_hdcBackbuffer, right, bottom);
        // Bottom line
        LineTo(m_hdcBackbuffer, left, bottom);
        // Left line
        LineTo(m_hdcBackbuffer, left, top);
        
        SelectObject(m_hdcBackbuffer, hOldPen);
        
        // Draw size info (only if there's space)
        if (top > 30) {
            wchar_t info[64];
            swprintf_s(info, L"%dx%d", right - left, bottom - top);
            
            // Draw text with transparent background (no need to clear, already black)
            SetBkMode(m_hdcBackbuffer, TRANSPARENT);
            SetTextColor(m_hdcBackbuffer, RGB(255, 255, 0));
            TextOutW(m_hdcBackbuffer, left, top - 20, info, (int)wcslen(info));
        }
    }
    
    // Fast BitBlt from backbuffer to screen
    BitBlt(hdc, 0, 0, m_backbufferWidth, m_backbufferHeight, m_hdcBackbuffer, 0, 0, SRCCOPY);
    
    EndPaint(hwnd, &ps);
}

void Overlay::OnMouseMove(int x, int y) {
    if (!m_isSelecting) return;
    
    // Frame rate limiting
    DWORD now = GetTickCount();
    if (now - m_lastPaintTime < 16) {
        return;
    }
    m_lastPaintTime = now;
    
    // Calculate old rectangle for invalidation
    RECT oldRect = {
        min(m_startPoint.x, m_currentPoint.x),
        min(m_startPoint.y, m_currentPoint.y),
        max(m_startPoint.x, m_currentPoint.x),
        max(m_startPoint.y, m_currentPoint.y)
    };
    
    m_currentPoint.x = x;
    m_currentPoint.y = y;
    
    RECT newRect = {
        min(m_startPoint.x, m_currentPoint.x),
        min(m_startPoint.y, m_currentPoint.y),
        max(m_startPoint.x, m_currentPoint.x),
        max(m_startPoint.y, m_currentPoint.y)
    };
    
    // Merge rectangles into single invalidate call
    RECT unionRect;
    UnionRect(&unionRect, &oldRect, &newRect);
    InflateRect(&unionRect, 5, 25);
    InvalidateRect(m_hwnd, &unionRect, FALSE);
}

void Overlay::OnLButtonDown(int x, int y) {
    DebugLog(L"OnLButtonDown: (%d,%d)", x, y);
    m_startPoint.x = x;
    m_startPoint.y = y;
    m_currentPoint = m_startPoint;
    m_isSelecting = true;
    m_lastPaintTime = 0;
    SetCapture(m_hwnd);
    DebugLog(L"  Capture set, isSelecting=true");
}

void Overlay::OnLButtonUp(int x, int y) {
    DebugLog(L"OnLButtonUp: (%d,%d)", x, y);
    
    if (m_isSelecting) {
        ReleaseCapture();
        m_isSelecting = false;
        
        // Convert client coordinates to screen coordinates using saved offset
        int screenStartX = m_startPoint.x + m_windowOffset.x;
        int screenStartY = m_startPoint.y + m_windowOffset.y;
        int screenEndX = x + m_windowOffset.x;
        int screenEndY = y + m_windowOffset.y;
        
        // Calculate selected region
        int left = min(screenStartX, screenEndX);
        int top = min(screenStartY, screenEndY);
        int right = max(screenStartX, screenEndX);
        int bottom = max(screenStartY, screenEndY);
        
        m_selectedRect = { left, top, right, bottom };
        m_isComplete = true;
        
        DebugLog(L"  Selection complete: L=%d T=%d R=%d B=%d", left, top, right, bottom);
        DebugLog(L"  m_isComplete set to true");
        
        // Post a dummy message to wake up GetMessage
        PostMessage(m_hwnd, WM_NULL, 0, 0);
    }
}

void Overlay::OnKeyDown(WPARAM key) {
    DebugLog(L"OnKeyDown: key=%d", key);
    if (key == VK_ESCAPE) {
        DebugLog(L"  ESC pressed, cancelling");
        m_isComplete = true;
        m_selectedRect = { 0, 0, 0, 0 };
        PostMessage(m_hwnd, WM_NULL, 0, 0);
    }
}

} // namespace ScreenCapture
