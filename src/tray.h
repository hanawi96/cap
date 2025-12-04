#pragma once
#include <windows.h>

namespace ScreenCapture {

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();
    
    bool Create(HWND hwnd);
    void Destroy();
    
    void ShowContextMenu(HWND hwnd);
    void ShowNotification(const wchar_t* title, const wchar_t* message);
    
    static const UINT WM_TRAYICON = WM_USER + 1;
    
    enum MenuID {
        MENU_CAPTURE_FULLSCREEN = 1001,
        MENU_CAPTURE_WINDOW = 1002,
        MENU_CAPTURE_REGION = 1003,
        MENU_OPEN_FOLDER = 1004,
        MENU_EXIT = 1005
    };
    
private:
    NOTIFYICONDATAW m_nid;
    bool m_created;
};

} // namespace ScreenCapture
