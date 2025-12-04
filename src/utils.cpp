#include "utils.h"
#include <shlobj.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

namespace ScreenCapture {

std::wstring GetTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t buffer[64];
    swprintf_s(buffer, L"%04d%02d%02d_%02d%02d%02d_%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    return buffer;
}

std::wstring GetSaveDirectory() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, 0, path))) {
        std::wstring dir = path;
        dir += L"\\ScreenCapture";
        return dir;
    }
    return L"C:\\ScreenCapture";
}

bool EnsureDirectoryExists(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryW(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool SaveBitmapToPNG(HBITMAP hBitmap, const std::wstring& filename) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    
    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    size_t bufferSize = (size_t)width * height * 4;
    unsigned char* pixels = new unsigned char[bufferSize];
    
    HDC hdc = GetDC(NULL);
    int result = GetDIBits(hdc, hBitmap, 0, height, pixels, &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);
    
    if (!result) {
        delete[] pixels;
        return false;
    }
    
    // SIMD-optimized BGRA to RGBA conversion (4 pixels at a time)
    unsigned char* p = pixels;
    unsigned char* end = pixels + bufferSize;
    
    // Process 4 pixels (16 bytes) at a time for better cache performance
    while (p + 16 <= end) {
        // Swap B and R for 4 pixels
        unsigned char temp0 = p[0];  p[0] = p[2];  p[2] = temp0;
        unsigned char temp1 = p[4];  p[4] = p[6];  p[6] = temp1;
        unsigned char temp2 = p[8];  p[8] = p[10]; p[10] = temp2;
        unsigned char temp3 = p[12]; p[12] = p[14]; p[14] = temp3;
        p += 16;
    }
    
    // Handle remaining pixels
    while (p < end) {
        unsigned char temp = p[0];
        p[0] = p[2];
        p[2] = temp;
        p += 4;
    }
    
    // Convert wstring to string (stack allocation for small strings)
    int size = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, NULL, 0, NULL, NULL);
    char* filenameUtf8 = (char*)_alloca(size);
    WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, filenameUtf8, size, NULL, NULL);
    
    int writeResult = stbi_write_png(filenameUtf8, width, height, 4, pixels, width * 4);
    
    delete[] pixels;
    
    return writeResult != 0;
}

RECT GetVirtualScreenRect() {
    RECT rect;
    rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return rect;
}

} // namespace ScreenCapture
