typedef struct 
{
	MGLDC *mgldca;
	MGLDC *mgldcb;
} FakeMGLDC_DIB;

typedef struct 
{
	MGLDC *mgldc;
	int		aPage;					// Current active display page
	int		vPage;					// Current visible display page
} FakeMGLDC_FULL;

typedef int (*FakeMGL_suspend_cb_t)(m_int flags);

// functions common to DIB (windowed) and FULL (fullscreen DirectDraw) modes
void 	FakeMGL_exit(void);
void	FakeMGL_fail(void);
const char * FakeMGL_modeDriverName(m_int mode);
void 	FakeMGL_unlock();

// unsure if DIB or common
void	FakeMGL_DIB_setAppInstance(MGL_HINSTANCE hInstApp);

// this should definitely by FULL, but for now it's in DIB-specific part of code
void 	FakeMGL_DIB_registerFullScreenWindow(HWND hwndFullScreen);

// DIB-specific functions
bool	FakeMGL_DIB_destroyDC(FakeMGLDC_DIB *dc);
void	FakeMGL_DIB_appActivate(FakeMGLDC_DIB *winDC,bool active);
void 	FakeMGL_DIB_setPalette(FakeMGLDC_DIB *dc,palette_t *pal,m_int numColors,m_int startIndex);
bool	FakeMGL_DIB_activatePalette(FakeMGLDC_DIB *dc,bool unrealize);
m_int 	FakeMGL_DIB_registerDriver(const char *name,void *driver);
bool	FakeMGL_DIB_initWindowed();
bool	FakeMGL_DIB_changeDisplayMode(m_int mode);
bool	FakeMGL_DIB_createWindowedDC(MGL_HWND hwnd, m_int xSize,m_int ySize, FakeMGLDC_DIB **dc);
void 	FakeMGL_DIB_lock(FakeMGLDC_DIB *dc, void **surface, int *bytesPerLine);
bool	FakeMGL_DIB_setWinDC(FakeMGLDC_DIB *dc,MGL_HDC hdc);
void 	FakeMGL_DIB_bitBltCoord(FakeMGLDC_DIB *dc,m_int left,m_int top,m_int right,m_int bottom,m_int dstLeft,m_int dstTop,m_int op);

// FULL-specific functions
bool	FakeMGL_FULL_destroyDC(FakeMGLDC_FULL *dc);
void	FakeMGL_FULL_appActivate(FakeMGLDC_FULL *winDC,bool active);
void 	FakeMGL_FULL_setPalette(FakeMGLDC_FULL *dc,palette_t *pal,m_int numColors,m_int startIndex);
bool	FakeMGL_FULL_activatePalette(FakeMGLDC_FULL *dc,bool unrealize);
m_int 	FakeMGL_FULL_registerDriver(const char *name,void *driver);
void 	FakeMGL_FULL_detectGraph(m_int *driver,m_int *mode);
uchar *	FakeMGL_FULL_availableModes(void);
m_int	FakeMGL_FULL_modeResolution(m_int mode,m_int *xRes,m_int *yRes,m_int *bitsPerPixel);
bool	FakeMGL_FULL_init(m_int *driver,m_int *mode);
void	FakeMGL_FULL_setSuspendAppCallback(FakeMGL_suspend_cb_t staveState);
bool	FakeMGL_FULL_changeDisplayMode(m_int mode);
FakeMGLDC_FULL	* FakeMGL_FULL_createFullscreenDC();
void	FakeMGL_FULL_makeCurrentDC(FakeMGLDC_FULL *dc);
void 	FakeMGL_FULL_lock(FakeMGLDC_FULL *dc, void **surface, int *bytesPerLine);
void	FakeMGL_FULL_flipScreen(FakeMGLDC_FULL *dc, int waitVRT);
