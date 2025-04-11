#include "header.h"

void entrypoint(void)
{
	// this seems to be needed if the user has set the scale of monitor, to get correct results
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

	// initialize window
	#ifdef WINDOW
		HWND window = CreateWindow("static", 0, WS_POPUP | WS_VISIBLE, 0, 0, XRES, YRES, 0, 0, 0, 0);
	#else // full screen, the default behaviour
		ChangeDisplaySettings(&screenSettings, CDS_FULLSCREEN);
		ShowCursor(0);
		HWND window = CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0);
	#endif
	const HDC hDC = GetDC(window);

	LPDIRECTSOUND lpds;
	LPDIRECTSOUNDBUFFER buf;
	DirectSoundCreate(0, &lpds, 0);

	lpds->SetCooperativeLevel(window, DSSCL_PRIORITY);
	lpds->CreateSoundBuffer(&bufferDesc, &buf, NULL);

	LPVOID p1;
	DWORD l1;

	buf->Lock(0, BUFFER_SIZE, &p1, &l1, NULL, NULL, NULL);	
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)su_render_song, p1, 0, 0);

	// initalize opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));

	// create and compile shader programs
	pidMain = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &shader_sync_frag);
	CHECK_ERRORS();

	buf->Play(0, 0, 0);

	long playCursor;	

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	do
	{				
		PeekMessage(0, 0, 0, 0, PM_REMOVE);

		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		CHECK_ERRORS();

		buf->GetCurrentPosition((DWORD*)&playCursor, NULL);
		float syncs[1 + RKT_NUMTRACKS + SU_NUMSYNCS];
		minirocket_sync(
			playCursor / TIME_DIVISOR,
			syncs
		);
		PFNGLUNIFORM1FVPROC glUniform1fvProc = ((PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv"));
		glUniform1fvProc(0, RKT_NUMTRACKS + 1, syncs);
		CHECK_ERRORS();

		glRects(-1, -1, 1, 1);
		CHECK_ERRORS();			
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);			
		glRects(-1, -1, 1, 1);

		SwapBuffers(hDC);
	} while (!GetAsyncKeyState(VK_ESCAPE) && playCursor < 0x2800000);

	ExitProcess(0);
}
