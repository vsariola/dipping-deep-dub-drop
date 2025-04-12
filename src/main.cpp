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

	// initalize opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));	

	LPDIRECTSOUND lpds;
	LPDIRECTSOUNDBUFFER buf;
	DirectSoundCreate(0, &lpds, 0);

	lpds->SetCooperativeLevel(window, DSSCL_PRIORITY);
	lpds->CreateSoundBuffer(&bufferDesc, &buf, NULL);

	LPVOID p1;
	DWORD l1;

	buf->Lock(0, BUFFER_SIZE, &p1, &l1, NULL, NULL, NULL);	
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)su_render_song, p1, 0, 0);

	// write text
	SelectObject(hDC, CreateFont(155 * YRES / 1080, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Verdana"));
#ifdef WINDOW
	wglUseFontBitmaps(hDC, 0, 255, 1000); // 0 does not work as a listbase when using 720p WINDOW, WHY????
	glListBase(1000);
#else
	wglUseFontBitmaps(hDC, 0, 256, 0);
#endif	
	CHECK_ERRORS();
	glRasterPos2s(-1, 0);
	CHECK_ERRORS();
	glCallLists((sizeof(overtext) / sizeof(overtext[0])) - 1, GL_UNSIGNED_BYTE, overtext);
	CHECK_ERRORS();

	glBindTexture(GL_TEXTURE_2D, 1);
	CHECK_ERRORS();	

	// create and compile shader programs
	pidMain = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &shader_sync_frag);
	CHECK_ERRORS();

	buf->Play(0, 0, 0);

	long playCursor;	

	do
	{				
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);
		((PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap"))(GL_TEXTURE_2D);
		glRects(-1, -1, 1, 1);

		SwapBuffers(hDC);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		PeekMessage(0, 0, 0, 0, PM_REMOVE);

		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		CHECK_ERRORS();

		buf->GetCurrentPosition((DWORD*)&playCursor, NULL);		
		float syncs[1 + RKT_NUMTRACKS + SU_NUMSYNCS];
		minirocket_sync(
			playCursor / TIME_DIVISOR,
			syncs
		);

		for (int i = 0; i < SU_NUMSYNCS; ++i)
		{
			syncs[i + 1 + RKT_NUMTRACKS] = syncBuf[(playCursor / (2 * sizeof(SUsample)) >> 8) * SU_NUMSYNCS + i];
		}

		PFNGLUNIFORM1FVPROC glUniform1fvProc = ((PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv"));
		glUniform1fvProc(0, RKT_NUMTRACKS + SU_NUMSYNCS + 1, syncs);
		CHECK_ERRORS();

		glRects(-1, -1, 1, 1);
		CHECK_ERRORS();				

		syncs[0] = -syncs[0];
		glUniform1fvProc(0, RKT_NUMTRACKS + SU_NUMSYNCS + 1, syncs);
		CHECK_ERRORS();

		// First time this copies the font to texture unit 0 bound to texture 1
		// Subsequent times this copies the screen to texture unit 1 bound to texture 0 for post processing		
		(((PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture")))(GL_TEXTURE1);		
	} while (!GetAsyncKeyState(VK_ESCAPE) && playCursor < SU_LENGTH_IN_SAMPLES*2*sizeof(SUsample));

	ExitProcess(0);
}
