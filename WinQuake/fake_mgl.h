
typedef struct 
{
	int dummy;
} FakeMGLDC_FULL;

typedef int (*FakeMGL_suspend_cb_t)(int flags);

// functions common to DIB (windowed) and FULL (fullscreen DirectDraw) modes
void 	FakeMGL_exit(void);
void	FakeMGL_fail(void);
const char * FakeMGL_modeDriverName(int mode);
void 	FakeMGL_unlock();

void 	FakeMGL_DIB_registerFullScreenWindow(HWND hwndFullScreen);
void	FakeMGL_DIB_setAppInstance(HINSTANCE hInstance);
void	FakeMGL_DIB_registerDriver(void);

// FULL-specific functions
int	FakeMGL_FULL_destroyDC(FakeMGLDC_FULL *dc);
int 	FakeMGL_FULL_registerDriver();
void 	FakeMGL_FULL_detectGraph(int *driver,int *mode);
unsigned char *	FakeMGL_FULL_availableModes(void);
int	FakeMGL_FULL_modeResolution(int mode,int *xRes,int *yRes,int *bitsPerPixel);
int	FakeMGL_FULL_init(int *driver,int *mode);
void	FakeMGL_FULL_setSuspendAppCallback(FakeMGL_suspend_cb_t staveState);
int	FakeMGL_FULL_changeDisplayMode(int mode);
FakeMGLDC_FULL	* FakeMGL_FULL_createFullscreenDC();
void	FakeMGL_FULL_makeCurrentDC(FakeMGLDC_FULL *dc);
void 	FakeMGL_FULL_lock(FakeMGLDC_FULL *dc, void **surface, int *bytesPerLine);
void	FakeMGL_FULL_flipScreen(FakeMGLDC_FULL *dc, int waitVRT);
