#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winternl.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "wtsapi32.lib")

#define banana(name) do { \
  printf("%s Error: %d\n", (name), GetLastError()); \
  return EXIT_FAILURE; \
} while(0)

// Save HBITMAP -> BMP file
int save(HBITMAP hBitmap, HDC hDC, const wchar_t *filename) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(bmp), &bmp);

    BITMAPFILEHEADER bfh = {0};
    BITMAPINFOHEADER bih = {0};

    // Fill BITMAPINFOHEADER
    bih.biSize        = sizeof(bih);
    bih.biWidth       = bmp.bmWidth;
    bih.biHeight      = -bmp.bmHeight; // top-down
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    DWORD imageSize = bmp.bmWidth * 4 * bmp.bmHeight;
    void *bits = malloc(imageSize);
    if (!bits) return -1;

    // Copy pixels from HBITMAP into our buffer
    GetDIBits(hDC, hBitmap, 0, bmp.bmHeight, bits, (BITMAPINFO*)&bih, DIB_RGB_COLORS);

    // Fill BITMAPFILEHEADER
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    HANDLE file = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return -1;

    DWORD written;
    if (!WriteFile(file, &bfh, sizeof(bfh), &written, NULL)) return -1;
    if (!WriteFile(file, &bih, sizeof(bih), &written, NULL)) return -1;
    if (!WriteFile(file, bits, imageSize, &written, NULL)) return -1;

    CloseHandle(file);
    free(bits);
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        printf("Usage: %S <output_filename.bmp>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Screenshot will be saved as: %S\n", argv[1]);

    // 1. Attach to "WinSta0" (the interactive window station)
    HWINSTA hWinSta = OpenWindowStationA("WinSta0", FALSE, WINSTA_ALL_ACCESS);
    if (!SetProcessWindowStation(hWinSta)) {
        banana("SetProcessWindowStation");
    }

    // 2. Attach to the input desktop (where user sees stuff)
    HDESK hDesk = OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
    if (!SetThreadDesktop(hDesk)) {
        banana("SetThreadDesktop");
    }

    // 3. Get the desktop window handle
    HWND hDesktopWnd = GetDesktopWindow();
    if (!hDesktopWnd) banana("GetDesktopWindow");

    // 4. Get DC of the desktop
    HDC hDesktopDC = GetDC(hDesktopWnd);
    if (!hDesktopDC) banana("GetDC");

    // 5. Create a memory DC compatible with desktop DC
    HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
    if (!hCaptureDC) banana("CreateCompatibleDC");

    // 6. Get virtual screen size (supports multi-monitor)
    int width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x      = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y      = GetSystemMetrics(SM_YVIRTUALSCREEN);

    // 7. Create a bitmap compatible with desktop DC
    HBITMAP hBitmap = CreateCompatibleBitmap(hDesktopDC, width, height);
    if (!hBitmap) banana("CreateCompatibleBitmap");

    SelectObject(hCaptureDC, hBitmap);

    // 8. Copy screen pixels into the memory DC
    if (!StretchBlt(hCaptureDC, 0, 0, width, height,
                    hDesktopDC, x, y, width, height, SRCCOPY)) {
        banana("StretchBlt");
    }

    // 9. Save the bitmap to file
    if (save(hBitmap, hCaptureDC, argv[1]) != 0) {
        banana("save");
    }

    // 10. Cleanup
    ReleaseDC(hDesktopWnd, hDesktopDC);
    DeleteDC(hCaptureDC);
    DeleteObject(hBitmap);
    CloseWindowStation(hWinSta);
    CloseDesktop(hDesk);

    printf("Saved screenshot to %S\n", argv[1]);
    return EXIT_SUCCESS;
}
