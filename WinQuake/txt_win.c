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

#include "SDL.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quakedef.h"

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

// Fonts:

#include "txt_font.h"
#include "txt_largefont.h"
#include "txt_smallfont.h"

static SDL_Surface *screen;
static SDL_Surface *screenbuffer;
static unsigned char *screendata;
static int key_mapping = 1;

// Font we are using:

static txt_font_t *font;

//#define TANGO

#ifndef TANGO

static SDL_Color ega_colors[] = 
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

static SDL_Color ega_colors[] = 
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

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

#endif

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

static void ChooseTextFont(void)
{
    const SDL_VideoInfo *info;
    char *env;

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

    // Get desktop resolution:

    info = SDL_GetVideoInfo();

    // If in doubt and we can't get a list, always prefer to
    // fall back to the normal font:

    if (info == NULL)
    {
        font = &main_font;
        return;
    }

    // On tiny low-res screens (eg. palmtops) use the small font.
    // If the screen resolution is at least 1920x1080, this is
    // a modern high-resolution display, and we can use the
    // large font.

    if (info->current_w < 640 || info->current_h < 480)
    {
        font = &small_font;
    }
#ifdef _WIN32
    // On Windows we can use the system DPI settings to make a
    // more educated guess about whether to use the large font.

    else if (Win32_UseLargeFont())
    {
        font = &large_font;
    }
#endif
    // TODO: Detect high DPI on Linux by inquiring about Gtk+ scale
    // settings. This looks like it should just be a case of shelling
    // out to invoke the 'gsettings' command, eg.
    //   gsettings get org.gnome.desktop.interface text-scaling-factor
    // and using large_font if the result is >= 2.
    else
    {
        font = &main_font;
    }
}

//
// Initialize text mode screen
//
// Returns 1 if successful, 0 if an error occurred
//

int TXT_Init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        return 0;
    }

    ChooseTextFont();

    // Always create the screen at the native screen depth (bpp=0);
    // some systems nowadays don't seem to support true 8-bit palettized
    // screen modes very well and we end up with screwed up colors.
    screen = SDL_SetVideoMode(TXT_SCREEN_W * font->w,
                              TXT_SCREEN_H * font->h, 0, 0);

    if (screen == NULL)
        return 0;

    // Instead, we draw everything into an intermediate 8-bit surface
    // the same dimensions as the screen. SDL then takes care of all the
    // 8->32 bit (or whatever depth) color conversions for us.
    screenbuffer = SDL_CreateRGBSurface(0, TXT_SCREEN_W * font->w,
                                        TXT_SCREEN_H * font->h,
                                        8, 0, 0, 0, 0);
    SDL_SetColors(screenbuffer, ega_colors, 0, 16);

    screendata = malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);

    // Ignore all mouse motion events

//    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    return 1;
}

void TXT_Shutdown(void)
{
    free(screendata);
    screendata = NULL;
    SDL_FreeSurface(screenbuffer);
    screenbuffer = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

unsigned char *TXT_GetScreenData(void)
{
    return screendata;
}

static inline void UpdateCharacter(int x, int y)
{
    unsigned char character;
    unsigned char *p;
    unsigned char *s, *s1;
    unsigned int bit, bytes;
    int bg, fg;
    unsigned int x1, y1;

    p = &screendata[(y * TXT_SCREEN_W + x) * 2];
    character = p[0];

    fg = p[1] & 0xf;
    bg = (p[1] >> 4) & 0xf;

    // How many bytes per line?
    bytes = (font->w + 7) / 8;
    p = &font->data[character * font->h * bytes];

    s = ((unsigned char *) screenbuffer->pixels)
      + (y * font->h * screenbuffer->pitch)
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

        s += screenbuffer->pitch;
    }
}

static int LimitToRange(int val, int min, int max)
{
    if (val < min)
    {
        return min;
    }
    else if (val > max)
    {
        return max;
    }
    else
    {
        return val;
    }
}

void TXT_UpdateScreen(void)
{
    SDL_Rect rect;
    int x, y;

    for (y=0; y<TXT_SCREEN_H; ++y)
    {
        for (x=0; x<TXT_SCREEN_W; ++x)
        {
            UpdateCharacter(x, y);
        }
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = TXT_SCREEN_W * font->w;
    rect.h = TXT_SCREEN_H * font->h;

    SDL_BlitSurface(screenbuffer, &rect, screen, &rect);
    SDL_UpdateRects(screen, 1, &rect);
}


void TXT_WaitForChar(void)
{
    SDL_Event ev;

	while (1)
    {
		SDL_WaitEvent(&ev);

        // Process the event.

        switch (ev.type)
        {
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button < TXT_MAX_MOUSE_BUTTONS)
                    return;
                break;

            case SDL_KEYDOWN:
                return;

            case SDL_KEYUP:
                break;

            case SDL_QUIT:
                // Quit = escape
                return;

            case SDL_MOUSEMOTION:
				break;

            default:
                break;
        }
    }
}

void TXT_SetWindowTitle(char *title)
{
    SDL_WM_SetCaption(title, NULL);
}
