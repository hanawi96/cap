# Screen Capture - Ứng dụng chụp màn hình chuyên nghiệp

Ứng dụng chụp màn hình siêu nhẹ, tốc độ cực nhanh được viết bằng C++ và Win32 API thuần túy.

## Tính năng

### ✅ Phase 1 - Đã hoàn thành

- **Chụp toàn màn hình**: Phím `PrintScreen`
- **Chụp cửa sổ active**: Phím `Ctrl + PrintScreen`
- **Chụp vùng tùy chọn**: Phím `Shift + PrintScreen`
- **System Tray Icon**: Chạy nền, menu chuột phải
- **Lưu file PNG**: Tự động lưu vào `Pictures\ScreenCapture\`
- **Hỗ trợ đa màn hình**: Tự động nhận diện virtual screen

## Phím tắt

| Phím tắt | Chức năng |
|----------|-----------|
| `PrintScreen` | Chụp toàn màn hình |
| `Ctrl + PrintScreen` | Chụp cửa sổ đang active |
| `Shift + PrintScreen` | Chụp vùng chọn (click-drag) |
| `ESC` | Hủy chọn vùng |

## Hiệu năng

- **Kích thước**: < 200KB (Release build)
- **Tốc độ chụp**: < 20ms
- **CPU khi idle**: < 0.1%
- **RAM**: ~5MB

## Công nghệ

- **Ngôn ngữ**: C++17
- **API**: Win32 API thuần túy
- **Capture**: BitBlt + GDI
- **Overlay**: Layered Window
- **Image**: stb_image_write (PNG)
- **Build**: MSVC / Visual Studio

## Cài đặt

1. Tải file `ScreenCapture.exe` từ thư mục `build\Release\`
2. Chạy file exe
3. Icon sẽ xuất hiện trong system tray
4. Sử dụng phím tắt hoặc menu chuột phải

## Build từ source

Xem file [BUILD.md](BUILD.md) để biết chi tiết.

## Cấu trúc project

```
ScreenCapture/
├── src/
│   ├── main.cpp        # Entry point, message loop
│   ├── capture.cpp/h   # Screen capture logic
│   ├── hotkeys.cpp/h   # Global hotkey handling
│   ├── overlay.cpp/h   # Region selection overlay
│   ├── tray.cpp/h      # System tray icon
│   └── utils.cpp/h     # Utilities (save, timestamp)
├── stb_image_write.h   # PNG writer library
├── ScreenCapture.sln   # Visual Studio solution
└── ScreenCapture.vcxproj
```

## Roadmap

### Phase 2 (Tương lai)
- [ ] DXGI Desktop Duplication (capture nhanh hơn)
- [ ] Chụp video màn hình
- [ ] Editor đơn giản (crop, arrow, text)
- [ ] Upload cloud (Imgur, Google Drive)
- [ ] OCR text từ ảnh

## License

MIT License - Tự do sử dụng và chỉnh sửa
