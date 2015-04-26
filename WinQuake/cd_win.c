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
#include <process.h>
#include "quakedef.h"
#include "winquake.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

extern	HWND	mainwindow;
extern	cvar_t	bgmvolume;
extern qboolean	dsound_init;

static qboolean cdValid = false;
static volatile qboolean playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static volatile qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		cdrom;
static volatile byte playTrack;
static byte		maxTrack;

static LPDIRECTSOUNDBUFFER pDSBufCD;
static LPDIRECTSOUNDNOTIFY pDSBufCDNotify;
static DWORD gCDBufSize;
static HANDLE cdStopEvent, cdPlayingFinishedEvent;

struct ThreadArgList_t
{
	byte playTrack;
	qboolean playLooping;

	OggVorbis_File vf;
	char oggb[4096];
	unsigned oggb_first;
	unsigned oggb_count;
};

typedef qboolean (*playcallback_t)(struct ThreadArgList_t *playData, char *ptr, DWORD len);
typedef void (*finishnotify_t)(struct ThreadArgList_t *playData);

qboolean OpenOGG(const char *filename, struct ThreadArgList_t *ud)
{
	FILE *file;
	file = fopen(filename, "rb");
	if (file == NULL)
		return false;

	if(ov_open_callbacks(file, &ud->vf, NULL, 0, OV_CALLBACKS_DEFAULT) < 0)
	{
		fclose(file);
		return true;
	}

	ud->oggb_count = 0;
	return true;
}


void CloseOGG(struct ThreadArgList_t *ud)
{
	ov_clear(&ud->vf);
}

static qboolean PaintSoundOGG(struct ThreadArgList_t *ud, char *ptr, DWORD len)
{
	int dummy;
	unsigned remaining = len;
	unsigned OGGBUFFER_SIZE = ARRAYSIZE(ud->oggb);
	unsigned read;

	unsigned tocopy;
	if (ud->oggb_count > 0)
	{
		tocopy = min(len, ud->oggb_count);
		memcpy(ptr, ud->oggb + ud->oggb_first, tocopy);
		ptr += tocopy;
		remaining -= tocopy;
		ud->oggb_first += tocopy;
		ud->oggb_count -= tocopy;
	}
	while (remaining >= OGGBUFFER_SIZE)
	{
		read = ov_read(&ud->vf, ptr, OGGBUFFER_SIZE, 0, 2, 1, &dummy);
		if (read == 0)
		{
			memset(ptr, 0, remaining);
			return false;
		}
		remaining -= read;
		ptr += read;
	}
	while (remaining > 0)
	{
		read = ov_read(&ud->vf, ud->oggb, OGGBUFFER_SIZE, 0, 2, 1, &dummy);
		if (read == 0)
		{
			memset(ptr, 0, remaining);
			return false;
		}
		tocopy = min(read, remaining);
		memcpy(ptr, ud->oggb, tocopy);
		remaining -= tocopy;
		ptr += tocopy;
		ud->oggb_count = read - tocopy;
		ud->oggb_first = tocopy;
	}
	return true;
}

static void CDAudio_Eject(void)
{
}


static void CDAudio_CloseDoor(void)
{
}

static int CDAudio_GetAudioDiskInfo(void)
{
	int i;

	cdValid = false;

	maxTrack = 0;

	// CDDA track numbers are in range of 1..99
	for (i=1; i<100; i++)
	{
		FILE *f;
		char filename[MAX_PATH];
		sprintf(filename, "%s\\Track%03d.ogg", com_gamedir, i);
		f = fopen(filename, "rb");
		if (!f)
		{
			sprintf(filename, "%s\\sound\\cdtracks\\Track%03d.ogg", com_gamedir, i);
			f = fopen(filename, "rb");
		}
		if (f)
		{
			maxTrack = i;
			remap[i] = i;
			fclose(f);
		}
		else
		{
			remap[i] = -1;
			// track 1 is allowed not to exist
			if (i > 1)
				break;
		}
	}

	if (maxTrack == 0)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	return 0;
}

void CDAudio_WaitForFinish(void)
{
	WaitForSingleObject(cdPlayingFinishedEvent, INFINITE);
}

#define BUFFER_PARTS 8

static unsigned int __stdcall PlayingThreadProc(void *arglist)
{
	struct ThreadArgList_t *tal = arglist;

	DSBPOSITIONNOTIFY notifies[BUFFER_PARTS];
	HANDLE events[BUFFER_PARTS+1]; // +1 for StopEvent
	HANDLE endevents[2];
	char *ptr;
	int i;
	DWORD dummy, fillpart;
	DWORD part_size;
	HRESULT hr;
	qboolean more_data;
	qboolean trackPlayedToEnd = false;

	CDAudio_WaitForFinish();
	ResetEvent(cdPlayingFinishedEvent);
	ResetEvent(cdStopEvent);

	part_size = gCDBufSize / BUFFER_PARTS;

	for (i=0; i<BUFFER_PARTS; i++)
	{
		events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		notifies[i].dwOffset = i * part_size;
		notifies[i].hEventNotify = events[i];
	}
	events[i] = cdStopEvent;

	pDSBufCD->lpVtbl->GetStatus(pDSBufCD, &dummy);
	if (dummy & DSBSTATUS_BUFFERLOST)
		pDSBufCD->lpVtbl->Restore(pDSBufCD);

	pDSBufCDNotify->lpVtbl->SetNotificationPositions(pDSBufCDNotify, BUFFER_PARTS, notifies);

	if (FAILED(pDSBufCD->lpVtbl->Lock(pDSBufCD, 0, 0, &ptr, &dummy, NULL, NULL, DSBLOCK_ENTIREBUFFER)))
	{
		Con_DPrintf("CDAudio: cannot lock sound buffer.\n");
		return -1;
	}
	
	if (!PaintSoundOGG(tal, ptr, (BUFFER_PARTS-1)*part_size))
		SetEvent(cdStopEvent);

	pDSBufCD->lpVtbl->Unlock(pDSBufCD, ptr, dummy, NULL, 0);

	pDSBufCD->lpVtbl->SetCurrentPosition(pDSBufCD, 0);
	pDSBufCD->lpVtbl->Play(pDSBufCD, 0, 0, DSBPLAY_LOOPING);

	while(true)
	{
		fillpart = WaitForMultipleObjects(BUFFER_PARTS+1, events, FALSE, 2000);
		if (fillpart >= WAIT_OBJECT_0 && fillpart < WAIT_OBJECT_0 + BUFFER_PARTS)
		{
			fillpart = (fillpart - WAIT_OBJECT_0 + BUFFER_PARTS - 1) % BUFFER_PARTS;
			hr = pDSBufCD->lpVtbl->Lock(pDSBufCD, part_size * fillpart, part_size, &ptr, &dummy, NULL, NULL, 0);
			if (hr == DSERR_BUFFERLOST)
			{
				pDSBufCD->lpVtbl->Restore(pDSBufCD);
				pDSBufCD->lpVtbl->Lock(pDSBufCD, part_size * fillpart, part_size, &ptr, &dummy, NULL, NULL, 0);
			}
			more_data = PaintSoundOGG(tal, ptr, part_size);
			pDSBufCD->lpVtbl->Unlock(pDSBufCD, ptr, dummy, NULL, 0);
			if (!more_data)
			{
				endevents[0] = events[fillpart];
				endevents[1] = cdStopEvent;
				if (WaitForMultipleObjects(2, endevents, FALSE, 2000) == WAIT_OBJECT_0)
					trackPlayedToEnd = true;
				break;
			}
		}
		else
			break;
	}

	pDSBufCD->lpVtbl->Stop(pDSBufCD);
	pDSBufCDNotify->lpVtbl->SetNotificationPositions(pDSBufCDNotify, 0, NULL);
	for (i=0; i<BUFFER_PARTS; i++)
		CloseHandle(events[i]);
	
	CloseOGG(tal);

	playing = false;
	free(arglist);

	SetEvent(cdPlayingFinishedEvent);

	if (trackPlayedToEnd && playLooping)
		CDAudio_Play(playTrack, true);	

	return 0;
}

void CDAudio_Play(byte track, qboolean looping)
{
	HANDLE playingThread;
	char filename[MAX_PATH];
	struct ThreadArgList_t *tal;

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

	tal = malloc(sizeof(struct ThreadArgList_t));
	tal->playLooping = looping;
	tal->playTrack = track;

	sprintf(filename, "%s\\Track%03d.ogg", com_gamedir, track);
	if (!OpenOGG(filename, tal))
	{
		sprintf(filename, "%s\\sound\\cdtracks\\Track%03d.ogg", com_gamedir, track);
		if (!OpenOGG(filename, tal))
		{
			Con_DPrintf("CDAudio: Cannot open Vorbis file \"%s\"", filename);
			return;
		}
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	// force volume update
	cdvolume = -1;

	playingThread = (HANDLE)_beginthreadex(NULL, 0, PlayingThreadProc, tal, CREATE_SUSPENDED, NULL);
	SetThreadPriority(playingThread, THREAD_PRIORITY_TIME_CRITICAL);
	ResumeThread(playingThread);
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

	if (!enabled)
		return;
	
	if (!playing)
		return;

	SetEvent(cdStopEvent);

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
	dsbuf.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY;
	dsbuf.dwBufferBytes = format.nAvgBytesPerSec;
	dsbuf.lpwfxFormat = &format;

	if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBufCD, NULL))
	{
		Con_Printf("CDAudio_Init: CreateSoundBuffer failed.\n");
		return -1;
	}

	pDSBufCD->lpVtbl->QueryInterface(pDSBufCD, &IID_IDirectSoundNotify, &pDSBufCDNotify);

	gCDBufSize = dsbuf.dwBufferBytes;

	cdPlayingFinishedEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	cdStopEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

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
	CDAudio_WaitForFinish();
	
	if (pDSBufCDNotify)
	{
		pDSBufCDNotify->lpVtbl->Release(pDSBufCDNotify);
		pDSBufCDNotify = NULL;
	}
	if (pDSBufCD)
	{
		pDSBufCD->lpVtbl->Stop(pDSBufCD);
		pDSBufCD->lpVtbl->Release(pDSBufCD);
		pDSBufCD = NULL;
	}
	if (cdPlayingFinishedEvent)
	{
		CloseHandle(cdPlayingFinishedEvent);
		cdPlayingFinishedEvent = NULL;
	}
	if (cdStopEvent)
	{
		CloseHandle(cdStopEvent);
		cdStopEvent = NULL;
	}
}
