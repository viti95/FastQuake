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
#include "quakedef.h"
#include "winquake.h"

// 64K is > 1 second at 16-bit, 22050 Hz
#define SECONDARY_BUFFER_SIZE	0x10000

qboolean	dsound_init;
static qboolean	snd_firsttime = true;

static int	sample16;


/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 

HPSTR		lpData;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf;

qboolean SNDDMA_InitDirect (void);


/*
==================
S_BlockSound
==================
*/
void S_BlockSound (void)
{

// DirectSound takes care of blocking itself
}


/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound (void)
{

// DirectSound takes care of blocking itself
}


/*
==================
FreeSound
==================
*/
void FreeSound (void)
{
	int		i;

	if (pDSBuf)
	{
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

	if (pDS)
	{
		pDS->lpVtbl->SetCooperativeLevel (pDS, mainwindow, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	pDS = NULL;
	pDSBuf = NULL;
	lpData = NULL;
	dsound_init = false;
}


/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
qboolean SNDDMA_InitDirect (void)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	WAVEFORMATEX	format, pformat; 
	HRESULT			hresult;
	int				reps;

	memset ((void *)&sn, 0, sizeof (sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;

	if (DirectSoundCreate(NULL, &pDS, NULL) != DS_OK)
	{
		Con_SafePrintf ("DirectSound create failed\n");
		return false;
	}

	if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, mainwindow, DSSCL_PRIORITY))
	{
		Con_SafePrintf ("Set coop level failed\n");
		FreeSound ();
		return false;
	}

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = shm->channels;
    format.wBitsPerSample = shm->samplebits;
    format.nSamplesPerSec = shm->speed;
    format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign; 

	// create the secondary buffer we'll actually work with
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
	dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
	dsbuf.lpwfxFormat = &format;

	if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
	{
		Con_SafePrintf ("DS:CreateSoundBuffer Failed");
		FreeSound ();
		return false;
	}

	shm->channels = format.nChannels;
	shm->samplebits = format.wBitsPerSample;
	shm->speed = format.nSamplesPerSec;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);

	if (DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps))
	{
		Con_SafePrintf ("DS:GetCaps failed\n");
		FreeSound ();
		return false;
	}

	if (snd_firsttime)
		Con_SafePrintf ("Using secondary sound buffer\n");

	// Make sure mixer is active
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Con_SafePrintf("   %d channel(s)\n"
		               "   %d bits/sample\n"
					   "   %d bytes/sec\n",
					   shm->channels, shm->samplebits, shm->speed);
	
	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound ();
			return false;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound ();
			return false;
		}

	}

	memset(lpData, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL; 

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	dsound_init = true;

	return true;
}


/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

int SNDDMA_Init(void)
{
	qboolean	stat;

	dsound_init = 0;

	stat = false;	// assume DirectSound won't initialize

	/* Init DirectSound */
	stat = SNDDMA_InitDirect ();;

	if (stat)
	{
		if (snd_firsttime)
			Con_SafePrintf ("DirectSound initialized\n");
	}
	else
		Con_SafePrintf ("DirectSound failed to init\n");

	snd_firsttime = false;

	if (!dsound_init)
	{
		if (snd_firsttime)
			Con_SafePrintf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	if (dsound_init) 
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}


	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound ();
}

