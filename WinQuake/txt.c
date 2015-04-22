//
// Copyright(C) 2005-2014 Simon Howard
// Copyright (C) 1996-1997 Id Software, Inc.
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

#include <stdio.h>
#include <string.h>

#include "quakedef.h"
#include "winquake.h"
#include "txt.h"

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

static void I_InitWindowTitle(void)
{
    char *buf;

    //buf = M_StringJoin(window_title, " - ", PACKAGE_STRING, NULL);
	buf = "PLACEHOLDER";
    TXT_SetWindowTitle(buf);
    //free(buf);
}

// Set the application icon

static void I_InitWindowIcon(void)
{
	/*
    SDL_Surface *surface;
    Uint8 *mask;
    int i;

    // Generate the mask

    mask = malloc(icon_w * icon_h / 8);
    memset(mask, 0, icon_w * icon_h / 8);

    for (i=0; i<icon_w * icon_h; ++i)
    {
        if (icon_data[i * 3] != 0x00
         || icon_data[i * 3 + 1] != 0x00
         || icon_data[i * 3 + 2] != 0x00)
        {
            mask[i / 8] |= 1 << (7 - i % 8);
        }
    }

    surface = SDL_CreateRGBSurfaceFrom(icon_data,
                                       icon_w,
                                       icon_h,
                                       24,
                                       icon_w * 3,
                                       0xff << 0,
                                       0xff << 8,
                                       0xff << 16,
                                       0);

    SDL_WM_SetIcon(surface, mask);
    SDL_FreeSurface(surface);
    free(mask);
	*/
}


#define ENDOOM_W 80
#define ENDOOM_H 25

// 
// Displays the text mode ending screen after the game quits
//

static void I_Endoom(byte *endoom_data)
{
    byte *screendata;
    int y;
    int indent;

    // Set up text mode screen

    TXT_Init();
    I_InitWindowTitle();
    I_InitWindowIcon();

    // Write the data to the screen memory

    screendata = TXT_GetScreenData();

    indent = (ENDOOM_W - TXT_SCREEN_W) / 2;

    for (y=0; y<TXT_SCREEN_H; ++y)
    {
        memcpy(screendata + (y * TXT_SCREEN_W * 2),
               endoom_data + (y * ENDOOM_W + indent) * 2,
               TXT_SCREEN_W * 2);
    }

    // Wait for a keypress

	TXT_UpdateScreen();
	TXT_WaitForChar();

    // Shut down text mode screen

    TXT_Shutdown();
}


void D_Endoom(void)
{
	byte	screen[80*25*2];
	byte	*d;
	char			ver[6];
	int			i;

    // Don't show ENDOOM if we have it disabled, or we're running
    // in screensaver or control test mode. Only show it once the
    // game has actually started.

    /*
	if (!show_endoom || !main_loop_started
     || screensaver_mode || M_CheckParm("-testcontrols") > 0)
    {
        return;
    }
	*/

	// load the sell screen
	if (registered.value)
		d = COM_LoadHunkFile ("end2.bin"); 
	else
		d = COM_LoadHunkFile ("end1.bin"); 
	if (d)
		memcpy (screen, d, sizeof(screen));

	// write the version number directly to the end screen
		sprintf (ver, " v%4.2f", VERSION);
		for (i=0 ; i<6 ; i++)
			screen[0*80*2 + 72*2 + i*2] = ver[i];

    I_Endoom(d);
}
