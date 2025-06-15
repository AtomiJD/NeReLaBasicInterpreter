#include "Graphics.hpp"
#include <windows.h> // Required for all GDI and console functions
#include <cstring>
#include <cstdio>
#include <tchar.h>
#include <conio.h>
#include <strsafe.h>

namespace {
    // We'll store the console handle and device context as static variables
    // so we don't have to retrieve them for every single drawing operation.
    HWND console_window = nullptr;
    HDC device_context = nullptr;

    // A classic 16-color palette, similar to QBasic.
    // This array maps a simple color index (0-15) to a GDI COLORREF value.
    COLORREF palette[16] = {
        RGB(0, 0, 0),       // 0: Black
        RGB(0, 0, 128),     // 1: Blue
        RGB(0, 128, 0),     // 2: Green
        RGB(0, 128, 128),   // 3: Cyan
        RGB(128, 0, 0),     // 4: Red
        RGB(128, 0, 128),   // 5: Magenta
        RGB(128, 128, 0),   // 6: Brown
        RGB(192, 192, 192), // 7: Light Gray
        RGB(128, 128, 128), // 8: Dark Gray
        RGB(0, 0, 255),     // 9: Light Blue
        RGB(0, 255, 0),     // 10: Light Green
        RGB(0, 255, 255),   // 11: Light Cyan
        RGB(255, 0, 0),     // 12: Light Red
        RGB(255, 0, 255),   // 13: Light Magenta
        RGB(255, 255, 0),   // 14: Yellow
        RGB(255, 255, 255)  // 15: White
    };
}


void Graphics::init() {
    console_window = GetConsoleWindow();
}

void Graphics::pset(int x, int y, int color_index) {
    if (console_window == nullptr || color_index < 0 || color_index > 15) {
        return;
    }
    // Get the device context (the "canvas") right before drawing.
    HDC device_context = GetDC(console_window);
    if (device_context == nullptr) return;

    // GDI function to set the color of a single pixel.
    SetPixel(device_context, x, y, palette[color_index]);

    // IMPORTANT: Release the device context immediately after drawing.
    ReleaseDC(console_window, device_context);
}

void Graphics::line(int x1, int y1, int x2, int y2, int color_index) {
    if (console_window == nullptr || color_index < 0 || color_index > 15) {
        return;
    }
    HDC device_context = GetDC(console_window);
    if (device_context == nullptr) return;

    HPEN pen = CreatePen(PS_SOLID, 1, palette[color_index]);
    HPEN old_pen = (HPEN)SelectObject(device_context, pen);

    MoveToEx(device_context, x1, y1, NULL);
    LineTo(device_context, x2, y2);

    SelectObject(device_context, old_pen);
    DeleteObject(pen);

    ReleaseDC(console_window, device_context);
}

void Graphics::circle(int x, int y, int radius, int color_index) {
    if (console_window == nullptr || color_index < 0 || color_index > 15) {
        return;
    }
    HDC device_context = GetDC(console_window);
    if (device_context == nullptr) return;

    HPEN pen = CreatePen(PS_SOLID, 1, palette[color_index]);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN old_pen = (HPEN)SelectObject(device_context, pen);
    HBRUSH old_brush = (HBRUSH)SelectObject(device_context, brush);

    int left = x - radius;
    int top = y - radius;
    int right = x + radius;
    int bottom = y + radius;
    Ellipse(device_context, left, top, right, bottom);

    SelectObject(device_context, old_pen);
    SelectObject(device_context, old_brush);
    DeleteObject(pen);

    ReleaseDC(console_window, device_context);
}