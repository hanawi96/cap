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
    , m_hdcScreenshot(NULL)
    , m_hbmScreenshot(NULL)
    , m_hbmOldScreenshot(NULL)
    , m_hdcBackbuffer(NULL)
    , m_hbmBackbuffer(NULL)
    , m_hbmOldBackbuffer(NULL)
    , m_backbufferWidth(0)
    , m_backbufferHeight(0)
    , m_hPenBorder(NULL)
    , m_hBrushDim(NULL)
    , m_lastPaintTime(0) {
    m_selectedRect = {};
    m_startPoint = {};
    m_currentPoint = {};
    m_windowOffset = {};
    m_lastRect = {};
    m_prevRect = {};
    DebugLog(L"Overlay constructor called");
}

Overlay::~Overlay() {
    DebugLog(L"Overlay destructor called");
    
    // Cleanup cached GDI objects
    if (m_hBrushDim) {
        DeleteObject(m_hBrushDim);
        m_hBrushDim = NULL;
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
    
    // Cleanup screenshot buffer
    if (m_hdcScreenshot) {
        if (m_hbmOldScreenshot) {
            SelectObject(m_hdcScreenshot, m_hbmOldScreenshot);
        }
        if (m_hbmScreenshot) {
            DeleteObject(m_hbmScreenshot);
        }
        DeleteDC(m_hdcScreenshot);
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
    
    // Capture screenshot BEFORE showing overlay
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        DebugLog(L"  ERROR: GetDC(NULL) failed");
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
        return false;
    }
    
    m_backbufferWidth = width;
    m_backbufferHeight = height;
    
    // Create screenshot buffer and capture current screen
    m_hdcScreenshot = CreateCompatibleDC(hdcScreen);
    m_hbmScreenshot = CreateCompatibleBitmap(hdcScreen, width, height);
    m_hbmOldScreenshot = (HBITMAP)SelectObject(m_hdcScreenshot, m_hbmScreenshot);
    
    // Capture entire screen to screenshot buffer
    DWORD captureStart = GetTickCount();
    BitBlt(m_hdcScreenshot, 0, 0, width, height, hdcScreen, screenRect.left, screenRect.top, SRCCOPY | CAPTUREBLT);
    DWORD captureTime = GetTickCount() - captureStart;
    DebugLog(L"  Screenshot captured: %dx%d in %dms", width, height, captureTime);
    
    FILE* f = _wfopen(L"debug_overlay.txt", L"a");
    if (f) {
        fwprintf(f, L"\n=== NEW OVERLAY SESSION ===\n");
        fwprintf(f, L"Screen size: %dx%d\n", width, height);
        fwprintf(f, L"Screenshot capture time: %dms\n", captureTime);
        fwprintf(f, L"Using: Direct rendering + Screenshot restore + Selective redraw\n");
        fwprintf(f, L"Target FPS: 120 (8ms frame time)\n\n");
        fclose(f);
    }
    
    // Create drawing backbuffer
    m_hdcBackbuffer = CreateCompatibleDC(hdcScreen);
    m_hbmBackbuffer = CreateCompatibleBitmap(hdcScreen, width, height);
    m_hbmOldBackbuffer = (HBITMAP)SelectObject(m_hdcBackbuffer, m_hbmBackbuffer);
    
    ReleaseDC(NULL, hdcScreen);
    DebugLog(L"  Backbuffer created: %dx%d", width, height);
    
    // Cache GDI objects
    m_hPenBorder = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
    m_hBrushDim = CreateSolidBrush(RGB(0, 0, 0));  // For dimming effect
    m_lastPaintTime = 0;
    m_lastRect = {};
    m_prevRect = {};
    
    // Initial draw: copy screenshot with dim overlay
    BitBlt(m_hdcBackbuffer, 0, 0, width, height, m_hdcScreenshot, 0, 0, SRCCOPY);
    
    // Set transparency
    SetLayeredWindowAttributes(m_hwnd, 0, 200, LWA_ALPHA);  // Slightly less transparent
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
    
    DWORD paintStartTime = GetTickCount();
    
    if (!m_hdcBackbuffer || !m_hdcScreenshot) {
        EndPaint(hwnd, &ps);
        return;
    }
    
    FILE* f = _wfopen(L"debug_overlay.txt", L"a");
    if (f) {
        fwprintf(f, L"[OnPaint] Called - InvalidRect: L=%d T=%d R=%d B=%d\n",
            ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
        fclose(f);
    }
    
    // Restore old rect area from screenshot (erase old border)
    if (m_lastRect.right > 0) {
        RECT restoreRect = m_lastRect;
        InflateRect(&restoreRect, 3, 30);  // Include border width and text
        BitBlt(m_hdcBackbuffer, 
               restoreRect.left, restoreRect.top,
               restoreRect.right - restoreRect.left, 
               restoreRect.bottom - restoreRect.top,
               m_hdcScreenshot, 
               restoreRect.left, restoreRect.top, 
               SRCCOPY);
    }
    
    // Draw new selection rectangle
    if (m_isSelecting) {
        int left = min(m_startPoint.x, m_currentPoint.x);
        int top = min(m_startPoint.y, m_currentPoint.y);
        int right = max(m_startPoint.x, m_currentPoint.x);
        int bottom = max(m_startPoint.y, m_currentPoint.y);
        
        m_lastRect = { left, top, right, bottom };
        
        // Draw border
        HPEN hOldPen = (HPEN)SelectObject(m_hdcBackbuffer, m_hPenBorder);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(m_hdcBackbuffer, GetStockObject(NULL_BRUSH));
        
        Rectangle(m_hdcBackbuffer, left, top, right, bottom);
        
        SelectObject(m_hdcBackbuffer, hOldPen);
        SelectObject(m_hdcBackbuffer, hOldBrush);
        
        // Draw size info
        if (top > 30) {
            wchar_t info[64];
            swprintf_s(info, L"%dx%d", right - left, bottom - top);
            
            SetBkMode(m_hdcBackbuffer, OPAQUE);
            SetBkColor(m_hdcBackbuffer, RGB(0, 0, 0));
            SetTextColor(m_hdcBackbuffer, RGB(255, 255, 0));
            TextOutW(m_hdcBackbuffer, left, top - 20, info, (int)wcslen(info));
        }
    }
    
    // BitBlt only the invalidated region (partial redraw)
    BitBlt(hdc, 
           ps.rcPaint.left, ps.rcPaint.top,
           ps.rcPaint.right - ps.rcPaint.left, 
           ps.rcPaint.bottom - ps.rcPaint.top,
           m_hdcBackbuffer, 
           ps.rcPaint.left, ps.rcPaint.top,
           SRCCOPY);
    
    DWORD paintTime = GetTickCount() - paintStartTime;
    
    f = _wfopen(L"debug_overlay.txt", L"a");
    if (f) {
        fwprintf(f, L"[OnPaint] Completed in %dms\n", paintTime);
        fclose(f);
    }
    
    EndPaint(hwnd, &ps);
}

void Overlay::OnMouseMove(int x, int y) {
    static int mouseMoveCount = 0;
    mouseMoveCount++;
    
    if (!m_isSelecting) return;
    
    // Ultra-fast frame limiting (8ms = 120fps)
    DWORD now = GetTickCount();
    DWORD timeSinceLastFrame = now - m_lastPaintTime;
    if (timeSinceLastFrame < 8) {
        // Log throttled frames
        if (mouseMoveCount % 50 == 0) {
            FILE* f = _wfopen(L"debug_overlay.txt", L"a");
            if (f) {
                fwprintf(f, L"[OnMouseMove] Throttled - only %dms since last frame\n", timeSinceLastFrame);
                fclose(f);
            }
        }
        return;
    }
    
    DWORD startTime = now;
    m_lastPaintTime = now;
    
    // Log that we're doing direct rendering
    if (mouseMoveCount % 10 == 0) {
        FILE* f = _wfopen(L"debug_overlay.txt", L"a");
        if (f) {
            fwprintf(f, L"[OnMouseMove] DIRECT RENDER at (%d,%d) - count=%d\n", x, y, mouseMoveCount);
            fclose(f);
        }
    }
    
    // Direct rendering (bypass WM_PAINT message queue for instant response)
    if (!m_hdcBackbuffer || !m_hdcScreenshot) return;
    
    // Save previous rect before updating
    m_prevRect = m_lastRect;
    
    // Update current point
    m_currentPoint.x = x;
    m_currentPoint.y = y;
    
    // Calculate new selection rect
    int left = min(m_startPoint.x, m_currentPoint.x);
    int top = min(m_startPoint.y, m_currentPoint.y);
    int right = max(m_startPoint.x, m_currentPoint.x);
    int bottom = max(m_startPoint.y, m_currentPoint.y);
    
    m_lastRect = { left, top, right, bottom };
    
    // Restore old rect from screenshot (erase old border)
    if (m_prevRect.right > 0) {
        RECT restoreRect = m_prevRect;
        InflateRect(&restoreRect, 3, 30);
        BitBlt(m_hdcBackbuffer, 
               restoreRect.left, restoreRect.top,
               restoreRect.right - restoreRect.left, 
               restoreRect.bottom - restoreRect.top,
               m_hdcScreenshot, 
               restoreRect.left, restoreRect.top, 
               SRCCOPY);
    }
    
    // Draw border
    HPEN hOldPen = (HPEN)SelectObject(m_hdcBackbuffer, m_hPenBorder);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(m_hdcBackbuffer, GetStockObject(NULL_BRUSH));
    Rectangle(m_hdcBackbuffer, left, top, right, bottom);
    SelectObject(m_hdcBackbuffer, hOldPen);
    SelectObject(m_hdcBackbuffer, hOldBrush);
    
    // Draw size info
    if (top > 30) {
        wchar_t info[64];
        swprintf_s(info, L"%dx%d", right - left, bottom - top);
        SetBkMode(m_hdcBackbuffer, OPAQUE);
        SetBkColor(m_hdcBackbuffer, RGB(0, 0, 0));
        SetTextColor(m_hdcBackbuffer, RGB(255, 255, 0));
        TextOutW(m_hdcBackbuffer, left, top - 20, info, (int)wcslen(info));
    }
    
    // Direct BitBlt to screen (no message queue, instant update)
    HDC hdcWindow = GetDC(m_hwnd);
    if (hdcWindow) {
        // Calculate update region (old + new rect)
        RECT updateRect = m_lastRect;
        InflateRect(&updateRect, 3, 30);
        
        // Union with previous rect area to ensure old border is cleared
        if (m_prevRect.right > 0) {
            RECT oldRestoreRect = m_prevRect;
            InflateRect(&oldRestoreRect, 3, 30);
            UnionRect(&updateRect, &updateRect, &oldRestoreRect);
        }
        
        int updateWidth = updateRect.right - updateRect.left;
        int updateHeight = updateRect.bottom - updateRect.top;
        
        // BitBlt only the changed region
        BitBlt(hdcWindow, 
               updateRect.left, updateRect.top,
               updateWidth, updateHeight,
               m_hdcBackbuffer, 
               updateRect.left, updateRect.top,
               SRCCOPY);
        
        ReleaseDC(m_hwnd, hdcWindow);
        
        // Calculate total render time
        DWORD renderTime = GetTickCount() - startTime;
        
        // Log performance metrics (every 10 frames to avoid spam)
        static int frameCount = 0;
        static DWORD totalRenderTime = 0;
        static DWORD minRenderTime = 999999;
        static DWORD maxRenderTime = 0;
        
        frameCount++;
        totalRenderTime += renderTime;
        if (renderTime < minRenderTime) minRenderTime = renderTime;
        if (renderTime > maxRenderTime) maxRenderTime = renderTime;
        
        if (frameCount % 10 == 0) {
            DWORD avgRenderTime = totalRenderTime / frameCount;
            int fps = (renderTime > 0) ? (1000 / renderTime) : 999;
            
            FILE* f = _wfopen(L"debug_overlay.txt", L"a");
            if (f) {
                fwprintf(f, L"[PERF] Frame %d: Render=%dms (avg=%dms, min=%dms, max=%dms) FPS=%d UpdateArea=%dx%d TimeSinceLastFrame=%dms\n",
                    frameCount, renderTime, avgRenderTime, minRenderTime, maxRenderTime, fps, updateWidth, updateHeight, timeSinceLastFrame);
                fclose(f);
            }
        }
    }
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
