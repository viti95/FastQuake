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

#include "txt_doomkeys.h"

#include "txt.h"

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

// Event callback function type: a function of this type can be used
// to intercept events in the textscreen event processing loop.  
// Returning 1 will cause the event to be eaten; the textscreen code
// will not see it.

typedef int (*TxtSDLEventCallbackFunc)(SDL_Event *event, void *user_data);

// Set a callback function to call in the SDL event loop.  Useful for
// intercepting events.  Pass callback=NULL to clear an existing
// callback function.
// user_data is a void pointer to be passed to the callback function.

void TXT_SDL_SetEventCallback(TxtSDLEventCallbackFunc callback, void *user_data);


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

// Time between character blinks in ms

#define BLINK_PERIOD 250

static SDL_Surface *screen;
static SDL_Surface *screenbuffer;
static unsigned char *screendata;
static int key_mapping = 1;

static TxtSDLEventCallbackFunc event_callback;
static void *event_callback_data;

static int modifier_state[TXT_NUM_MODIFIERS];

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

static void ChooseFont(void)
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

    ChooseFont();

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
    SDL_EnableUNICODE(1);

    screendata = malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);

    // Ignore all mouse motion events

//    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    // Repeat key presses so we can hold down arrows to scroll down the
    // menu, for example. This is what setup.exe does.

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

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

    if (bg & 0x8)
    {
        // blinking

        bg &= ~0x8;

        if (((SDL_GetTicks() / BLINK_PERIOD) % 2) == 0)
        {
            fg = bg;
        }
    }

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

static void TXT_UpdateScreenArea(int x, int y, int w, int h)
{
    SDL_Rect rect;
    int x1, y1;
    int x_end;
    int y_end;

    x_end = LimitToRange(x + w, 0, TXT_SCREEN_W);
    y_end = LimitToRange(y + h, 0, TXT_SCREEN_H);
    x = LimitToRange(x, 0, TXT_SCREEN_W);
    y = LimitToRange(y, 0, TXT_SCREEN_H);

    for (y1=y; y1<y_end; ++y1)
    {
        for (x1=x; x1<x_end; ++x1)
        {
            UpdateCharacter(x1, y1);
        }
    }

    rect.x = x * font->w;
    rect.y = y * font->h;
    rect.w = (x_end - x) * font->w;
    rect.h = (y_end - y) * font->h;

    SDL_BlitSurface(screenbuffer, &rect, screen, &rect);
    SDL_UpdateRects(screen, 1, &rect);
}

void TXT_UpdateScreen(void)
{
    TXT_UpdateScreenArea(0, 0, TXT_SCREEN_W, TXT_SCREEN_H);
}

static void TXT_GetMousePosition(int *x, int *y)
{
    SDL_GetMouseState(x, y);

    *x /= font->w;
    *y /= font->h;
}

//
// Translates the SDL key
//

static int TranslateKey(SDL_keysym *sym)
{
    switch(sym->sym)
    {
        case SDLK_LEFT:        return KEY_LEFTARROW;
        case SDLK_RIGHT:       return KEY_RIGHTARROW;
        case SDLK_DOWN:        return KEY_DOWNARROW;
        case SDLK_UP:          return KEY_UPARROW;
        case SDLK_ESCAPE:      return KEY_ESCAPE;
        case SDLK_RETURN:      return KEY_ENTER;
        case SDLK_TAB:         return KEY_TAB;
        case SDLK_F1:          return KEY_F1;
        case SDLK_F2:          return KEY_F2;
        case SDLK_F3:          return KEY_F3;
        case SDLK_F4:          return KEY_F4;
        case SDLK_F5:          return KEY_F5;
        case SDLK_F6:          return KEY_F6;
        case SDLK_F7:          return KEY_F7;
        case SDLK_F8:          return KEY_F8;
        case SDLK_F9:          return KEY_F9;
        case SDLK_F10:         return KEY_F10;
        case SDLK_F11:         return KEY_F11;
        case SDLK_F12:         return KEY_F12;
        case SDLK_PRINT:       return KEY_PRTSCR;

        case SDLK_BACKSPACE:   return KEY_BACKSPACE;
        case SDLK_DELETE:      return KEY_DEL;

        case SDLK_PAUSE:       return KEY_PAUSE;

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
                               return KEY_RSHIFT;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
                               return KEY_RCTRL;

        case SDLK_LALT:
        case SDLK_RALT:
        case SDLK_LMETA:
        case SDLK_RMETA:
                               return KEY_RALT;

        case SDLK_CAPSLOCK:    return KEY_CAPSLOCK;
        case SDLK_SCROLLOCK:   return KEY_SCRLCK;

        case SDLK_HOME:        return KEY_HOME;
        case SDLK_INSERT:      return KEY_INS;
        case SDLK_END:         return KEY_END;
        case SDLK_PAGEUP:      return KEY_PGUP;
        case SDLK_PAGEDOWN:    return KEY_PGDN;

		default:               break;
    }

    // Returned value is different, depending on whether key mapping is
    // enabled.  Key mapping is preferable most of the time, for typing
    // in text, etc.  However, when we want to read raw keyboard codes
    // for the setup keyboard configuration dialog, we want the raw
    // key code.

    if (key_mapping)
    {
        // Unicode characters beyond the ASCII range need to be
        // mapped up into textscreen's Unicode range.

        if (sym->unicode < 128)
        {
            return sym->unicode;
        }
        else
        {
            return sym->unicode - 128 + TXT_UNICODE_BASE;
        }
    }
    else
    {
        // Keypad mapping is only done when we want a raw value:
        // most of the time, the keypad should behave as it normally
        // does.

        switch (sym->sym)
        {
            case SDLK_KP0:         return KEYP_0;
            case SDLK_KP1:         return KEYP_1;
            case SDLK_KP2:         return KEYP_2;
            case SDLK_KP3:         return KEYP_3;
            case SDLK_KP4:         return KEYP_4;
            case SDLK_KP5:         return KEYP_5;
            case SDLK_KP6:         return KEYP_6;
            case SDLK_KP7:         return KEYP_7;
            case SDLK_KP8:         return KEYP_8;
            case SDLK_KP9:         return KEYP_9;

            case SDLK_KP_PERIOD:   return KEYP_PERIOD;
            case SDLK_KP_MULTIPLY: return KEYP_MULTIPLY;
            case SDLK_KP_PLUS:     return KEYP_PLUS;
            case SDLK_KP_MINUS:    return KEYP_MINUS;
            case SDLK_KP_DIVIDE:   return KEYP_DIVIDE;
            case SDLK_KP_EQUALS:   return KEYP_EQUALS;
            case SDLK_KP_ENTER:    return KEYP_ENTER;

            default:
                return tolower(sym->sym);
        }
    }
}

// Convert an SDL button index to textscreen button index.
//
// Note special cases because 2 == mid in SDL, 3 == mid in textscreen/setup

static int SDLButtonToTXTButton(int button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:
            return TXT_MOUSE_LEFT;
        case SDL_BUTTON_RIGHT:
            return TXT_MOUSE_RIGHT;
        case SDL_BUTTON_MIDDLE:
            return TXT_MOUSE_MIDDLE;
        default:
            return TXT_MOUSE_BASE + button - 1;
    }
}

static int MouseHasMoved(void)
{
    static int last_x = 0, last_y = 0;
    int x, y;

    TXT_GetMousePosition(&x, &y);

    if (x != last_x || y != last_y)
    {
        last_x = x; last_y = y;
        return 1;
    }
    else
    {
        return 0;
    }
}

// Examine a key press/release and update the modifier key state
// if necessary.

static void UpdateModifierState(SDL_keysym *sym, int pressed)
{
    txt_modifier_t mod;

    switch (sym->sym)
    {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            mod = TXT_MOD_SHIFT;
            break;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
            mod = TXT_MOD_CTRL;
            break;

        case SDLK_LALT:
        case SDLK_RALT:
        case SDLK_LMETA:
        case SDLK_RMETA:
            mod = TXT_MOD_ALT;
            break;

        default:
            return;
    }

    if (pressed)
    {
        ++modifier_state[mod];
    }
    else
    {
        --modifier_state[mod];
    }
}

signed int TXT_GetChar(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
    {
        // If there is an event callback, allow it to intercept this
        // event.

        if (event_callback != NULL)
        {
            if (event_callback(&ev, event_callback_data))
            {
                continue;
            }
        }

        // Process the event.

        switch (ev.type)
        {
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button < TXT_MAX_MOUSE_BUTTONS)
                {
                    return SDLButtonToTXTButton(ev.button.button);
                }
                break;

            case SDL_KEYDOWN:
                UpdateModifierState(&ev.key.keysym, 1);

                return TranslateKey(&ev.key.keysym);

            case SDL_KEYUP:
                UpdateModifierState(&ev.key.keysym, 0);
                break;

            case SDL_QUIT:
                // Quit = escape
                return 27;

            case SDL_MOUSEMOTION:
                if (MouseHasMoved())
                {
                    return 0;
                }

            default:
                break;
        }
    }

    return -1;
}

void TXT_Sleep(int timeout)
{
	// We can just wait forever until an event occurs
	SDL_WaitEvent(NULL);
}

void TXT_SetWindowTitle(char *title)
{
    SDL_WM_SetCaption(title, NULL);
}
