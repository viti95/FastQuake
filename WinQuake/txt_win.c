//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Text mode emulation in SDL
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "winquake.h"

#include <ddraw.h>

#include "txt.h"

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif


typedef struct
{
    unsigned char *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

typedef unsigned char uint8_t;

// Fonts:

#include "txt_font.h"
#include "txt_largefont.h"
#include "txt_smallfont.h"

// static SDL_Surface *screen;
static unsigned char *screen;
static unsigned char *screenbuffer;
static unsigned char *screendata;
extern HICON g_hIcon;

// Font we are using:

static txt_font_t *font;

//#define TANGO

#ifndef TANGO

static RGBQUAD ega_colors[] = 
{
    {0x00, 0x00, 0x00, 0x00},          // 0: Black
    {0x00, 0x00, 0xa8, 0x00},          // 1: Blue
    {0x00, 0xa8, 0x00, 0x00},          // 2: Green
    {0x00, 0xa8, 0xa8, 0x00},          // 3: Cyan
    {0xa8, 0x00, 0x00, 0x00},          // 4: Red
    {0xa8, 0x00, 0xa8, 0x00},          // 5: Magenta
    {0xa8, 0x54, 0x00, 0x00},          // 6: Brown
    {0xa8, 0xa8, 0xa8, 0x00},          // 7: Grey
    {0x54, 0x54, 0x54, 0x00},          // 8: Dark grey
    {0x54, 0x54, 0xfe, 0x00},          // 9: Bright blue
    {0x54, 0xfe, 0x54, 0x00},          // 10: Bright green
    {0x54, 0xfe, 0xfe, 0x00},          // 11: Bright cyan
    {0xfe, 0x54, 0x54, 0x00},          // 12: Bright red
    {0xfe, 0x54, 0xfe, 0x00},          // 13: Bright magenta
    {0xfe, 0xfe, 0x54, 0x00},          // 14: Yellow
    {0xfe, 0xfe, 0xfe, 0x00},          // 15: Bright white
};

#else

// Colors that fit the Tango desktop guidelines: see
// http://tango.freedesktop.org/ also
// http://uwstopia.nl/blog/2006/07/tango-terminal

static RGBQUAD ega_colors[] = 
{
    {0x2e, 0x34, 0x36, 0x00},          // 0: Black
    {0x34, 0x65, 0xa4, 0x00},          // 1: Blue
    {0x4e, 0x9a, 0x06, 0x00},          // 2: Green
    {0x06, 0x98, 0x9a, 0x00},          // 3: Cyan
    {0xcc, 0x00, 0x00, 0x00},          // 4: Red
    {0x75, 0x50, 0x7b, 0x00},          // 5: Magenta
    {0xc4, 0xa0, 0x00, 0x00},          // 6: Brown
    {0xd3, 0xd7, 0xcf, 0x00},          // 7: Grey
    {0x55, 0x57, 0x53, 0x00},          // 8: Dark grey
    {0x72, 0x9f, 0xcf, 0x00},          // 9: Bright blue
    {0x8a, 0xe2, 0x34, 0x00},          // 10: Bright green
    {0x34, 0xe2, 0xe2, 0x00},          // 11: Bright cyan
    {0xef, 0x29, 0x29, 0x00},          // 12: Bright red
    {0x34, 0xe2, 0xe2, 0x00},          // 13: Bright magenta
    {0xfc, 0xe9, 0x4f, 0x00},          // 14: Yellow
    {0xee, 0xee, 0xec, 0x00},          // 15: Bright white
};

#endif

LPDIRECTDRAW7 pDD;
LPDIRECTDRAWSURFACE7 pDDPrimary, pDDSecondary;

static void TXT_Shutdown(HWND hwnd);
static void TXT_UpdateScreen(HWND hwnd);
static void TXT_Init(HWND hwnd);

// Examine system DPI settings to determine whether to use the large font.

static int Win32_UseLargeFont(void)
{
    HDC hdc = GetDC(NULL);
    int dpix;

    if (!hdc)
    {
        return 0;
    }

    dpix = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    // 144 is the DPI when using "150%" scaling. If the user has this set
    // then consider this an appropriate threshold for using the large font.

    return dpix >= 144;
}

static txt_font_t *FontForName(char *name)
{
    if (!strcmp(name, "small"))
    {
        return &small_font;
    }
    else if (!strcmp(name, "normal"))
    {
        return &main_font;
    }
    else if (!strcmp(name, "large"))
    {
        return &large_font;
    }
    else
    {
        return NULL;
    }
}

//
// Select the font to use, based on screen resolution
//
// If the highest screen resolution available is less than
// 640x480, use the small font.
//

static void ChooseTextFont(HWND hwnd)
{
	HMONITOR monitor;
	MONITORINFO mi;
    char *env;
	unsigned current_w, current_h;

    // Allow normal selection to be overridden from an environment variable:

    env = getenv("TEXTSCREEN_FONT");

    if (env != NULL)
    {
        font = FontForName(env);

        if (font != NULL)
        {
            return;
        }
    }

	monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    // fall back to the normal font:
	if (GetMonitorInfoA(monitor, &mi) == FALSE)
    {
        font = &main_font;
        return;
    }

    // Get current screen resolution:
	current_w = mi.rcMonitor.right-mi.rcMonitor.left;
	current_h = mi.rcMonitor.bottom-mi.rcMonitor.top;

    // On tiny low-res screens (eg. palmtops) use the small font.
    // If the screen resolution is at least 1920x1080, this is
    // a modern high-resolution display, and we can use the
    // large font.

	if (current_w < 640 || current_h < 480)
    {
        font = &small_font;
    }

    // On Windows we can use the system DPI settings to make a
    // more educated guess about whether to use the large font.

    else if (Win32_UseLargeFont())
    {
        font = &large_font;
    }

    else
    {
        font = &main_font;
    }
}

void TXT_Init(HWND hwnd)
{
	HRESULT hr;
	RECT rect;

	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWCLIPPER pDDClipper;

	ChooseTextFont(hwnd);

	GetWindowRect(hwnd, &rect);
	MoveWindow(
		hwnd,
		rect.left, rect.top,
		TXT_SCREEN_W * font->w,
		TXT_SCREEN_H * font->h,
		FALSE
	);

    screenbuffer = calloc(TXT_SCREEN_W * TXT_SCREEN_H, font->w * font->h);
	screen = calloc(TXT_SCREEN_W * TXT_SCREEN_H * font->w * font->h, 4);

	hr = DirectDrawCreateEx(NULL, &pDD, &IID_IDirectDraw7, NULL);
	hr = IDirectDraw7_SetCooperativeLevel(pDD, hwnd, DDSCL_NORMAL);

	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	hr = IDirectDraw7_CreateSurface(pDD, &ddsd, &pDDPrimary, NULL);

	hr = IDirectDraw7_CreateClipper(pDD, 0, &pDDClipper, NULL);
	hr = IDirectDrawClipper_SetHWnd(pDDClipper, 0, hwnd);
	hr = IDirectDrawSurface7_SetClipper(pDDPrimary, pDDClipper);
	IDirectDrawClipper_Release(pDDClipper);

	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = TXT_SCREEN_W * font->w;
	ddsd.dwHeight = TXT_SCREEN_H * font->h;
	hr = IDirectDraw7_CreateSurface(pDD, &ddsd, &pDDSecondary, NULL);

	if (hr);
}

LRESULT CALLBACK TXT_WndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	switch (umsg)
	{
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_KEYDOWN:
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_PAINT:
		TXT_UpdateScreen(hwnd);
		break;
	case WM_CREATE:
		TXT_Init(hwnd);
		break;
	case WM_DESTROY:
		TXT_Shutdown(hwnd);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, umsg, wparam, lparam);
	}
	return 0;
}

void TXT_ShowScreen(const char *title, byte *ascreendata)
{
	WNDCLASS wc;
	ATOM atom;
	HWND hwnd;
	MSG msg;

    if (ascreendata == NULL)
        return;

    screendata = ascreendata;

	ZeroMemory(&wc, sizeof(wc));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = g_hIcon;
	wc.hInstance = global_hInstance;
	wc.lpfnWndProc = TXT_WndProc;
	wc.lpszClassName = "WinQuakeTXT";
	atom = RegisterClass(&wc);

	hwnd = CreateWindow(
		MAKEINTATOM(atom),
		title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		(HWND)NULL,
		(HMENU)NULL,
		(HINSTANCE)global_hInstance,
		(LPVOID)NULL
	);

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void TXT_Shutdown(HWND hwnd)
{
    free(screenbuffer);
	free(screen);
    screenbuffer = NULL;
	screen = NULL;

	if (pDD)
	{
		IDirectDraw7_Release(pDD);
		pDD = NULL;
	}
	if (pDDPrimary)
	{
		IDirectDrawSurface7_Release(pDDPrimary);
		pDDPrimary = NULL;
	}
	if (pDDSecondary)
	{
		IDirectDrawSurface7_Release(pDDSecondary);
		pDDSecondary = NULL;
	}
}

static inline void UpdateCharacter(int x, int y)
{
    unsigned char character;
    unsigned char *p;
    unsigned char *s, *s1;
    unsigned int bit, bytes;
    int bg, fg;
    unsigned int x1, y1;

	unsigned int pitch = TXT_SCREEN_W * font->w;

    p = &screendata[(y * TXT_SCREEN_W + x) * 2];
    character = p[0];

    fg = p[1] & 0xf;
    bg = (p[1] >> 4) & 0xf;

    // How many bytes per line?
    bytes = (font->w + 7) / 8;
    p = &font->data[character * font->h * bytes];

    s = ((unsigned char *) screenbuffer)
      + (y * font->h * pitch)
      + (x * font->w);

    for (y1=0; y1<font->h; ++y1)
    {
        s1 = s;
        bit = 0;

        for (x1=0; x1<font->w; ++x1)
        {
            if (*p & (1 << (7-bit)))
            {
                *s1++ = fg;
            }
            else
            {
                *s1++ = bg;
            }

            ++bit;
            if (bit == 8)
            {
                ++p;
                bit = 0;
            }
        }

        if (bit != 0)
        {
            ++p;
        }

        s += pitch;
    }
}

static void Blit8bppTo32bpp(unsigned char *dst, int dstpitch, unsigned char *src, unsigned w, unsigned h, const RGBQUAD *pal, unsigned numcols)
{
	unsigned x, y;
	
	// SDL_LockSurface(dst);
	for (y=0; y<h; y++)
	{
		RGBQUAD *scanline = (RGBQUAD *)((byte*)dst + y*dstpitch);
		for (x=0; x<w; x++)
		{
			unsigned char col = src[y*w + x];
			if (col >= numcols)
				col = 0;
			memcpy(&scanline[x], &pal[col], sizeof(RGBQUAD));
		}
	}
	// SDL_UnlockSurface(dst);
}

static void TXT_UpdateScreen(HWND hwnd)
{
    RECT rect;
    int x, y;

    for (y=0; y<TXT_SCREEN_H; ++y)
    {
        for (x=0; x<TXT_SCREEN_W; ++x)
        {
            UpdateCharacter(x, y);
        }
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = TXT_SCREEN_W * font->w;
    rect.bottom = TXT_SCREEN_H * font->h;

	Blit8bppTo32bpp(
		screen, TXT_SCREEN_W * font->w * 4, screenbuffer,
		rect.right-rect.left, rect.bottom-rect.top,
		ega_colors, sizeof(ega_colors)/sizeof(RGBQUAD)
	);
}
