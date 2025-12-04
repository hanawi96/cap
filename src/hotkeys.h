#pragma once
#include <windows.h>

namespace ScreenCapture {

enum HotkeyID {
    HOTKEY_FULLSCREEN = 1,
    HOTKEY_WINDOW = 2,
    HOTKEY_REGION = 3
};

// Register all hotkeys
bool RegisterHotkeys(HWND hwnd);

// Unregister all hotkeys
void UnregisterHotkeys(HWND hwnd);

// Handle hotkey message
void HandleHotkey(int hotkeyId);

} // namespace ScreenCapture
