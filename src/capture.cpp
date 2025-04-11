#include "header.h"

#include "bitmap.h"
#include <stdlib.h>
#include <stdio.h>

static long int frame_counter = 0;
#ifndef CAPTURE_FRAME_RATE
#define CAPTURE_FRAME_RATE 60
#endif

SUSample buffer[SU_BUFFER_LENGTH];

void entrypoint(void)
{
	CreateDirectory("frames", NULL);

	// this seems to be needed if the user has set the scale of monitor, to get correct results
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

	// initialize window
	ChangeDisplaySettings(&screenSettings, CDS_FULLSCREEN);
	ShowCursor(0);
	HWND window = CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0);
	const HDC hDC = GetDC(window);	
		
	su_render_song(buffer);
	FILE* f;
	f = fopen("song.raw", "wb");
	fwrite((void*)p1, sizeof(SUsample), SU_BUFFER_LENGTH, f);
	fclose(f);

	// initalize opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));

	// create and compile shader programs
	pidMain = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &shader_sync_frag);
	CHECK_ERRORS();	

	long playCursor;

	do
	{
		PeekMessage(0, 0, 0, 0, PM_REMOVE);

		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		CHECK_ERRORS();

		playCursor = (frame_counter++ * SU_SAMPLE_RATE) / CAPTURE_FRAME_RATE * 2 * sizeof(SUsample);

		float syncs[1 + RKT_NUMTRACKS];
		minirocket_sync(
			playCursor / TIME_DIVISOR,
			syncs
		);
		PFNGLUNIFORM1FVPROC glUniform1fvProc = ((PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv"));
		glUniform1fvProc(0, RKT_NUMTRACKS + 1, syncs);
		CHECK_ERRORS();

		glRects(-1, -1, 1, 1);
		CHECK_ERRORS();

		glBindTexture(GL_TEXTURE_2D, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);
		((PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap"))(GL_TEXTURE_2D);
		((PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture"))(GL_TEXTURE0);
		glRects(-1, -1, 1, 1);


		SwapBuffers(hDC);

		static unsigned char framepixels[XRES * YRES * 4];
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, XRES, YRES, GL_BGRA, GL_UNSIGNED_BYTE, framepixels);
		for (int y = 0; y < (YRES + 1) / 2; ++y)
			for (int x = 0; x < XRES; ++x)
				for (int c = 0; c < 4; ++c) {
					auto b = framepixels[(x + y * XRES) * 4 + c]; framepixels[(x + y * XRES) * 4 + c] = framepixels[(x + (YRES - 1 - y) * XRES) * 4 + c]; framepixels[(x + (YRES - 1 - y) * XRES) * 4 + c] = b;
				}
		HBITMAP bitmap = CreateBitmap(XRES, YRES, 1, 32, framepixels);
		PBITMAPINFO bitmapinfo = CreateBitmapInfoStruct(window, bitmap);
		char filename[1024];
		wsprintf(filename, "frames\\frame%06d.bmp", frame_counter);
		CreateBMPFile(window, filename, bitmapinfo, bitmap, hDC);
		DeleteObject(bitmap);
	} while (!GetAsyncKeyState(VK_ESCAPE) && frame_counter < CAPTURE_FRAME_RATE * SU_LENGTH_IN_SAMPLES / SU_SAMPLE_RATE);

	ExitProcess(0);
}
