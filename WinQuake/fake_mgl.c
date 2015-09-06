#include "quakedef.h"
#include "winquake.h"
#include "fake_mgl.h"

void 	FakeMGL_DIB_registerFullScreenWindow(HWND hwndFullScreen)
{
}

void	FakeMGL_DIB_setAppInstance(HINSTANCE hInstance)
{
}

void	FakeMGL_DIB_registerDriver(void)
{
}

void 	FakeMGL_exit(void)
{
}


void	FakeMGL_fail()
{
}


int 	FakeMGL_FULL_registerDriver()
{
	return 0;
}

void 	FakeMGL_FULL_detectGraph(int *driver,int *mode)
{
}


unsigned char *	FakeMGL_FULL_availableModes(void)
{
	return 0;
}


int	FakeMGL_FULL_modeResolution(int mode,int *xRes,int *yRes,int *bitsPerPixel)
{
	return 0;
}


int	FakeMGL_FULL_init(int *driver,int *mode)
{
	return false;
}


int	FakeMGL_DIB_initWindowed()
{
	return false;
}

void	FakeMGL_FULL_setSuspendAppCallback(FakeMGL_suspend_cb_t staveState)
{
}


int	FakeMGL_FULL_changeDisplayMode(int mode)
{
	return false;
}


int	FakeMGL_DIB_changeDisplayMode(int mode)
{
	return false;
}


FakeMGLDC_FULL	* FakeMGL_FULL_createFullscreenDC()
{
	return NULL;
}

void FakeMGL_FULL_flipScreen(FakeMGLDC_FULL *dc, int waitVRT)
{
}

const char * FakeMGL_modeDriverName(int mode)
{
	return "bogus";
}


int	FakeMGL_FULL_destroyDC(FakeMGLDC_FULL *dc)
{
	return false;
}


void 	FakeMGL_FULL_lock(FakeMGLDC_FULL *dc, void **surface, int *bytesPerLine)
{
}


void 	FakeMGL_unlock()
{
}


int	FakeMGL_FULL_activatePalette(FakeMGLDC_FULL *dc,int unrealize)
{
	return 0;
}
