typedef struct 
{
	MGLDC *mgldc;
	int		aPage;					// Current active display page
	int		vPage;					// Current visible display page
} FakeMGLDC;

typedef int (*FakeMGL_suspend_cb_t)(m_int flags);

void 	FakeMGL_exit(void);
void	FakeMGL_fatalError(const char *msg);
const char * FakeMGL_errorMsg(m_int err);
m_int 	FakeMGL_result(void);
m_int 	FakeMGL_registerDriver(const char *name,void *driver);
void 	FakeMGL_detectGraph(m_int *driver,m_int *mode);
uchar *	FakeMGL_availableModes(void);
m_int	FakeMGL_modeResolution(m_int mode,m_int *xRes,m_int *yRes,m_int *bitsPerPixel);
bool	FakeMGL_init(m_int *driver,m_int *mode);
bool	FakeMGL_initWindowed();
void	FakeMGL_setSuspendAppCallback(FakeMGL_suspend_cb_t staveState);
bool	FakeMGL_changeDisplayMode(m_int mode);
FakeMGLDC	* FakeMGL_createFullscreenDC();
void	FakeMGL_makeCurrentDC(FakeMGLDC *dc);
FakeMGLDC 	* FakeMGL_createMemoryDC(m_int xSize,m_int ySize);
void	FakeMGL_setAppInstance(MGL_HINSTANCE hInstApp);
const char * FakeMGL_modeDriverName(m_int mode);
bool	FakeMGL_destroyDC(FakeMGLDC *dc);
void 	FakeMGL_registerFullScreenWindow(HWND hwndFullScreen);
FakeMGLDC	* FakeMGL_createWindowedDC(MGL_HWND hwnd);
void 	FakeMGL_lock(FakeMGLDC *dc, void **surface, int *bytesPerLine);
void 	FakeMGL_unlock();
void 	FakeMGL_setPalette(FakeMGLDC *dc,palette_t *pal,m_int numColors,m_int startIndex);
void	FakeMGL_realizePalette(FakeMGLDC *dc,m_int numColors,m_int startIndex,m_int waitVRT);
void 	FakeMGL_bitBltCoord(FakeMGLDC *dst,FakeMGLDC *src,m_int left,m_int top,m_int right,m_int bottom,m_int dstLeft,m_int dstTop,m_int op);
bool	FakeMGL_setWinDC(FakeMGLDC *dc,MGL_HDC hdc);
void	FakeMGL_appActivate(FakeMGLDC *winDC,bool active);
bool	FakeMGL_activatePalette(FakeMGLDC *dc,bool unrealize);
void	FakeMGL_flipScreen(FakeMGLDC *dc, int waitVRT);

