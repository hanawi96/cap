#pragma once
#include <windows.h>
#include <string>

namespace ScreenCapture {

// Get timestamp string for filename
std::wstring GetTimestamp();

// Get save directory path
std::wstring GetSaveDirectory();

// Ensure directory exists
bool EnsureDirectoryExists(const std::wstring& path);

// Save bitmap to PNG file
bool SaveBitmapToPNG(HBITMAP hBitmap, const std::wstring& filename);

// Get monitor info for multi-monitor support
RECT GetVirtualScreenRect();

} // namespace ScreenCapture
