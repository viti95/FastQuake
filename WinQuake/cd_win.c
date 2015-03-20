/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include <windows.h>
#include "quakedef.h"
#include "winquake.h"

extern	HWND	mainwindow;
extern	cvar_t	bgmvolume;
extern qboolean	dsound_init;

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		cdrom;
static byte		playTrack;
static byte		maxTrack;

static LPDIRECTSOUNDBUFFER pDSBufCD;

static void CDAudio_Eject(void)
{
}


static void CDAudio_CloseDoor(void)
{
}


static int CDAudio_GetAudioDiskInfo(void)
{
	cdValid = false;

	//!TODO: get info here

	Con_DPrintf("CDAudio: no music tracks\n");
	return -1;
	
	// cdValid = true;
	// maxTrack = ...;

	// return 0;
}


void CDAudio_Play(byte track, qboolean looping)
{
	if (!enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

	//!TODO: play here

	playLooping = looping;
	playTrack = track;
	playing = true;

	// force volume update
	cdvolume = -1;
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

	if (!enabled)
		return;
	
	if (!playing)
		return;

	//!TODO: stop here

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	if (!enabled)
		return;

	if (!playing)
		return;

	//!TODO: pause here

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	if (!enabled)
		return;
	
	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	//!TODO: resume here

	playing = true;
}


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;
	int		startAddress;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (Q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));
		return;
	}

	if (Q_strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

/*
	//!TODO: 
	when track finishes, execute this:
		if (playing)
		{
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
*/

void CDAudio_Update(void)
{
	if (!enabled)
		return;

	if (bgmvolume.value != cdvolume)
	{
		LONG max_attenuation = 4000; // 40 dB
		LONG attenuation;

		cdvolume = bgmvolume.value;
		if (cdvolume < 0.01)
			attenuation = DSBVOLUME_MIN;
		else
			attenuation = -max_attenuation + cdvolume*max_attenuation;
		pDSBufCD->lpVtbl->SetVolume(pDSBufCD, attenuation);
	}
}


int CDAudio_Init(void)
{
	WAVEFORMATEX	format;
	DSBUFFERDESC	dsbuf;
	DWORD			dwSize;
	char*			lpData;
	int				n;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio"))
		return -1;

	if (!dsound_init)
	{
		Con_Printf("CDAudio_Init: DirectSound not initialized.\n");
		return -1;
	}

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.wBitsPerSample = 16;
    format.nSamplesPerSec = 44100;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_CTRLVOLUME;
	dsbuf.dwBufferBytes = format.nAvgBytesPerSec;
	dsbuf.lpwfxFormat = &format;

	if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBufCD, NULL))
	{
		Con_Printf("CDAudio_Init: CreateSoundBuffer failed.\n");
		return -1;
	}

	if (DS_OK != pDSBufCD->lpVtbl->Lock(pDSBufCD, 0, 0, &lpData, &dwSize, NULL, NULL, DSBLOCK_ENTIREBUFFER))
	{
		Con_Printf("CDAudio_Init: Lock sound buffer failed.\n");
		pDSBuf->lpVtbl->Release(pDSBuf);
		return -1;
	}

	memset (lpData, 0, dwSize);
	//lpData[1] = 0x7F;
	//cdvolume = -1;

	pDSBufCD->lpVtbl->Unlock(pDSBufCD, lpData, dwSize, NULL, 0);

	if (DS_OK != pDSBufCD->lpVtbl->Play(pDSBufCD, 0, 0, DSBPLAY_LOOPING))
	{
		Con_Printf("CDAudio_Init: Play sound buffer failed.\n");
		pDSBuf->lpVtbl->Release(pDSBuf);
		return -1;
	}

	for (n = 0; n < 100; n++)
		remap[n] = n;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("CDAudio_Init: No tracks found.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	
	if (pDSBufCD)
	{
		pDSBufCD->lpVtbl->Stop(pDSBufCD);
		pDSBufCD->lpVtbl->Release(pDSBufCD);
	}
	pDSBufCD = NULL;
}
