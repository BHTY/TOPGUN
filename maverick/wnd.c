#include <windows.h>
#include "../common/i386.h"

BITMAPINFO* bmi;
HBITMAP BackingBitmap;
char* pBits;

extern i386* Pcpu;
extern char* Os2Base;

PVOID(WINAPI* pAddVectoredExceptionHandler)(ULONG, PVOID) = NULL;

DWORD TickCountStart;

DWORD WINAPI CallbackExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo){
	if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION){
		printf("Acesss Violation accessing address %p (Os2Base= %p)\n", pExceptionInfo->ExceptionRecord->ExceptionInformation[1], Os2Base);
		dump_386(Pcpu);
	}
	
	return EXCEPTION_CONTINUE_SEARCH;
}

void DosPresent(HDC hdc){ //converts VRAM contents into visible HBITMAP, then blits
	HDC hdcMem = CreateCompatibleDC(hdc);
	
	memcpy(pBits, Os2Base + 0xA0000, 64000);
	
	SelectObject(hdcMem, BackingBitmap);
	
	StretchBlt(hdc, 0, 0, 640, 400, hdcMem, 0, 0, 320, 200, SRCCOPY);
	
	DeleteDC(hdcMem);
}

int WinReadTimer(){
	return GetTickCount();
}

int CtrlDown(){
	if(GetKeyState(VK_CONTROL) & 0x8000) return 1;
	return 0;
}

int AltDown(){
	if(GetKeyState(VK_MENU) & 0x8000) return 1;
	return 0;
}

int ShiftDown(){
	if(GetKeyState(VK_SHIFT) & 0x8000) return 1;
	return 0;
}

LRESULT CALLBACK DosWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
	PAINTSTRUCT ps;
	HDC hdc;
	
	switch(msg){
		DWORD tc;
		
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_QUIT:
			break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_TIMER:
			if(wParam == 1){
				InvalidateRect(hwnd, NULL, 0);
			}
			if(wParam == 2){
				tc = GetTickCount() - TickCountStart;
				//printf("%d instructions per second (%d instructions / %d ms)\n", Pcpu->steps_run * 1000 / tc, Pcpu->steps_run, tc);
			}
			break;
		case WM_PAINT:
			hdc = BeginPaint(hwnd, &ps);
			DosPresent(hdc);
			EndPaint(hwnd, &ps);
			break;
		default:
			return DefWindowProcA(hwnd, msg, wParam, lParam);
	}

	//return 0;
}

HBITMAP CreateBackingDIB(PVOID* ppvBits){
	HBITMAP hBitmap;
	HDC hdcScreen;
	
	bmi = malloc(sizeof(BITMAPINFO)+256 * sizeof(RGBQUAD));
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = 320;
	bmi->bmiHeader.biHeight = -200;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biClrUsed = 256;
	bmi->bmiHeader.biCompression = BI_RGB;
	
	hdcScreen = GetDC(NULL);
	hBitmap = CreateDIBSection(hdcScreen, bmi, DIB_RGB_COLORS, ppvBits, NULL, NULL);
	ReleaseDC(NULL, hdcScreen);
	
	return hBitmap;
}

HWND CreateDOSWindow(){
	WNDCLASSA wc;
	HWND hwnd;
	RECT winRect;
	
	winRect.left = 0;
	winRect.top = 0;
	winRect.bottom = 400;
	winRect.right = 640;
	
	AdjustWindowRect(&winRect, WS_OVERLAPPEDWINDOW, FALSE);
	
	wc.style = 0;
	wc.lpfnWndProc = DosWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1)); //101
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "Locutus v0.1";
	RegisterClass(&wc);
	
	hwnd = CreateWindow("Locutus v0.1", "Locutus v0.1", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, winRect.right - winRect.left, winRect.bottom - winRect.top, NULL, NULL, GetModuleHandle(NULL), NULL);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	
	return hwnd;
}

void SetVGAPalette(int index, char r, char g, char b, HBITMAP hBitmap){
	RGBQUAD col;
	RGBQUAD *willPalette = &(bmi->bmiColors[0]);

	HDC tempHDC = CreateCompatibleDC(NULL);
	
	SelectObject(tempHDC, hBitmap);
	col.rgbRed = r;
	col.rgbGreen = g;
	col.rgbBlue = b;
	willPalette[index] = col;
	SetDIBColorTable(tempHDC, index, 1, willPalette + index);
	DeleteDC(tempHDC);
}

int ProcessWindowEvents(HWND hwnd){
	MSG Msg;
	
	//printf("Processing window events\n");

	while(PeekMessage(&Msg, hwnd, 0, 0, PM_REMOVE)){
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
	}
	
	if(Msg.message == WM_QUIT) return 1;
	return 0;
}

DWORD WINAPI DosWindowThreadOld(HWND hDosWindow){
	while(1){
		if(ProcessWindowEvents(hDosWindow)){
			break;
		}
	}
}

DWORD WINAPI DosWindowThread(HWND hIgnore){
	int i;
	
	HWND DosWindow;
	
	BackingBitmap = CreateBackingDIB(&pBits);
	DosWindow = CreateDOSWindow();

	SetTimer(DosWindow, 1, 10, NULL);
	SetTimer(DosWindow, 2, 1000, NULL);
	TickCountStart = GetTickCount();
	
	SetVGAPalette(0, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(1, 0x00, 0x00, 0xaa, BackingBitmap);
	SetVGAPalette(2, 0x00, 0xaa, 0x00, BackingBitmap);
	SetVGAPalette(3, 0x00, 0xaa, 0xaa, BackingBitmap);
	SetVGAPalette(4, 0xaa, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(5, 0xaa, 0x00, 0xaa, BackingBitmap);
	SetVGAPalette(6, 0xaa, 0x55, 0x00, BackingBitmap);
	SetVGAPalette(7, 0xaa, 0xaa, 0xaa, BackingBitmap);
	SetVGAPalette(8, 0x55, 0x55, 0x55, BackingBitmap);
	SetVGAPalette(9, 0x55, 0x55, 0xff, BackingBitmap);
	SetVGAPalette(10, 0x55, 0xff, 0x55, BackingBitmap);
	SetVGAPalette(11, 0x55, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(12, 0xff, 0x55, 0x55, BackingBitmap);
	SetVGAPalette(13, 0xff, 0x55, 0xff, BackingBitmap);
	SetVGAPalette(14, 0xff, 0xff, 0x55, BackingBitmap);
	SetVGAPalette(15, 0xff, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(16, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(17, 0x14, 0x14, 0x14, BackingBitmap);
	SetVGAPalette(18, 0x20, 0x20, 0x20, BackingBitmap);
	SetVGAPalette(19, 0x2c, 0x2c, 0x2c, BackingBitmap);
	SetVGAPalette(20, 0x38, 0x38, 0x38, BackingBitmap);
	SetVGAPalette(21, 0x45, 0x45, 0x45, BackingBitmap);
	SetVGAPalette(22, 0x51, 0x51, 0x51, BackingBitmap);
	SetVGAPalette(23, 0x61, 0x61, 0x61, BackingBitmap);
	SetVGAPalette(24, 0x71, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(25, 0x82, 0x82, 0x82, BackingBitmap);
	SetVGAPalette(26, 0x92, 0x92, 0x92, BackingBitmap);
	SetVGAPalette(27, 0xa2, 0xa2, 0xa2, BackingBitmap);
	SetVGAPalette(28, 0xb6, 0xb6, 0xb6, BackingBitmap);
	SetVGAPalette(29, 0xcb, 0xcb, 0xcb, BackingBitmap);
	SetVGAPalette(30, 0xe3, 0xe3, 0xe3, BackingBitmap);
	SetVGAPalette(31, 0xff, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(32, 0x00, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(33, 0x41, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(34, 0x7d, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(35, 0xbe, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(36, 0xff, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(37, 0xff, 0x00, 0xbe, BackingBitmap);
	SetVGAPalette(38, 0xff, 0x00, 0x7d, BackingBitmap);
	SetVGAPalette(39, 0xff, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(40, 0xff, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(41, 0xff, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(42, 0xff, 0x7d, 0x00, BackingBitmap);
	SetVGAPalette(43, 0xff, 0xbe, 0x00, BackingBitmap);
	SetVGAPalette(44, 0xff, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(45, 0xbe, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(46, 0x7d, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(47, 0x41, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(48, 0x00, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(49, 0x00, 0xff, 0x41, BackingBitmap);
	SetVGAPalette(50, 0x00, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(51, 0x00, 0xff, 0xbe, BackingBitmap);
	SetVGAPalette(52, 0x00, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(53, 0x00, 0xbe, 0xff, BackingBitmap);
	SetVGAPalette(54, 0x00, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(55, 0x00, 0x41, 0xff, BackingBitmap);
	SetVGAPalette(56, 0x7d, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(57, 0x9e, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(58, 0xbe, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(59, 0xdf, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(60, 0xff, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(61, 0xff, 0x7d, 0xdf, BackingBitmap);
	SetVGAPalette(62, 0xff, 0x7d, 0xbe, BackingBitmap);
	SetVGAPalette(63, 0xff, 0x7d, 0x9e, BackingBitmap);
	SetVGAPalette(64, 0xff, 0x7d, 0x7d, BackingBitmap);
	SetVGAPalette(65, 0xff, 0x9e, 0x7d, BackingBitmap);
	SetVGAPalette(66, 0xff, 0xbe, 0x7d, BackingBitmap);
	SetVGAPalette(67, 0xff, 0xdf, 0x7d, BackingBitmap);
	SetVGAPalette(68, 0xff, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(69, 0xdf, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(70, 0xbe, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(71, 0x9e, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(72, 0x7d, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(73, 0x7d, 0xff, 0x9e, BackingBitmap);
	SetVGAPalette(74, 0x7d, 0xff, 0xbe, BackingBitmap);
	SetVGAPalette(75, 0x7d, 0xff, 0xdf, BackingBitmap);
	SetVGAPalette(76, 0x7d, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(77, 0x7d, 0xdf, 0xff, BackingBitmap);
	SetVGAPalette(78, 0x7d, 0xbe, 0xff, BackingBitmap);
	SetVGAPalette(79, 0x7d, 0x9e, 0xff, BackingBitmap);
	SetVGAPalette(80, 0xb6, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(81, 0xc7, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(82, 0xdb, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(83, 0xeb, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(84, 0xff, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(85, 0xff, 0xb6, 0xeb, BackingBitmap);
	SetVGAPalette(86, 0xff, 0xb6, 0xdb, BackingBitmap);
	SetVGAPalette(87, 0xff, 0xb6, 0xc7, BackingBitmap);
	SetVGAPalette(88, 0xff, 0xb6, 0xb6, BackingBitmap);
	SetVGAPalette(89, 0xff, 0xc7, 0xb6, BackingBitmap);
	SetVGAPalette(90, 0xff, 0xdb, 0xb6, BackingBitmap);
	SetVGAPalette(91, 0xff, 0xeb, 0xb6, BackingBitmap);
	SetVGAPalette(92, 0xff, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(93, 0xeb, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(94, 0xdb, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(95, 0xc7, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(96, 0xb6, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(97, 0xb6, 0xff, 0xc7, BackingBitmap);
	SetVGAPalette(98, 0xb6, 0xff, 0xdb, BackingBitmap);
	SetVGAPalette(99, 0xb6, 0xff, 0xeb, BackingBitmap);
	SetVGAPalette(100, 0xb6, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(101, 0xb6, 0xeb, 0xff, BackingBitmap);
	SetVGAPalette(102, 0xb6, 0xdb, 0xff, BackingBitmap);
	SetVGAPalette(103, 0xb6, 0xc7, 0xff, BackingBitmap);
	SetVGAPalette(104, 0x00, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(105, 0x1c, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(106, 0x38, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(107, 0x55, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(108, 0x71, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(109, 0x71, 0x00, 0x55, BackingBitmap);
	SetVGAPalette(110, 0x71, 0x00, 0x38, BackingBitmap);
	SetVGAPalette(111, 0x71, 0x00, 0x1c, BackingBitmap);
	SetVGAPalette(112, 0x71, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(113, 0x71, 0x1c, 0x00, BackingBitmap);
	SetVGAPalette(114, 0x71, 0x38, 0x00, BackingBitmap);
	SetVGAPalette(115, 0x71, 0x55, 0x00, BackingBitmap);
	SetVGAPalette(116, 0x71, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(117, 0x55, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(118, 0x38, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(119, 0x1c, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(120, 0x00, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(121, 0x00, 0x71, 0x1c, BackingBitmap);
	SetVGAPalette(122, 0x00, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(123, 0x00, 0x71, 0x55, BackingBitmap);
	SetVGAPalette(124, 0x00, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(125, 0x00, 0x55, 0x71, BackingBitmap);
	SetVGAPalette(126, 0x00, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(127, 0x00, 0x1c, 0x71, BackingBitmap);
	SetVGAPalette(128, 0x38, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(129, 0x45, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(130, 0x55, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(131, 0x61, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(132, 0x71, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(133, 0x71, 0x38, 0x61, BackingBitmap);
	SetVGAPalette(134, 0x71, 0x38, 0x55, BackingBitmap);
	SetVGAPalette(135, 0x71, 0x38, 0x45, BackingBitmap);
	SetVGAPalette(136, 0x71, 0x38, 0x38, BackingBitmap);
	SetVGAPalette(137, 0x71, 0x45, 0x38, BackingBitmap);
	SetVGAPalette(138, 0x71, 0x55, 0x38, BackingBitmap);
	SetVGAPalette(139, 0x71, 0x61, 0x38, BackingBitmap);
	SetVGAPalette(140, 0x71, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(141, 0x61, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(142, 0x55, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(143, 0x45, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(144, 0x38, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(145, 0x38, 0x71, 0x45, BackingBitmap);
	SetVGAPalette(146, 0x38, 0x71, 0x55, BackingBitmap);
	SetVGAPalette(147, 0x38, 0x71, 0x61, BackingBitmap);
	SetVGAPalette(148, 0x38, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(149, 0x38, 0x61, 0x71, BackingBitmap);
	SetVGAPalette(150, 0x38, 0x55, 0x71, BackingBitmap);
	SetVGAPalette(151, 0x38, 0x45, 0x71, BackingBitmap);
	SetVGAPalette(152, 0x51, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(153, 0x59, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(154, 0x61, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(155, 0x69, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(156, 0x71, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(157, 0x71, 0x51, 0x69, BackingBitmap);
	SetVGAPalette(158, 0x71, 0x51, 0x61, BackingBitmap);
	SetVGAPalette(159, 0x71, 0x51, 0x59, BackingBitmap);
	SetVGAPalette(160, 0x71, 0x51, 0x51, BackingBitmap);
	SetVGAPalette(161, 0x71, 0x59, 0x51, BackingBitmap);
	SetVGAPalette(162, 0x71, 0x61, 0x51, BackingBitmap);
	SetVGAPalette(163, 0x71, 0x69, 0x51, BackingBitmap);
	SetVGAPalette(164, 0x71, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(165, 0x69, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(166, 0x61, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(167, 0x59, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(168, 0x51, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(169, 0x51, 0x71, 0x59, BackingBitmap);
	SetVGAPalette(170, 0x51, 0x71, 0x61, BackingBitmap);
	SetVGAPalette(171, 0x51, 0x71, 0x69, BackingBitmap);
	SetVGAPalette(172, 0x51, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(173, 0x51, 0x69, 0x71, BackingBitmap);
	SetVGAPalette(174, 0x51, 0x61, 0x71, BackingBitmap);
	SetVGAPalette(175, 0x51, 0x59, 0x71, BackingBitmap);
	SetVGAPalette(176, 0x00, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(177, 0x10, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(178, 0x20, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(179, 0x30, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(180, 0x41, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(181, 0x41, 0x00, 0x30, BackingBitmap);
	SetVGAPalette(182, 0x41, 0x00, 0x20, BackingBitmap);
	SetVGAPalette(183, 0x41, 0x00, 0x10, BackingBitmap);
	SetVGAPalette(184, 0x41, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(185, 0x41, 0x10, 0x00, BackingBitmap);
	SetVGAPalette(186, 0x41, 0x20, 0x00, BackingBitmap);
	SetVGAPalette(187, 0x41, 0x30, 0x00, BackingBitmap);
	SetVGAPalette(188, 0x41, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(189, 0x30, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(190, 0x20, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(191, 0x10, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(192, 0x00, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(193, 0x00, 0x41, 0x10, BackingBitmap);
	SetVGAPalette(194, 0x00, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(195, 0x00, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(196, 0x00, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(197, 0x00, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(198, 0x00, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(199, 0x00, 0x10, 0x41, BackingBitmap);
	SetVGAPalette(200, 0x20, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(201, 0x28, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(202, 0x30, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(203, 0x38, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(204, 0x41, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(205, 0x41, 0x20, 0x38, BackingBitmap);
	SetVGAPalette(206, 0x41, 0x20, 0x30, BackingBitmap);
	SetVGAPalette(207, 0x41, 0x20, 0x28, BackingBitmap);
	SetVGAPalette(208, 0x41, 0x20, 0x20, BackingBitmap);
	SetVGAPalette(209, 0x41, 0x28, 0x20, BackingBitmap);
	SetVGAPalette(210, 0x41, 0x30, 0x20, BackingBitmap);
	SetVGAPalette(211, 0x41, 0x38, 0x20, BackingBitmap);
	SetVGAPalette(212, 0x41, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(213, 0x38, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(214, 0x30, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(215, 0x28, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(216, 0x20, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(217, 0x20, 0x41, 0x28, BackingBitmap);
	SetVGAPalette(218, 0x20, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(219, 0x20, 0x41, 0x38, BackingBitmap);
	SetVGAPalette(220, 0x20, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(221, 0x20, 0x38, 0x41, BackingBitmap);
	SetVGAPalette(222, 0x20, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(223, 0x20, 0x28, 0x41, BackingBitmap);
	SetVGAPalette(224, 0x2c, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(225, 0x30, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(226, 0x34, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(227, 0x3c, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(228, 0x41, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(229, 0x41, 0x2c, 0x3c, BackingBitmap);
	SetVGAPalette(230, 0x41, 0x2c, 0x34, BackingBitmap);
	SetVGAPalette(231, 0x41, 0x2c, 0x30, BackingBitmap);
	SetVGAPalette(232, 0x41, 0x2c, 0x2c, BackingBitmap);
	SetVGAPalette(233, 0x41, 0x30, 0x2c, BackingBitmap);
	SetVGAPalette(234, 0x41, 0x34, 0x2c, BackingBitmap);
	SetVGAPalette(235, 0x41, 0x3c, 0x2c, BackingBitmap);
	SetVGAPalette(236, 0x41, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(237, 0x3c, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(238, 0x34, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(239, 0x30, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(240, 0x2c, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(241, 0x2c, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(242, 0x2c, 0x41, 0x34, BackingBitmap);
	SetVGAPalette(243, 0x2c, 0x41, 0x3c, BackingBitmap);
	SetVGAPalette(244, 0x2c, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(245, 0x2c, 0x3c, 0x41, BackingBitmap);
	SetVGAPalette(246, 0x2c, 0x34, 0x41, BackingBitmap);
	SetVGAPalette(247, 0x2c, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(248, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(249, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(250, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(251, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(252, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(253, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(254, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(255, 0x00, 0x00, 0x00, BackingBitmap);
	
	
	while(1){
		if(ProcessWindowEvents(DosWindow)) break;
	}
	
	return 0;
}

HWND InitDOSWindow(){
	CreateThread(NULL, 0, DosWindowThread, NULL, 0, NULL);
	pAddVectoredExceptionHandler = GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "AddVectoredExceptionHandler");
	if(pAddVectoredExceptionHandler) pAddVectoredExceptionHandler(1, &CallbackExceptionHandler);
	
	return NULL;
}

HWND InitDOSWindowOld(){
	int i;
	
	HWND DosWindow;
	
	BackingBitmap = CreateBackingDIB(&pBits);
	DosWindow = CreateDOSWindow();

	SetTimer(DosWindow, 1, 10, NULL);
	
	pAddVectoredExceptionHandler = GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "AddVectoredExceptionHandler");
	pAddVectoredExceptionHandler(1, &CallbackExceptionHandler);

	SetVGAPalette(0, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(1, 0x00, 0x00, 0xaa, BackingBitmap);
	SetVGAPalette(2, 0x00, 0xaa, 0x00, BackingBitmap);
	SetVGAPalette(3, 0x00, 0xaa, 0xaa, BackingBitmap);
	SetVGAPalette(4, 0xaa, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(5, 0xaa, 0x00, 0xaa, BackingBitmap);
	SetVGAPalette(6, 0xaa, 0x55, 0x00, BackingBitmap);
	SetVGAPalette(7, 0xaa, 0xaa, 0xaa, BackingBitmap);
	SetVGAPalette(8, 0x55, 0x55, 0x55, BackingBitmap);
	SetVGAPalette(9, 0x55, 0x55, 0xff, BackingBitmap);
	SetVGAPalette(10, 0x55, 0xff, 0x55, BackingBitmap);
	SetVGAPalette(11, 0x55, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(12, 0xff, 0x55, 0x55, BackingBitmap);
	SetVGAPalette(13, 0xff, 0x55, 0xff, BackingBitmap);
	SetVGAPalette(14, 0xff, 0xff, 0x55, BackingBitmap);
	SetVGAPalette(15, 0xff, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(16, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(17, 0x14, 0x14, 0x14, BackingBitmap);
	SetVGAPalette(18, 0x20, 0x20, 0x20, BackingBitmap);
	SetVGAPalette(19, 0x2c, 0x2c, 0x2c, BackingBitmap);
	SetVGAPalette(20, 0x38, 0x38, 0x38, BackingBitmap);
	SetVGAPalette(21, 0x45, 0x45, 0x45, BackingBitmap);
	SetVGAPalette(22, 0x51, 0x51, 0x51, BackingBitmap);
	SetVGAPalette(23, 0x61, 0x61, 0x61, BackingBitmap);
	SetVGAPalette(24, 0x71, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(25, 0x82, 0x82, 0x82, BackingBitmap);
	SetVGAPalette(26, 0x92, 0x92, 0x92, BackingBitmap);
	SetVGAPalette(27, 0xa2, 0xa2, 0xa2, BackingBitmap);
	SetVGAPalette(28, 0xb6, 0xb6, 0xb6, BackingBitmap);
	SetVGAPalette(29, 0xcb, 0xcb, 0xcb, BackingBitmap);
	SetVGAPalette(30, 0xe3, 0xe3, 0xe3, BackingBitmap);
	SetVGAPalette(31, 0xff, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(32, 0x00, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(33, 0x41, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(34, 0x7d, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(35, 0xbe, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(36, 0xff, 0x00, 0xff, BackingBitmap);
	SetVGAPalette(37, 0xff, 0x00, 0xbe, BackingBitmap);
	SetVGAPalette(38, 0xff, 0x00, 0x7d, BackingBitmap);
	SetVGAPalette(39, 0xff, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(40, 0xff, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(41, 0xff, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(42, 0xff, 0x7d, 0x00, BackingBitmap);
	SetVGAPalette(43, 0xff, 0xbe, 0x00, BackingBitmap);
	SetVGAPalette(44, 0xff, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(45, 0xbe, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(46, 0x7d, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(47, 0x41, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(48, 0x00, 0xff, 0x00, BackingBitmap);
	SetVGAPalette(49, 0x00, 0xff, 0x41, BackingBitmap);
	SetVGAPalette(50, 0x00, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(51, 0x00, 0xff, 0xbe, BackingBitmap);
	SetVGAPalette(52, 0x00, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(53, 0x00, 0xbe, 0xff, BackingBitmap);
	SetVGAPalette(54, 0x00, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(55, 0x00, 0x41, 0xff, BackingBitmap);
	SetVGAPalette(56, 0x7d, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(57, 0x9e, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(58, 0xbe, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(59, 0xdf, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(60, 0xff, 0x7d, 0xff, BackingBitmap);
	SetVGAPalette(61, 0xff, 0x7d, 0xdf, BackingBitmap);
	SetVGAPalette(62, 0xff, 0x7d, 0xbe, BackingBitmap);
	SetVGAPalette(63, 0xff, 0x7d, 0x9e, BackingBitmap);
	SetVGAPalette(64, 0xff, 0x7d, 0x7d, BackingBitmap);
	SetVGAPalette(65, 0xff, 0x9e, 0x7d, BackingBitmap);
	SetVGAPalette(66, 0xff, 0xbe, 0x7d, BackingBitmap);
	SetVGAPalette(67, 0xff, 0xdf, 0x7d, BackingBitmap);
	SetVGAPalette(68, 0xff, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(69, 0xdf, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(70, 0xbe, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(71, 0x9e, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(72, 0x7d, 0xff, 0x7d, BackingBitmap);
	SetVGAPalette(73, 0x7d, 0xff, 0x9e, BackingBitmap);
	SetVGAPalette(74, 0x7d, 0xff, 0xbe, BackingBitmap);
	SetVGAPalette(75, 0x7d, 0xff, 0xdf, BackingBitmap);
	SetVGAPalette(76, 0x7d, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(77, 0x7d, 0xdf, 0xff, BackingBitmap);
	SetVGAPalette(78, 0x7d, 0xbe, 0xff, BackingBitmap);
	SetVGAPalette(79, 0x7d, 0x9e, 0xff, BackingBitmap);
	SetVGAPalette(80, 0xb6, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(81, 0xc7, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(82, 0xdb, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(83, 0xeb, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(84, 0xff, 0xb6, 0xff, BackingBitmap);
	SetVGAPalette(85, 0xff, 0xb6, 0xeb, BackingBitmap);
	SetVGAPalette(86, 0xff, 0xb6, 0xdb, BackingBitmap);
	SetVGAPalette(87, 0xff, 0xb6, 0xc7, BackingBitmap);
	SetVGAPalette(88, 0xff, 0xb6, 0xb6, BackingBitmap);
	SetVGAPalette(89, 0xff, 0xc7, 0xb6, BackingBitmap);
	SetVGAPalette(90, 0xff, 0xdb, 0xb6, BackingBitmap);
	SetVGAPalette(91, 0xff, 0xeb, 0xb6, BackingBitmap);
	SetVGAPalette(92, 0xff, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(93, 0xeb, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(94, 0xdb, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(95, 0xc7, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(96, 0xb6, 0xff, 0xb6, BackingBitmap);
	SetVGAPalette(97, 0xb6, 0xff, 0xc7, BackingBitmap);
	SetVGAPalette(98, 0xb6, 0xff, 0xdb, BackingBitmap);
	SetVGAPalette(99, 0xb6, 0xff, 0xeb, BackingBitmap);
	SetVGAPalette(100, 0xb6, 0xff, 0xff, BackingBitmap);
	SetVGAPalette(101, 0xb6, 0xeb, 0xff, BackingBitmap);
	SetVGAPalette(102, 0xb6, 0xdb, 0xff, BackingBitmap);
	SetVGAPalette(103, 0xb6, 0xc7, 0xff, BackingBitmap);
	SetVGAPalette(104, 0x00, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(105, 0x1c, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(106, 0x38, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(107, 0x55, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(108, 0x71, 0x00, 0x71, BackingBitmap);
	SetVGAPalette(109, 0x71, 0x00, 0x55, BackingBitmap);
	SetVGAPalette(110, 0x71, 0x00, 0x38, BackingBitmap);
	SetVGAPalette(111, 0x71, 0x00, 0x1c, BackingBitmap);
	SetVGAPalette(112, 0x71, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(113, 0x71, 0x1c, 0x00, BackingBitmap);
	SetVGAPalette(114, 0x71, 0x38, 0x00, BackingBitmap);
	SetVGAPalette(115, 0x71, 0x55, 0x00, BackingBitmap);
	SetVGAPalette(116, 0x71, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(117, 0x55, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(118, 0x38, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(119, 0x1c, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(120, 0x00, 0x71, 0x00, BackingBitmap);
	SetVGAPalette(121, 0x00, 0x71, 0x1c, BackingBitmap);
	SetVGAPalette(122, 0x00, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(123, 0x00, 0x71, 0x55, BackingBitmap);
	SetVGAPalette(124, 0x00, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(125, 0x00, 0x55, 0x71, BackingBitmap);
	SetVGAPalette(126, 0x00, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(127, 0x00, 0x1c, 0x71, BackingBitmap);
	SetVGAPalette(128, 0x38, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(129, 0x45, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(130, 0x55, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(131, 0x61, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(132, 0x71, 0x38, 0x71, BackingBitmap);
	SetVGAPalette(133, 0x71, 0x38, 0x61, BackingBitmap);
	SetVGAPalette(134, 0x71, 0x38, 0x55, BackingBitmap);
	SetVGAPalette(135, 0x71, 0x38, 0x45, BackingBitmap);
	SetVGAPalette(136, 0x71, 0x38, 0x38, BackingBitmap);
	SetVGAPalette(137, 0x71, 0x45, 0x38, BackingBitmap);
	SetVGAPalette(138, 0x71, 0x55, 0x38, BackingBitmap);
	SetVGAPalette(139, 0x71, 0x61, 0x38, BackingBitmap);
	SetVGAPalette(140, 0x71, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(141, 0x61, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(142, 0x55, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(143, 0x45, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(144, 0x38, 0x71, 0x38, BackingBitmap);
	SetVGAPalette(145, 0x38, 0x71, 0x45, BackingBitmap);
	SetVGAPalette(146, 0x38, 0x71, 0x55, BackingBitmap);
	SetVGAPalette(147, 0x38, 0x71, 0x61, BackingBitmap);
	SetVGAPalette(148, 0x38, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(149, 0x38, 0x61, 0x71, BackingBitmap);
	SetVGAPalette(150, 0x38, 0x55, 0x71, BackingBitmap);
	SetVGAPalette(151, 0x38, 0x45, 0x71, BackingBitmap);
	SetVGAPalette(152, 0x51, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(153, 0x59, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(154, 0x61, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(155, 0x69, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(156, 0x71, 0x51, 0x71, BackingBitmap);
	SetVGAPalette(157, 0x71, 0x51, 0x69, BackingBitmap);
	SetVGAPalette(158, 0x71, 0x51, 0x61, BackingBitmap);
	SetVGAPalette(159, 0x71, 0x51, 0x59, BackingBitmap);
	SetVGAPalette(160, 0x71, 0x51, 0x51, BackingBitmap);
	SetVGAPalette(161, 0x71, 0x59, 0x51, BackingBitmap);
	SetVGAPalette(162, 0x71, 0x61, 0x51, BackingBitmap);
	SetVGAPalette(163, 0x71, 0x69, 0x51, BackingBitmap);
	SetVGAPalette(164, 0x71, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(165, 0x69, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(166, 0x61, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(167, 0x59, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(168, 0x51, 0x71, 0x51, BackingBitmap);
	SetVGAPalette(169, 0x51, 0x71, 0x59, BackingBitmap);
	SetVGAPalette(170, 0x51, 0x71, 0x61, BackingBitmap);
	SetVGAPalette(171, 0x51, 0x71, 0x69, BackingBitmap);
	SetVGAPalette(172, 0x51, 0x71, 0x71, BackingBitmap);
	SetVGAPalette(173, 0x51, 0x69, 0x71, BackingBitmap);
	SetVGAPalette(174, 0x51, 0x61, 0x71, BackingBitmap);
	SetVGAPalette(175, 0x51, 0x59, 0x71, BackingBitmap);
	SetVGAPalette(176, 0x00, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(177, 0x10, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(178, 0x20, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(179, 0x30, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(180, 0x41, 0x00, 0x41, BackingBitmap);
	SetVGAPalette(181, 0x41, 0x00, 0x30, BackingBitmap);
	SetVGAPalette(182, 0x41, 0x00, 0x20, BackingBitmap);
	SetVGAPalette(183, 0x41, 0x00, 0x10, BackingBitmap);
	SetVGAPalette(184, 0x41, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(185, 0x41, 0x10, 0x00, BackingBitmap);
	SetVGAPalette(186, 0x41, 0x20, 0x00, BackingBitmap);
	SetVGAPalette(187, 0x41, 0x30, 0x00, BackingBitmap);
	SetVGAPalette(188, 0x41, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(189, 0x30, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(190, 0x20, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(191, 0x10, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(192, 0x00, 0x41, 0x00, BackingBitmap);
	SetVGAPalette(193, 0x00, 0x41, 0x10, BackingBitmap);
	SetVGAPalette(194, 0x00, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(195, 0x00, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(196, 0x00, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(197, 0x00, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(198, 0x00, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(199, 0x00, 0x10, 0x41, BackingBitmap);
	SetVGAPalette(200, 0x20, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(201, 0x28, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(202, 0x30, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(203, 0x38, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(204, 0x41, 0x20, 0x41, BackingBitmap);
	SetVGAPalette(205, 0x41, 0x20, 0x38, BackingBitmap);
	SetVGAPalette(206, 0x41, 0x20, 0x30, BackingBitmap);
	SetVGAPalette(207, 0x41, 0x20, 0x28, BackingBitmap);
	SetVGAPalette(208, 0x41, 0x20, 0x20, BackingBitmap);
	SetVGAPalette(209, 0x41, 0x28, 0x20, BackingBitmap);
	SetVGAPalette(210, 0x41, 0x30, 0x20, BackingBitmap);
	SetVGAPalette(211, 0x41, 0x38, 0x20, BackingBitmap);
	SetVGAPalette(212, 0x41, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(213, 0x38, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(214, 0x30, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(215, 0x28, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(216, 0x20, 0x41, 0x20, BackingBitmap);
	SetVGAPalette(217, 0x20, 0x41, 0x28, BackingBitmap);
	SetVGAPalette(218, 0x20, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(219, 0x20, 0x41, 0x38, BackingBitmap);
	SetVGAPalette(220, 0x20, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(221, 0x20, 0x38, 0x41, BackingBitmap);
	SetVGAPalette(222, 0x20, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(223, 0x20, 0x28, 0x41, BackingBitmap);
	SetVGAPalette(224, 0x2c, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(225, 0x30, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(226, 0x34, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(227, 0x3c, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(228, 0x41, 0x2c, 0x41, BackingBitmap);
	SetVGAPalette(229, 0x41, 0x2c, 0x3c, BackingBitmap);
	SetVGAPalette(230, 0x41, 0x2c, 0x34, BackingBitmap);
	SetVGAPalette(231, 0x41, 0x2c, 0x30, BackingBitmap);
	SetVGAPalette(232, 0x41, 0x2c, 0x2c, BackingBitmap);
	SetVGAPalette(233, 0x41, 0x30, 0x2c, BackingBitmap);
	SetVGAPalette(234, 0x41, 0x34, 0x2c, BackingBitmap);
	SetVGAPalette(235, 0x41, 0x3c, 0x2c, BackingBitmap);
	SetVGAPalette(236, 0x41, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(237, 0x3c, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(238, 0x34, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(239, 0x30, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(240, 0x2c, 0x41, 0x2c, BackingBitmap);
	SetVGAPalette(241, 0x2c, 0x41, 0x30, BackingBitmap);
	SetVGAPalette(242, 0x2c, 0x41, 0x34, BackingBitmap);
	SetVGAPalette(243, 0x2c, 0x41, 0x3c, BackingBitmap);
	SetVGAPalette(244, 0x2c, 0x41, 0x41, BackingBitmap);
	SetVGAPalette(245, 0x2c, 0x3c, 0x41, BackingBitmap);
	SetVGAPalette(246, 0x2c, 0x34, 0x41, BackingBitmap);
	SetVGAPalette(247, 0x2c, 0x30, 0x41, BackingBitmap);
	SetVGAPalette(248, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(249, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(250, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(251, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(252, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(253, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(254, 0x00, 0x00, 0x00, BackingBitmap);
	SetVGAPalette(255, 0x00, 0x00, 0x00, BackingBitmap);
	
	CreateThread(NULL, 0, DosWindowThread, DosWindow, 0, NULL);
	
	return DosWindow;
}