#pragma once

#include <windows.h>

HWND InitDOSWindow();
int ProcessWindowEvents(HWND hwnd);
void SetVGAPalette(int index, char r, char g, char b, HBITMAP hBitmap);
int WinReadTimer();

extern HBITMAP BackingBitmap;