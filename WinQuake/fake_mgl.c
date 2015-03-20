#include "quakedef.h"
#include "winquake.h"
#include "fake_mgl.h"

void 	FakeMGL_exit(void)
{
	MGL_exit();
}


void	FakeMGL_fatalError(const char *msg)
{
	MGL_fatalError(msg);
}


const char * FakeMGL_errorMsg(m_int err)
{
	return MGL_errorMsg(err);
}


m_int 	FakeMGL_result(void)
{
	return MGL_result();
}


m_int 	FakeMGL_registerDriver(const char *name,void *driver)
{
	return MGL_registerDriver(name, driver);
}


void	FakeMGL_unregisterAllDrivers(void)
{
	MGL_unregisterAllDrivers();
}


void 	FakeMGL_detectGraph(m_int *driver,m_int *mode)
{
	MGL_detectGraph(driver, mode);
}


uchar *	FakeMGL_availableModes(void)
{
	// modes are 0xFF-terminated
	uchar *result = MGL_availableModes();
//	result[1] = 0xFF;
	return result;
}


m_int	FakeMGL_modeResolution(m_int mode,m_int *xRes,m_int *yRes,m_int *bitsPerPixel)
{
	return MGL_modeResolution(mode, xRes, yRes, bitsPerPixel);
}


bool	FakeMGL_init(m_int *driver,m_int *mode)
{
	/* Initialise the MGL for fullscreen output */
	return MGL_init(driver, mode, "");
}


bool	FakeMGL_initWindowed()
{
	/* Initialise the MGL just for Windowed output, not full screen */
	return MGL_initWindowed("");
}


void	FakeMGL_setSuspendAppCallback(MGL_suspend_cb_t staveState)
{
	MGL_setSuspendAppCallback(staveState);
}


bool	FakeMGL_changeDisplayMode(m_int mode)
{
	/* Change the active display mode. You must destroy all display device
	* contexts before calling this function, and re-create them again with
	* the new display mode. Does not affect any event handling hooks.
	*/
	return MGL_changeDisplayMode(mode);
}


m_int	FakeMGL_availablePages(m_int mode)
{
	return MGL_availablePages(mode);
}


MGLDC	* FakeMGL_createDisplayDC(m_int numBuffers)
{
	return MGL_createDisplayDC(numBuffers);
}


m_int	FakeMGL_surfaceAccessType(MGLDC *dc)
{
	return MGL_surfaceAccessType(dc);
}


MGLDC *	FakeMGL_makeCurrentDC(MGLDC *dc)
{
	return MGL_makeCurrentDC(dc);
}


MGLDC 	* FakeMGL_createMemoryDC(m_int xSize,m_int ySize,m_int bitsPerPixel,pixel_format_t *pf)
{
	return MGL_createMemoryDC(xSize, ySize, bitsPerPixel, pf);
}


m_int 	FakeMGL_sizex(MGLDC *dc)
{
	return MGL_sizex(dc);
}


m_int 	FakeMGL_sizey(MGLDC *dc)
{
	return MGL_sizey(dc);
}


void	FakeMGL_setActivePage(MGLDC *dc,m_int page)
{
	MGL_setActivePage(dc, page);
}


void	FakeMGL_setVisualPage(MGLDC *dc,m_int page,m_int waitVRT)
{
	MGL_setVisualPage(dc, page, waitVRT);
}


void	FakeMGL_setAppInstance(MGL_HINSTANCE hInstApp)
{
	MGL_setAppInstance(hInstApp);
}


const char * FakeMGL_modeDriverName(m_int mode)
{
	return MGL_modeDriverName(mode);
}


bool	FakeMGL_destroyDC(MGLDC *dc)
{
	return MGL_destroyDC(dc);
}


void 	FakeMGL_registerFullScreenWindow(HWND hwndFullScreen)
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


MGLDC	* FakeMGL_createWindowedDC(MGL_HWND hwnd)
{
	return MGL_createWindowedDC(hwnd);
}


void 	FakeMGL_beginDirectAccess(void)
{
	MGL_beginDirectAccess();
}


void 	FakeMGL_endDirectAccess(void)
{
	MGL_endDirectAccess();
}


void 	FakeMGL_setPalette(MGLDC *dc,palette_t *pal,m_int numColors,m_int startIndex)
{
	MGL_setPalette(dc, pal, numColors, startIndex);
}


void	FakeMGL_realizePalette(MGLDC *dc,m_int numColors,m_int startIndex,m_int waitVRT)
{
	MGL_realizePalette(dc, numColors, startIndex, waitVRT);
}


void 	FakeMGL_stretchBltCoord(MGLDC *dst,MGLDC *src,m_int left,m_int top,m_int right,m_int bottom,m_int dstLeft,m_int dstTop,m_int dstRight,m_int dstBottom)
{
	MGL_stretchBltCoord(dst, src, left, top, right, bottom, dstLeft, dstTop, dstRight, dstBottom);
}


void 	FakeMGL_bitBltCoord(MGLDC *dst,MGLDC *src,m_int left,m_int top,m_int right,m_int bottom,m_int dstLeft,m_int dstTop,m_int op)
{
	MGL_bitBltCoord(dst, src, left, top, right, bottom, dstLeft, dstTop, op);
}


bool	FakeMGL_setWinDC(MGLDC *dc,MGL_HDC hdc)
{
	return MGL_setWinDC(dc, hdc);
}


void	FakeMGL_appActivate(MGLDC *winDC,bool active)
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
	MGL_appActivate(winDC, active);
}


bool	FakeMGL_activatePalette(MGLDC *dc,bool unrealize)
{
	/* Activate the WindowDC's palette */
	return MGL_activatePalette(dc, unrealize);
}
