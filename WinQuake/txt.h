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

// Screen size

#define TXT_SCREEN_W 80
#define TXT_SCREEN_H 25

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

