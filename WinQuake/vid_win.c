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
// vid_win.c -- Win32 video driver

#include "quakedef.h"
#include "winquake.h"
#include "d_local.h"
#include "resource.h"

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3

extern int		Minimized;

HWND		mainwindow;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyleFramed = WS_OVERLAPPED | WS_BORDER | WS_CAPTION |
								WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
								WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

DWORD		WindowStyleFullscreen = WS_POPUP;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

static qboolean	startwindowed = 0, windowed_mode_set;
static int		firstupdate = 1;
static qboolean	vid_initialized = false, vid_palettized;
static int		lockcount;
static qboolean	force_minimized, in_mode_set, force_mode_set;
static int		windowed_mouse;
static qboolean	palette_changed, hide_window;
HICON			g_hIcon;

viddef_t	vid;				// global video state

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 100)

// Note that 0 is MODE_WINDOWED
cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		_windowed_mouse = {"_windowed_mouse","0", true};
cvar_t		vid_fullscreen_mode = {"vid_fullscreen_mode","3", true};
cvar_t		vid_windowed_mode = {"vid_windowed_mode","0", true};
cvar_t		vid_window_x = {"vid_window_x", "0", true};
cvar_t		vid_window_y = {"vid_window_y", "0", true};

typedef struct {
	int		width;
	int		height;
} lmode_t;

int			vid_modenum = NO_MODE;
int			vid_testingmode, vid_realmode;
double		vid_testendtime;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;

modestate_t	modestate = MS_UNINIT;

static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;

unsigned char	vid_curpal[256*3];
unsigned char	vid_fakepal[256*4];

int     driver,mode;

IDirectDraw7 *lpDirectDraw;
IDirectDrawSurface7 *lpddsFrontBuffer, *lpddsBackBuffer;
byte *lpOffScreenBuffer;

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[13];
} vmode_t;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;

int		waitVRT = true;			// True to wait for retrace on flip

static vmode_t	badmode;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);


/*
================
VID_RememberWindowPos
================
*/
void VID_RememberWindowPos (void)
{
	RECT	rect;

	if (GetWindowRect (mainwindow, &rect))
	{
		if ((rect.left < GetSystemMetrics (SM_CXSCREEN)) &&
			(rect.top < GetSystemMetrics (SM_CYSCREEN))  &&
			(rect.right > 0)                             &&
			(rect.bottom > 0))
		{
			Cvar_SetValue ("vid_window_x", (float)rect.left);
			Cvar_SetValue ("vid_window_y", (float)rect.top);
		}
	}
}


/*
================
VID_CheckWindowXY
================
*/
void VID_CheckWindowXY (void)
{

	if (((int)vid_window_x.value > (GetSystemMetrics (SM_CXSCREEN) - 160)) ||
		((int)vid_window_y.value > (GetSystemMetrics (SM_CYSCREEN) - 120)) ||
		((int)vid_window_x.value < 0)									   ||
		((int)vid_window_y.value < 0))
	{
		Cvar_SetValue ("vid_window_x", 0.0);
		Cvar_SetValue ("vid_window_y", 0.0 );
	}
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}


/*
================
VID_AllocBuffers
================
*/
qboolean VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tsize = D_SurfaceCacheForRes (width, height);

	tbuffersize += tsize;

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	if ((host_parms.memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
		 0x10000 * 3) < minimum_memory)
	{
		Con_SafePrintf ("Not enough memory for video mode\n");
		return false;		// not enough memory for mode
	}

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");

	vid_surfcache = (byte *)d_pzbuffer +
			width * height * sizeof (*d_pzbuffer);
	
	return true;
}


void initFatalError(void)
{
	exit(EXIT_FAILURE);
}

int VID_Suspend (int flags)
{
#if 0
// todo: fix me
	if (flags & MGL_DEACTIVATE)
	{
		block_drawing = true;	// so we don't try to draw while switched away
	}
	else if (flags & MGL_REACTIVATE)
	{
		block_drawing = false;
	}
	
	return MGL_NO_SUSPEND_APP;
#endif
	return 0;
}


void VID_InitFullScreenModes (HINSTANCE hInstance)
{
	struct {int w; int h; } resolutions[] =
	{{320, 240}, {400, 300}, {640, 480}, {800, 600}, {1024,768}};

	int i;
	for (i=0; i<ARRAYSIZE(resolutions); i++)
	{
		modelist[nummodes].type = MS_FULLSCREEN;
		modelist[nummodes].width = resolutions[i].w;
		modelist[nummodes].height = resolutions[i].h;
		sprintf(modelist[nummodes].modedesc, "%dx%d", resolutions[i].w, resolutions[i].h);
		modelist[nummodes].modenum = MODE_FULLSCREEN_DEFAULT + i;
		modelist[nummodes].dib = 0;
		modelist[nummodes].fullscreen = 1;
		modelist[nummodes].halfscreen = 0;
		modelist[nummodes].bpp = 8;
		nummodes++;
	}

#if 0
	int			i, xRes, yRes, bits, lowres, curmode, temp;
    unsigned char		*m;

	// Initialise the MGL
	FakeMGL_FULL_registerDriver();
	FakeMGL_FULL_detectGraph(&driver,&mode);
	m = FakeMGL_FULL_availableModes();

	if (m[0] != 0xFF)
	{ 
		lowres = 99999;
		curmode = 0;

	// find the lowest-res mode
		for (i = 0; m[i] != 0xFF; i++)
		{
			FakeMGL_FULL_modeResolution(m[i], &xRes, &yRes,&bits);

			if ((bits == 8) &&
				(xRes <= MAXWIDTH) &&
				(yRes <= MAXHEIGHT) &&
				(curmode < MAX_MODE_LIST))
			{
				if (xRes < lowres)
				{
					lowres = xRes;
					mode = i;
				}
			}

			curmode++;
		}

	// build the mode list
		nummodes++;		// leave room for default mode

		for (i = 0; m[i] != 0xFF; i++)
		{
			FakeMGL_FULL_modeResolution(m[i], &xRes, &yRes,&bits);

			if ((bits == 8) &&
				(xRes <= MAXWIDTH) &&
				(yRes <= MAXHEIGHT) &&
				(nummodes < MAX_MODE_LIST))
			{
				if (i == mode)
				{
					curmode = MODE_FULLSCREEN_DEFAULT;
				}
				else
				{
					curmode = nummodes++;
				}

				modelist[curmode].type = MS_FULLSCREEN;
				modelist[curmode].width = xRes;
				modelist[curmode].height = yRes;
				sprintf (modelist[curmode].modedesc, "%dx%d", xRes, yRes);
				modelist[curmode].modenum = m[i];
				modelist[curmode].dib = 0;
				modelist[curmode].fullscreen = 1;
				modelist[curmode].halfscreen = 0;
				modelist[curmode].bpp = 8;
			}
		}

		temp = m[0];

		if (!FakeMGL_FULL_init(&driver, &temp))
		{
			initFatalError();
		}
	}

	FakeMGL_FULL_setSuspendAppCallback(VID_Suspend);
#endif
}


void VID_InitWindowedModes (HINSTANCE hInstance)
{
	struct {int w; int h; } resolutions[] =
	{{320, 240}, /*{400, 300},*/ {640, 480}, {800, 600}, /*{1024,768}*/};

	int i;
	for (i=0; i<ARRAYSIZE(resolutions); i++)
	{
		modelist[nummodes].type = MS_WINDOWED;
		modelist[nummodes].width = resolutions[i].w;
		modelist[nummodes].height = resolutions[i].h;
		sprintf(modelist[nummodes].modedesc, "%dx%d", resolutions[i].w, resolutions[i].h);
		modelist[nummodes].modenum = MODE_WINDOWED + i;
		modelist[nummodes].dib = 0;
		modelist[nummodes].fullscreen = 0;
		modelist[nummodes].halfscreen = 0;
		modelist[nummodes].bpp = 8;
		nummodes++;
	}

	windowed_default = MODE_WINDOWED;
	vid_default = windowed_default;
}


/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}

	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescriptionMemCheck
=================
*/
char *VID_GetModeDescriptionMemCheck (int mode)
{
	char		*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;

	return pinfo;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;
	return pinfo;
}


/*
=================
VID_GetModeDescription2

Tacks on "windowed" or "fullscreen"
=================
*/
char *VID_GetModeDescription2 (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLSCREEN)
	{
		sprintf(pinfo,"%s fullscreen", pv->modedesc);
	}
	else
	{
		sprintf(pinfo, "%s windowed", pv->modedesc);
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLSCREEN)
	{
		sprintf(pinfo,"%s fullscreen ddraw",pv->modedesc);
	}
	else
	{
		sprintf(pinfo, "%s windowed ddraw", pv->modedesc);
	}

	return pinfo;
}

void DestroyVideoMode (void)
{
	if (lpOffScreenBuffer)
	{
		free(lpOffScreenBuffer);
		lpOffScreenBuffer = NULL;
	}
	if (modestate == MS_WINDOWED && lpddsBackBuffer)
	{
		IDirectDrawSurface7_Release(lpddsBackBuffer);
		lpddsBackBuffer = NULL;
	}
	if (lpddsFrontBuffer)
	{
		IDirectDrawSurface7_Release(lpddsFrontBuffer);
		lpddsFrontBuffer = NULL;
	}
	ChangeDisplaySettings(NULL, 0);
	modestate = MS_UNINIT;
}

qboolean VID_SetWindowedMode (int modenum)
{
	IDirectDrawClipper *lpddClipper;
	DDSURFACEDESC2	ddsd;
	HRESULT			hr;
	int				lastmodestate;

	if (!windowed_mode_set)
	{
		if (COM_CheckParm ("-resetwinpos"))
		{
			Cvar_SetValue ("vid_window_x", 0.0);
			Cvar_SetValue ("vid_window_y", 0.0);
		}

		windowed_mode_set;
	}

	lastmodestate = modestate;

	DestroyVideoMode ();
	hr = IDirectDraw7_SetCooperativeLevel(lpDirectDraw, mainwindow, DDSCL_NORMAL);

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;
	
	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	AdjustWindowRectEx(&WindowRect, WindowStyleFramed, FALSE, 0);

	if (hide_window)
		return true;

// position and show the DIB window
	VID_CheckWindowXY ();
	SetWindowPos (mainwindow,
				  HWND_TOP,
				  (int)vid_window_x.value,
				  (int)vid_window_y.value,
				  WindowRect.right - WindowRect.left,
				  WindowRect.bottom - WindowRect.top,
				  SWP_SHOWWINDOW | SWP_DRAWFRAME | SWP_NOCOPYBITS);

	SetWindowLong(mainwindow, GWL_STYLE, WindowStyleFramed);

	if (force_minimized)
		ShowWindow (mainwindow, SW_MINIMIZE);
	else
		ShowWindow (mainwindow, SW_SHOWDEFAULT);

	modestate = MS_WINDOWED;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	hr = IDirectDraw7_CreateSurface(lpDirectDraw, &ddsd, &lpddsFrontBuffer, NULL);

	hr = IDirectDraw7_CreateClipper(lpDirectDraw, 0, &lpddClipper, NULL);
	hr = IDirectDrawClipper_SetHWnd(lpddClipper, 0, mainwindow);
	hr = IDirectDrawSurface7_SetClipper(lpddsFrontBuffer, lpddClipper);
	hr = IDirectDrawClipper_Release(lpddClipper);

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = DIBWidth;
	ddsd.dwHeight = DIBHeight;
	hr = IDirectDraw7_CreateSurface(lpDirectDraw, &ddsd, &lpddsBackBuffer, NULL);

	lpOffScreenBuffer = malloc(DIBWidth * DIBHeight);
	memset(lpOffScreenBuffer, 255, DIBWidth * DIBHeight);

	vid.buffer = vid.conbuffer = vid.direct = NULL;
	vid.rowbytes = vid.conrowbytes = 0;
	vid.numpages = 1;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.height = vid.conheight = DIBHeight;
	vid.width = vid.conwidth = DIBWidth;
	vid.aspect = ((float)vid.height / (float)vid.width) *
				(320.0 / 240.0);

	return true;
}


qboolean VID_SetFullscreenMode (int modenum)
{
	DDSURFACEDESC2	ddsd;
	DDSCAPS2		caps;
	HRESULT			hr;
	int				w, h;
	DEVMODE			dm;

	DestroyVideoMode ();

	w = modelist[modenum].width;
	h = modelist[modenum].height;

	ZeroMemory(&dm, sizeof(dm));
	dm.dmSize = sizeof(dm);
	dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	dm.dmBitsPerPel = 32;
	dm.dmPelsWidth = w;
	dm.dmPelsHeight = h;
	ChangeDisplaySettings(&dm, CDS_FULLSCREEN);

	hr = IDirectDraw7_SetCooperativeLevel(lpDirectDraw, mainwindow, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT);

	SetWindowPos (
		mainwindow,
		HWND_TOP,
		0, 0,
		w, h,
		SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetWindowLong(mainwindow, GWL_STYLE, WindowStyleFullscreen);

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
	ddsd.dwBackBufferCount = 1;
	hr = IDirectDraw7_CreateSurface(lpDirectDraw, &ddsd, &lpddsFrontBuffer, NULL);

	memset(&caps, 0, sizeof(caps));
	caps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = IDirectDrawSurface7_GetAttachedSurface(lpddsFrontBuffer, &caps, &lpddsBackBuffer);

	mode = modelist[modenum].modenum;
	vid.numpages = 2;
	waitVRT = true;
	modestate = MS_FULLSCREEN;

	vid.buffer = vid.conbuffer = vid.direct = NULL;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	DIBHeight = vid.height = vid.conheight = modelist[modenum].height;
	DIBWidth = vid.width = vid.conwidth = modelist[modenum].width;
	vid.aspect = ((float)vid.height / (float)vid.width) *
				(320.0 / 240.0);

	lpOffScreenBuffer = malloc(DIBWidth * DIBHeight);
	memset(lpOffScreenBuffer, 255, DIBWidth * DIBHeight);

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

// shouldn't be needed, but Kendall needs to let us get the activation
// message for this not to be needed on NT
	AppActivate (true, false);

	return true;
}


void VID_RestoreOldMode (int original_mode)
{
	in_mode_set = false;

// make sure mode set happens (video mode changes)
	vid_modenum = original_mode - 1;

	if (!VID_SetMode (original_mode, vid_curpal))
	{
		vid_modenum = MODE_WINDOWED - 1;

		if (!VID_SetMode (windowed_default, vid_curpal))
			Sys_Error ("Can't set any video mode");
	}
}


void VID_SetDefaultMode (void)
{

	if (vid_initialized)
		VID_SetMode (0, vid_curpal);

	IN_DeactivateMouse ();
}


int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qboolean		stat;
    MSG				msg;
	HDC				hdc;

	while ((modenum >= nummodes) || (modenum < 0))
	{
		if (vid_modenum == NO_MODE)
		{
			if (modenum == vid_default)
			{
				modenum = windowed_default;
			}
			else
			{
				modenum = vid_default;
			}

			Cvar_SetValue ("vid_mode", (float)modenum);
		}
		else
		{
			Cvar_SetValue ("vid_mode", (float)vid_modenum);
			return 0;
		}
	}

	if (!force_mode_set && (modenum == vid_modenum))
		return true;

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	in_mode_set = true;

	CDAudio_Pause ();
	S_ClearBuffer ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	}
	else
	{
		stat = VID_SetFullscreenMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}

	window_width = vid.width;
	window_height = vid.height;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		VID_RestoreOldMode (original_mode);
		return false;
	}

	if (hide_window)
		return true;

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	if (!force_minimized)
		SetForegroundWindow (mainwindow);

	hdc = GetDC(NULL);

	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	if (!VID_AllocBuffers (vid.width, vid.height))
	{
	// couldn't get memory for this mode; try to fall back to previous mode
		VID_RestoreOldMode (original_mode);
		return false;
	}

	D_InitCaches (vid_surfcache, vid_surfcachesize);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	if (!force_minimized)
	{
		SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

		SetForegroundWindow (mainwindow);
	}

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("%s\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	in_mode_set = false;
	vid.recalc_refdef = 1;

	return true;
}

void VID_LockBuffer (void)
{
	void *surface = lpOffScreenBuffer;
	int bytesPerLine = vid.width;

	lockcount++;

	if (lockcount > 1)
		return;

	// we are not really locking anything. actual locking is done in FlipScreen.
	// just set the pointers to offscreen buffer.

	// Update surface pointer for linear access modes
	vid.buffer = vid.conbuffer = vid.direct = surface;
	vid.rowbytes = vid.conrowbytes = bytesPerLine;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = (void *)(byte *)vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;

	if (lcd_x.value)
		screenwidth <<= 1;
}
		
		
void VID_UnlockBuffer (void)
{
	lockcount--;

	if (lockcount > 0)
		return;

	if (lockcount < 0)
		Sys_Error ("Unbalanced unlock");

	// we are not really unlocking anything. actual unlocking is done in FlipScreen.

// to turn up any unlocked accesses
	vid.buffer = vid.conbuffer = vid.direct = d_viewbuffer = NULL;

}


int VID_ForceUnlockedAndReturnState (void)
{
	int	lk;

	if (!lockcount)
		return 0;

	lk = lockcount;

	lockcount = 1;
	VID_UnlockBuffer ();

	return lk;
}


void VID_ForceLockState (int lk)
{

	if (lk>0)
	{
		lockcount = 0;
		VID_LockBuffer ();
	}

	lockcount = lk;
}


void	VID_SetPalette (unsigned char *palette)
{
	int i;
	palette_changed = true;

	if (palette != vid_curpal)
	{
		memcpy (vid_curpal, palette, sizeof(vid_curpal));
		for (i=0; i<=255; i++)
		{
			vid_fakepal[i*4 + 0] = vid_curpal[i*3 + 2];
			vid_fakepal[i*4 + 1] = vid_curpal[i*3 + 1];
			vid_fakepal[i*4 + 2] = vid_curpal[i*3 + 0];
			vid_fakepal[i*4 + 3] = 0;
		}
	}
}


void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		modenum;
	
	modenum = Q_atoi (Cmd_Argv(1));

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes;
	char		*pinfo;
	vmode_t		*pv;

	lnummodes = VID_NumModes ();

	for (i=0 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}
}


/*
=================
VID_TestMode_f
=================
*/
void VID_TestMode_f (void)
{
	int		modenum;
	double	testduration;

	if (!vid_testingmode)
	{
		modenum = Q_atoi (Cmd_Argv(1));

		if (VID_SetMode (modenum, vid_curpal))
		{
			vid_testingmode = 1;
			testduration = Q_atof (Cmd_Argv(2));
			if (testduration == 0)
				testduration = 5.0;
			vid_testendtime = realtime + testduration;
		}
	}
}


/*
=================
VID_Windowed_f
=================
*/
void VID_Windowed_f (void)
{

	VID_SetMode ((int)vid_windowed_mode.value, vid_curpal);
}


/*
=================
VID_Fullscreen_f
=================
*/
void VID_Fullscreen_f (void)
{

	VID_SetMode ((int)vid_fullscreen_mode.value, vid_curpal);
}


/*
=================
VID_Minimize_f
=================
*/
void VID_Minimize_f (void)
{

// we only support minimizing windows; if you're fullscreen,
// switch to windowed first
	if (modestate == MS_WINDOWED)
		ShowWindow (mainwindow, SW_MINIMIZE);
}



/*
=================
VID_ForceMode_f
=================
*/
void VID_ForceMode_f (void)
{
	int		modenum;

	if (!vid_testingmode)
	{
		modenum = Q_atoi (Cmd_Argv(1));

		force_mode_set = 1;
		VID_SetMode (modenum, vid_curpal);
		force_mode_set = 0;
	}
}


HWND WINAPI InitializeWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASS		wc;
	HWND			hwnd;
	
	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "WinQuake";

	if (!RegisterClass(&wc))
		Sys_Error("Couldn't register window class");

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[MODE_WINDOWED].width;
	WindowRect.bottom = modelist[MODE_WINDOWED].height;

	AdjustWindowRectEx(&WindowRect, WindowStyleFramed, FALSE, 0);

	// create the window we'll use for the rest of the session
	hwnd = CreateWindow(
		"WinQuake",
		"WinQuake",
		WindowStyleFramed,
		0, 0,
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
		Sys_Error("Couldn't create DIB window");

	// tell MGL to use this window for fullscreen modes
	//FakeMGL_DIB_registerFullScreenWindow(hwnd);

	return hwnd;
}


void	VID_Init (unsigned char *palette)
{
	int		i, bestmatch, bestmatchmetric, t, dr, dg, db;
	int		basenummodes;
	byte	*ptmp;
	HRESULT	hr;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&vid_fullscreen_mode);
	Cvar_RegisterVariable (&vid_windowed_mode);
	Cvar_RegisterVariable (&vid_window_x);
	Cvar_RegisterVariable (&vid_window_y);

	Cmd_AddCommand ("vid_testmode", VID_TestMode_f);
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f);
	Cmd_AddCommand ("vid_windowed", VID_Windowed_f);
	Cmd_AddCommand ("vid_fullscreen", VID_Fullscreen_f);
	Cmd_AddCommand ("vid_minimize", VID_Minimize_f);

	mainwindow = InitializeWindow(global_hInstance, global_nCmdShow);

	hr = DirectDrawCreateEx(NULL, &lpDirectDraw, &IID_IDirectDraw7, NULL);

	VID_InitWindowedModes (global_hInstance);

	basenummodes = nummodes;

	VID_InitFullScreenModes (global_hInstance);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid_testingmode = 0;

	if (COM_CheckParm("-startwindowed"))
	{
		startwindowed = 1;
		vid_default = windowed_default;
	}
	else
		vid_default = MODE_FULLSCREEN_DEFAULT;

	if (hwnd_dialog)
		DestroyWindow (hwnd_dialog);

// sound initialization has to go here, preceded by a windowed mode set,
// so there's a window for DirectSound to work with but we're not yet
// fullscreen so the "hardware already in use" dialog is visible if it
// gets displayed
	S_Init ();

	vid_initialized = true;

	force_mode_set = true;
	VID_SetMode (windowed_default, palette);
	force_mode_set = false;

	vid_realmode = vid_modenum;

	VID_SetPalette (palette);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
}


void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		AppActivate(false, false);
		DestroyVideoMode ();

		if (lpDirectDraw)
			IDirectDraw7_Release(lpDirectDraw);

		if (hwnd_dialog)
			DestroyWindow (hwnd_dialog);

		if (mainwindow)
			DestroyWindow(mainwindow);

		vid_testingmode = 0;
		vid_initialized = 0;
	}
}


/*
================
FlipScreen
================
*/
void FlipScreen (vrect_t *rects)
{
	RECT r;
	HRESULT hr;

	DDSURFACEDESC2 ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
		
	hr = IDirectDrawSurface7_Lock(lpddsBackBuffer, NULL, &ddsd, DDLOCK_WRITEONLY, NULL);
	if (hr == DDERR_SURFACELOST)
	{
		if (modestate == MS_FULLSCREEN)
			IDirectDrawSurface7_Restore(lpddsFrontBuffer);
		else if (modestate == MS_WINDOWED)
			IDirectDrawSurface7_Restore(lpddsBackBuffer);

		hr = IDirectDrawSurface7_Lock(lpddsBackBuffer, NULL, &ddsd, DDLOCK_WRITEONLY, NULL);
	}

	if (hr != S_OK)
		return;

	while (rects)
	{
		int x, y;
		byte *srcscanline = lpOffScreenBuffer + rects->y * vid.width;
		byte *dstscanline = (byte*)ddsd.lpSurface + rects->y * ddsd.lPitch;

		int xlimit = rects->x + rects->width;
		for (y = 0; y < rects->height; y++)
		{
			for (x = rects->x; x < xlimit; x++)
			{
				byte col = srcscanline[x];
				((unsigned*)dstscanline)[x] = ((unsigned*)vid_fakepal)[col];
			}
			srcscanline += vid.width;
			dstscanline += ddsd.lPitch;
		}

		rects = rects->pnext;
	}

	hr = IDirectDrawSurface7_Unlock(lpddsBackBuffer, NULL);

	if (modestate == MS_WINDOWED)
	{
		GetClientRect(mainwindow, &r);
		MapWindowPoints(mainwindow, HWND_DESKTOP, (POINT*)&r, 2);
		hr = IDirectDrawSurface7_Blt(lpddsFrontBuffer, &r, lpddsBackBuffer, NULL, 0, NULL);
		if (hr == DDERR_SURFACELOST)
		{
			IDirectDrawSurface7_Restore(lpddsFrontBuffer);
			hr = IDirectDrawSurface7_Blt(lpddsFrontBuffer, &r, lpddsBackBuffer, NULL, 0, NULL);
		}
	}
	else if (modestate == MS_FULLSCREEN)
	{
		hr = IDirectDrawSurface7_Flip(lpddsFrontBuffer, NULL, 0);
	}
}


void	VID_Update (vrect_t *rects)
{
	vrect_t	rect;
	RECT	trect;

	if (palette_changed)
	{
		palette_changed = false;
		rect.x = 0;
		rect.y = 0;
		rect.width = vid.width;
		rect.height = vid.height;
		rect.pnext = NULL;
		rects = &rect;
	}

	if (firstupdate)
	{
		if (modestate == MS_WINDOWED)
		{
			GetWindowRect (mainwindow, &trect);

			if ((trect.left != (int)vid_window_x.value) ||
				(trect.top  != (int)vid_window_y.value))
			{
				if (COM_CheckParm ("-resetwinpos"))
				{
					Cvar_SetValue ("vid_window_x", 0.0);
					Cvar_SetValue ("vid_window_y", 0.0);
				}

				VID_CheckWindowXY ();
				SetWindowPos (mainwindow, NULL, (int)vid_window_x.value,
				  (int)vid_window_y.value, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
			}
		}

		if ((_vid_default_mode_win.value != vid_default) &&
			(!startwindowed || (_vid_default_mode_win.value < MODE_FULLSCREEN_DEFAULT)))
		{
			firstupdate = 0;

			if (COM_CheckParm ("-resetwinpos"))
			{
				Cvar_SetValue ("vid_window_x", 0.0);
				Cvar_SetValue ("vid_window_y", 0.0);
			}

			if ((_vid_default_mode_win.value < 0) ||
				(_vid_default_mode_win.value >= nummodes))
			{
				Cvar_SetValue ("_vid_default_mode_win", windowed_default);
			}

			Cvar_SetValue ("vid_mode", _vid_default_mode_win.value);
		}
	}

	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	if (vid_testingmode)
	{
		if (realtime >= vid_testendtime)
		{
			VID_SetMode (vid_realmode, vid_curpal);
			vid_testingmode = 0;
		}
	}
	else
	{
		if ((int)vid_mode.value != vid_realmode)
		{
			VID_SetMode ((int)vid_mode.value, vid_curpal);
			Cvar_SetValue ("vid_mode", (float)vid_modenum);
								// so if mode set fails, we don't keep on
								//  trying to set that mode
			vid_realmode = vid_modenum;
		}
	}

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if ((int)_windowed_mouse.value != windowed_mouse)
		{
			if (_windowed_mouse.value)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			windowed_mouse = (int)_windowed_mouse.value;
		}
	}
}


/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


//==========================================================================

byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
}; 

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;

	return scantokey[key];
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	ActiveApp = fActive;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		S_ClearBuffer ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		S_ClearBuffer ();
		sound_active = true;
	}

// minimize/restore fulldib windows/mouse-capture normal windows on demand
	if (!in_mode_set)
	{
		if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			if (ActiveApp)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
}


/*
================
VID_HandlePause
================
*/
void VID_HandlePause (qboolean pause)
{

	if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
	{
		if (pause)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
		else
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	LONG			lRet = 0;
	int				fActive, fMinimized, temp;
	HDC				hdc;
	PAINTSTRUCT		ps;

	switch (uMsg)
	{
		case WM_CREATE:
			break;

		case WM_SYSCOMMAND:

		// Check for maximize being hit
			switch (wParam & ~0x0F)
			{
				case SC_MAXIMIZE:
				// if minimized, bring up as a window before going fullscreen,
				// so MGL will have the right state to restore
					if (Minimized)
					{
						force_mode_set = true;
						VID_SetMode (vid_modenum, vid_curpal);
						force_mode_set = false;
					}

					VID_SetMode ((int)vid_fullscreen_mode.value, vid_curpal);
					break;

                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
					if (modestate != MS_WINDOWED)
					{
					// don't call DefWindowProc() because we don't want to start
					// the screen saver fullscreen
						break;
					}

				// fall through windowed and allow the screen saver to start

				default:
					if (!in_mode_set)
					{
						S_BlockSound ();
						S_ClearBuffer ();
					}

					lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);

					if (!in_mode_set)
					{
						S_UnblockSound ();
					}
			}
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();

			if ((modestate == MS_WINDOWED) && !in_mode_set && !Minimized)
				VID_RememberWindowPos ();

			break;

		case WM_SIZE:
			Minimized = false;
			
			if (!(wParam & SIZE_RESTORED))
			{
				if (wParam & SIZE_MINIMIZED)
					Minimized = true;
			}
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
		
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			if (modestate == MS_FULLSCREEN)
			{
				if (!fActive)
				{
					IN_RestoreOriginalMouseState ();
					CDAudio_Pause ();
					in_mode_set = true;
				}
				else if (!fMinimized)
				{
					IN_SetQuakeMouseState ();
					CDAudio_Resume ();
					vid.recalc_refdef = 1;
					in_mode_set = false;
				}
			}

			break;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);

			if (!in_mode_set && host_initialized)
				SCR_UpdateWholeScreen ();

			EndPaint(hWnd, &ps);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!in_mode_set)
				Key_Event (MapKey(lParam), true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!in_mode_set)
				Key_Event (MapKey(lParam), false);
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			if (!in_mode_set)
			{
				temp = 0;

				if (wParam & MK_LBUTTON)
					temp |= 1;

				if (wParam & MK_RBUTTON)
					temp |= 2;

				if (wParam & MK_MBUTTON)
					temp |= 4;

				IN_MouseEvent (temp);
			}
			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

		case WM_DISPLAYCHANGE:
			if (!in_mode_set && (modestate == MS_WINDOWED))
			{
				force_mode_set = true;
				VID_SetMode (vid_modenum, vid_curpal);
				force_mode_set = false;
			}
			break;

   	    case WM_CLOSE:
		// this causes Close in the right-click task bar menu not to work, but right
		// now bad things happen if Close is handled in that case (garbage and a
		// crash on Win95)
			if (!in_mode_set)
			{
				if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				{
					Sys_Quit ();
				}
			}
			break;

		default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
	        break;
    }

    /* return 0 if handled message, 1 if not */
    return lRet;
}


extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
	int		width;
} modedesc_t;

#define MAX_COLUMN_SIZE		5
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 6)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, j, k, column, row, dup, dupmode;
	char		temp[100];
	vmode_t		*pv;
	modedesc_t	tmodedesc;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i<3 ; i++)
	{
		ptr = VID_GetModeDescriptionMemCheck (i);
		modedescs[i].modenum = modelist[i].modenum;
		modedescs[i].desc = ptr;
		modedescs[i].iscur = 0;

		if (vid_modenum == i)
			modedescs[i].iscur = 1;
	}

	vid_wmodes = 3;
	lnummodes = VID_NumModes ();
	
	for (i=3 ; i<lnummodes ; i++)
	{
		ptr = VID_GetModeDescriptionMemCheck (i);
		pv = VID_GetModePtr (i);

	// we only have room for 15 fullscreen modes, so don't allow
	// 360-wide modes, because if there are 5 320-wide modes and
	// 5 360-wide modes, we'll run out of space
		if (ptr && ((pv->width != 360) || COM_CheckParm("-allow360")))
		{
			dup = 0;

			for (j=3 ; j<vid_wmodes ; j++)
			{
				if (!strcmp (modedescs[j].desc, ptr))
				{
					dup = 1;
					dupmode = j;
					break;
				}
			}

			if (dup || (vid_wmodes < MAX_MODEDESCS))       
			{
				if (!dup || COM_CheckParm("-noforcevga"))
				{
					if (dup)
					{
						k = dupmode;
					}
					else
					{
						k = vid_wmodes;
					}

					modedescs[k].modenum = i;
					modedescs[k].desc = ptr;
					modedescs[k].iscur = 0;
					modedescs[k].width = pv->width;

					if (i == vid_modenum)
						modedescs[k].iscur = 1;

					if (!dup)
						vid_wmodes++;
				}
			}
		}
	}

// sort the modes on width (to handle picking up oddball dibonly modes
// after all the others)
	for (i=3 ; i<(vid_wmodes-1) ; i++)
	{
		for (j=(i+1) ; j<vid_wmodes ; j++)
		{
			if (modedescs[i].width > modedescs[j].width)
			{
				tmodedesc = modedescs[i];
				modedescs[i] = modedescs[j];
				modedescs[j] = tmodedesc;
			}
		}
	}


	M_Print (13*8, 36, "Windowed Modes");

	column = 16;
	row = 36+2*8;

	for (i=0 ; i<3; i++)
	{
		if (modedescs[i].iscur)
			M_PrintWhite (column, row, modedescs[i].desc);
		else
			M_Print (column, row, modedescs[i].desc);

		column += 13*8;
	}

	if (vid_wmodes > 3)
	{
		M_Print (12*8, 36+4*8, "Fullscreen Modes");

		column = 16;
		row = 36+6*8;

		for (i=3 ; i<vid_wmodes ; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if (((i - 3) % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 16;
				row += 8;
			}
		}
	}

// line cursor
	if (vid_testingmode)
	{
		sprintf (temp, "TESTING %s",
				modedescs[vid_line].desc);
		M_Print (13*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4, temp);
		M_Print (9*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6,
				"Please wait 5 seconds...");
	}
	else
	{
		M_Print (9*8, 36 + MODE_AREA_HEIGHT * 8 + 8,
				"Press Enter to set mode");
		M_Print (6*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3,
				"T to test mode for 5 seconds");
		ptr = VID_GetModeDescription2 (vid_modenum);

		if (ptr)
		{
			sprintf (temp, "D to set default: %s", ptr);
			M_Print (2*8, 36 + MODE_AREA_HEIGHT * 8 + 8*5, temp);
		}

		ptr = VID_GetModeDescription2 ((int)_vid_default_mode_win.value);

		if (ptr)
		{
			sprintf (temp, "Current default: %s", ptr);
			M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6, temp);
		}

		M_Print (15*8, 36 + MODE_AREA_HEIGHT * 8 + 8*8,
				"Esc to exit");

		row = 36 + 2*8 + (vid_line / VID_ROW_SIZE) * 8;
		column = 8 + (vid_line % VID_ROW_SIZE) * 13*8;

		if (vid_line >= 3)
			row += 3*8;

		M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
	}
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	if (vid_testingmode)
		return;

	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 2) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = vid_wmodes - 1;
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 4) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = (vid_line / VID_ROW_SIZE) * VID_ROW_SIZE;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line -= VID_ROW_SIZE;

		if (vid_line < 0)
		{
			vid_line += ((vid_wmodes + (VID_ROW_SIZE - 1)) /
					VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line >= vid_wmodes)
				vid_line -= VID_ROW_SIZE;
		}
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line += VID_ROW_SIZE;

		if (vid_line >= vid_wmodes)
		{
			vid_line -= ((vid_wmodes + (VID_ROW_SIZE - 1)) /
					VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line < 0)
				vid_line += VID_ROW_SIZE;
		}
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu1.wav");
		VID_SetMode (modedescs[vid_line].modenum, vid_curpal);
		break;

	case 'T':
	case 't':
		S_LocalSound ("misc/menu1.wav");
	// have to set this before setting the mode because WM_PAINT
	// happens during the mode set and does a VID_Update, which
	// checks vid_testingmode
		vid_testingmode = 1;
		vid_testendtime = realtime + 5.0;

		if (!VID_SetMode (modedescs[vid_line].modenum, vid_curpal))
		{
			vid_testingmode = 0;
		}
		break;

	case 'D':
	case 'd':
		S_LocalSound ("misc/menu1.wav");
		firstupdate = 0;
		Cvar_SetValue ("_vid_default_mode_win", vid_modenum);
		break;

	default:
		break;
	}
}
