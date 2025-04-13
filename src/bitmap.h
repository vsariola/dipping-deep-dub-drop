#ifndef __BITMAP_H
#define __BITMAP_H

#include <windows.h>

#ifndef CAPTURE_FRAME_RATE
#define CAPTURE_FRAME_RATE 60
#endif

long SaveFrame(HDC hDC, HWND window);
PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp);
void CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC);

#endif