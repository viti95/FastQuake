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

/*static void I_InitWindowTitle(void)
{
    char *buf;

    //buf = M_StringJoin(window_title, " - ", PACKAGE_STRING, NULL);
	buf = "PLACEHOLDER";
    TXT_SetWindowTitle(buf);
    //free(buf);
}*/

// 
// Displays the text mode ending screen after the game quits
//
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

    TXT_Init("PLACEHOLDER", screen);
}
