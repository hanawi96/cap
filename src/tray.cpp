#include "tray.h"
#include "capture.h"
#include "overlay.h"
#include "utils.h"
#include <shellapi.h>

namespace ScreenCapture {

TrayIcon::TrayIcon() : m_created(false) {
    ZeroMemory(&m_nid, sizeof(m_nid));
}

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(HWND hwnd) {
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    
    // Load icon (use default application icon for now)
    m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    wcscpy_s(m_nid.szTip, L"Screen Capture");
    
    m_created = Shell_NotifyIconW(NIM_ADD, &m_nid);
    return m_created;
}

void TrayIcon::Destroy() {
    if (m_created) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_created = false;
    }
}

void TrayIcon::ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, MENU_CAPTURE_FULLSCREEN, L"Chụp toàn màn hình");
    AppendMenuW(hMenu, MF_STRING, MENU_CAPTURE_WINDOW, L"Chụp cửa sổ");
    AppendMenuW(hMenu, MF_STRING, MENU_CAPTURE_REGION, L"Chụp vùng chọn");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_OPEN_FOLDER, L"Mở thư mục ảnh");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_EXIT, L"Thoát");
    
    SetForegroundWindow(hwnd);
    
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, hwnd, NULL);
    
    DestroyMenu(hMenu);
}

void TrayIcon::ShowNotification(const wchar_t* title, const wchar_t* message) {
    if (!m_created) return;
    
    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, message);
    
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

} // namespace ScreenCapture
