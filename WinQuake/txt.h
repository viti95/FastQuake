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

void TXT_Init(const char *title, byte *ascreendata);

// Get a pointer to the buffer containing the raw screen data.
unsigned char *TXT_GetScreenData(void);

void TXT_Show(void);

#endif

