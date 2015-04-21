#include "quakedef.h"
#include "winquake.h"
#include "fake_mgl.h"

void 	FakeMGL_exit(void)
{
	MGL_exit();
}


void	FakeMGL_fail()
{
	FakeMGL_exit();
	MGL_fatalError(MGL_errorMsg(MGL_result()));
}


m_int 	FakeMGL_FULL_registerDriver(const char *name,void *driver)
{
	return MGL_registerDriver(name, driver);
}


m_int 	FakeMGL_DIB_registerDriver(const char *name,void *driver)
{
	return MGL_registerDriver(name, driver);
}


void 	FakeMGL_FULL_detectGraph(m_int *driver,m_int *mode)
{
	MGL_detectGraph(driver, mode);
}


uchar *	FakeMGL_FULL_availableModes(void)
{
	// modes are 0xFF-terminated
	uchar *result = MGL_availableModes();
//	result[1] = 0xFF;
	return result;
}


m_int	FakeMGL_FULL_modeResolution(m_int mode,m_int *xRes,m_int *yRes,m_int *bitsPerPixel)
{
	return MGL_modeResolution(mode, xRes, yRes, bitsPerPixel);
}


bool	FakeMGL_FULL_init(m_int *driver,m_int *mode)
{
	/* Initialise the MGL for fullscreen output */
	return MGL_init(driver, mode, "");
}


bool	FakeMGL_DIB_initWindowed()
{
	/* Initialise the MGL just for Windowed output, not full screen */
	return MGL_initWindowed("");
}

FakeMGL_suspend_cb_t fakeStaveState;

int wrapStaveState(MGLDC *dc,m_int flags)
{
	return fakeStaveState(flags);
}

void	FakeMGL_FULL_setSuspendAppCallback(FakeMGL_suspend_cb_t staveState)
{
	/*---------------------------------------------------------------------------
	* Set a fullscreen suspend application callback function. This is used in
	* fullscreen video modes to allow switching back to the normal operating
	* system graphical shell (such as Windows GDI, OS/2 PM etc).
	*-------------------------------------------------------------------------*/

	fakeStaveState = staveState;
	MGL_setSuspendAppCallback(wrapStaveState);
}


bool	FakeMGL_FULL_changeDisplayMode(m_int mode)
{
	/* Change the active display mode. You must destroy all display device
	* contexts before calling this function, and re-create them again with
	* the new display mode. Does not affect any event handling hooks.
	*/
	return MGL_changeDisplayMode(mode);
}


bool	FakeMGL_DIB_changeDisplayMode(m_int mode)
{
	/* Change the active display mode. You must destroy all display device
	* contexts before calling this function, and re-create them again with
	* the new display mode. Does not affect any event handling hooks.
	*/
	return MGL_changeDisplayMode(mode);
}


static FakeMGLDC * makeFakeDC(MGLDC *realDC)
{
	if (realDC)
	{
		FakeMGLDC *fakedc = malloc(sizeof(FakeMGLDC));
		fakedc->mgldc = realDC;
		return fakedc;
	}
	else
		return NULL;
}

FakeMGLDC	* FakeMGL_FULL_createFullscreenDC()
{
	FakeMGLDC *fakedc = makeFakeDC(MGL_createDisplayDC(2));

	if (fakedc)
	{
		// Set up for page flipping
		MGL_setActivePage(fakedc->mgldc, fakedc->aPage = 1);
		MGL_setVisualPage(fakedc->mgldc, fakedc->vPage = 0, false);
		MGL_makeCurrentDC(fakedc->mgldc);
	}

	return fakedc;
}

void FakeMGL_FULL_flipScreen(FakeMGLDC *dc, int waitVRT)
{
	if (dc)
	{
		dc->aPage = (dc->aPage+1) % 2;
		dc->vPage = (dc->vPage+1) % 2;
		MGL_setActivePage(dc->mgldc,dc->aPage);
		MGL_setVisualPage(dc->mgldc,dc->vPage,waitVRT);
	}
}


FakeMGLDC 	* FakeMGL_DIB_createMemoryDC(m_int xSize,m_int ySize)
{
	FakeMGLDC *fakedc = makeFakeDC(MGL_createMemoryDC(xSize, ySize, 8, NULL));
	
	if (fakedc)
		MGL_makeCurrentDC(fakedc->mgldc);

	return fakedc;
}


void	FakeMGL_DIB_setAppInstance(MGL_HINSTANCE hInstApp)
{
	MGL_setAppInstance(hInstApp);
}


const char * FakeMGL_modeDriverName(m_int mode)
{
	return MGL_modeDriverName(mode);
}


bool	FakeMGL_FULL_destroyDC(FakeMGLDC *dc)
{
	if (!dc)
		return MGL_destroyDC(NULL);
	else
	{
		bool result = MGL_destroyDC(dc->mgldc);
		free(dc);
		return result;
	}
}


bool	FakeMGL_DIB_destroyDC(FakeMGLDC *dc)
{
	if (!dc)
		return MGL_destroyDC(NULL);
	else
	{
		bool result = MGL_destroyDC(dc->mgldc);
		free(dc);
		return result;
	}
}



void 	FakeMGL_DIB_registerFullScreenWindow(HWND hwndFullScreen)
{
	/* Function to register a fullscreen window with the MGL. If you wish
	 * for the MGL to use your own window for fullscreen modes, you can
	 * register it with this function. Note that when the MGL goes into
	 * fullscreen modes, the attributes, size and position of the window are
	 * modified to make it into a fullscreen Window necessary to cover the
	 * entire desktop, and the state of the window will be restore to the original
	 * format on return to normal GDI mode.
	 *
	 * Note that if you are using a common window for Windowed mode and fullscreen
	 * modes of your application, you will need to ensure that certain messages
	 * that you window normally handles in windowed modes are ignored when in
	 * fullscreen modes.
	 */
	MGL_registerFullScreenWindow(hwndFullScreen);
}


FakeMGLDC	* FakeMGL_DIB_createWindowedDC(MGL_HWND hwnd)
{
	return makeFakeDC(MGL_createWindowedDC(hwnd));
}


void 	FakeMGL_FULL_lock(FakeMGLDC *dc, void **surface, int *bytesPerLine)
{
	MGL_beginDirectAccess();
	if (dc)
	{
		if (surface)
			*surface = dc->mgldc->surface;
		if (bytesPerLine)
			*bytesPerLine = dc->mgldc->mi.bytesPerLine;
	}
}


void 	FakeMGL_DIB_lock(FakeMGLDC *dc, void **surface, int *bytesPerLine)
{
	MGL_beginDirectAccess();
	if (dc)
	{
		if (surface)
			*surface = dc->mgldc->surface;
		if (bytesPerLine)
			*bytesPerLine = dc->mgldc->mi.bytesPerLine;
	}
}


void 	FakeMGL_unlock()
{
	MGL_endDirectAccess();
}


void 	FakeMGL_DIB_setPalette(FakeMGLDC *dc,palette_t *pal,m_int numColors,m_int startIndex)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	MGL_setPalette(mdc, pal, numColors, startIndex);
}


void 	FakeMGL_FULL_setPalette(FakeMGLDC *dc,palette_t *pal,m_int numColors,m_int startIndex)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	MGL_setPalette(mdc, pal, numColors, startIndex);
}


void	FakeMGL_realizePalette(FakeMGLDC *dc,m_int numColors,m_int startIndex,m_int waitVRT)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	MGL_realizePalette(mdc, numColors, startIndex, waitVRT);
}


void	FakeMGL_DIB_realizePalette(FakeMGLDC *dc,m_int numColors,m_int startIndex,m_int waitVRT)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	MGL_realizePalette(mdc, numColors, startIndex, waitVRT);
}


void	FakeMGL_FULL_realizePalette(FakeMGLDC *dc,m_int numColors,m_int startIndex,m_int waitVRT)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	MGL_realizePalette(mdc, numColors, startIndex, waitVRT);
}


void 	FakeMGL_DIB_bitBltCoord(FakeMGLDC *dst,FakeMGLDC *src,m_int left,m_int top,m_int right,m_int bottom,m_int dstLeft,m_int dstTop,m_int op)
{
	MGLDC *mdst = dst ? dst->mgldc : NULL;
	MGLDC *msrc = src ? src->mgldc : NULL;

	MGL_bitBltCoord(mdst, msrc, left, top, right, bottom, dstLeft, dstTop, op);
}


bool	FakeMGL_DIB_setWinDC(FakeMGLDC *dc,MGL_HDC hdc)
{
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	return MGL_setWinDC(mdc, hdc);
}


void	FakeMGL_DIB_appActivate(FakeMGLDC *winDC,bool active)
{
	/* Let the MGL know when your application is being activated or deactivated.
	* This function only needs to be called when running in Windowed modes and
	* you have set the system palette to SYSPAL_NOSTATIC mode, to ensure
	* that the MGL can properly re-map your application palette when your
	* app is not active and allow Windows to re-map your bitmap colors on the
	* fly. This function should be passed a pointer to the currently active
	* MGL Windowed DC and a flag to indicate whether the app is in the background
	* or not.
	*/
	MGLDC *mwinDC = winDC ? winDC->mgldc : NULL;

	MGL_appActivate(mwinDC, active);
}


void	FakeMGL_FULL_appActivate(FakeMGLDC *winDC,bool active)
{
	/* Let the MGL know when your application is being activated or deactivated.
	* This function only needs to be called when running in Windowed modes and
	* you have set the system palette to SYSPAL_NOSTATIC mode, to ensure
	* that the MGL can properly re-map your application palette when your
	* app is not active and allow Windows to re-map your bitmap colors on the
	* fly. This function should be passed a pointer to the currently active
	* MGL Windowed DC and a flag to indicate whether the app is in the background
	* or not.
	*/
	MGLDC *mwinDC = winDC ? winDC->mgldc : NULL;

	MGL_appActivate(mwinDC, active);
}


bool	FakeMGL_DIB_activatePalette(FakeMGLDC *dc,bool unrealize)
{
	/* Activate the WindowDC's palette */
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	return MGL_activatePalette(mdc, unrealize);
}


bool	FakeMGL_FULL_activatePalette(FakeMGLDC *dc,bool unrealize)
{
	/* Activate the WindowDC's palette */
	MGLDC *mdc = dc ? dc->mgldc : NULL;

	return MGL_activatePalette(mdc, unrealize);
}
