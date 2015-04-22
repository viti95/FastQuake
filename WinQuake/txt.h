//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//    Exit text-mode ENDOOM screen.
//


#ifndef __I_ENDOOM__
#define __I_ENDOOM__

// Display the Endoom screen on shutdown.
void D_Endoom(void);

// textscreen key values:
// Key values are difficult because we have to support multiple conflicting
// address spaces.
// First, Doom's key constants use 0-127 as ASCII and extra values from
// 128-255 to represent special keys. Second, mouse buttons are represented
// as buttons. Finally, we want to be able to support Unicode.
//
// So we define different ranges:
// 0-255:    Doom key constants, including ASCII.
// 256-511:  Mouse buttons and other reserved.
// >=512:    Unicode values greater than 127 are offset up into this range.

// Special keypress values that correspond to mouse button clicks

#define TXT_MOUSE_BASE         256
#define TXT_MOUSE_LEFT         (TXT_MOUSE_BASE + 0)
#define TXT_MOUSE_RIGHT        (TXT_MOUSE_BASE + 1)
#define TXT_MOUSE_MIDDLE       (TXT_MOUSE_BASE + 2)
#define TXT_MAX_MOUSE_BUTTONS  16

// Unicode offset. Unicode values from 128 onwards are offset up into
// this range, so TXT_UNICODE_BASE = Unicode character #128, and so on.

#define TXT_UNICODE_BASE       512

// Screen size

#define TXT_SCREEN_W 80
#define TXT_SCREEN_H 25

// Modifier keys.

typedef enum
{
    TXT_MOD_SHIFT,
    TXT_MOD_CTRL,
    TXT_MOD_ALT,
    TXT_NUM_MODIFIERS
} txt_modifier_t;

// Initialize the screen
// Returns 1 if successful, 0 if failed.
int TXT_Init(void);

// Shut down text mode emulation
void TXT_Shutdown(void);

// Get a pointer to the buffer containing the raw screen data.
unsigned char *TXT_GetScreenData(void);

// Update the whole screen
void TXT_UpdateScreen(void);

// Read a character from the keyboard
void TXT_WaitForChar(void);

// Set the window title of the window containing the text mode screen
void TXT_SetWindowTitle(char *title);

#endif

