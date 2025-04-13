// minify windows.h
#pragma warning( disable : 6031 6387)
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define VC_LEANMEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>
#include <GL/gl.h>

// Defining OPENGL_DEBUG makes the CHECK_ERRORS() macro show the error code in messagebox.
// Without the macro, CHECK_ERRORS() is a nop.
#include "debug.h"

#include "glext.h"
#include "song.h"
#include <minirocket.h>
#include "sync.h"
#include "bitmap.h"

#pragma data_seg(".shader")
#include <shader.inl>
#include <cstdio>

#pragma data_seg(".pixelfmt")
static const PIXELFORMATDESCRIPTOR pfd = {
	sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
	32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
};

#pragma data_seg(".screensettings")
static DEVMODE screenSettings = {
	{0}, 0, 0, sizeof(screenSettings), 0, DM_PELSWIDTH | DM_PELSHEIGHT,
	{0}, 0, 0, 0, 0, 0, {0}, 0, 0, XRES, YRES, 0, 0,
	#if(WINVER >= 0x0400)
		0, 0, 0, 0, 0, 0,
			#if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400)
			0, 0
		#endif
	#endif
};

#pragma bss_seg(".syncbuf")
extern "C" {
	float syncBuf[SU_SYNCBUFFER_LENGTH];
}

#pragma data_seg(".wavefmt")
static WAVEFORMATEX WaveFMT =
{
#ifdef SU_SAMPLE_FLOAT
	WAVE_FORMAT_IEEE_FLOAT,
#else
	WAVE_FORMAT_PCM,
#endif
	2,                                     // channels
	SU_SAMPLE_RATE,                        // samples per sec
	SU_SAMPLE_RATE * sizeof(SUsample) * 2, // bytes per sec
	sizeof(SUsample) * 2,                  // block alignment;
	sizeof(SUsample) * 8,                  // bits per sample
	0                                      // extension not needed
};

// the song takes less but we allocate more
#define BUFFER_SIZE (1 << 27)

#pragma data_seg(".descfmt")
static DSBUFFERDESC bufferDesc = { sizeof(DSBUFFERDESC), DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_TRUEPLAYPOSITION, BUFFER_SIZE, NULL, &WaveFMT, NULL };

#pragma bss_seg(".pid")
// static allocation saves a few bytes
static int pidMain;

#pragma data_seg(".timediv")
static float TIME_DIVISOR = SU_SAMPLES_PER_ROW * 2 * sizeof(SUsample);

#ifdef UNCOMPRESSED
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#pragma data_seg(".overtxt")
static const char overtext[] = " chlumpie & pestis";
void entrypoint(void)
{
#ifdef SYNC
	rocket_init();
#endif

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
#ifdef CAPTURE
	su_render_song((SUsample*)p1);
	FILE* f;
	f = fopen("song.raw", "wb");
	fwrite((void*)p1, sizeof(SUsample), SU_BUFFER_LENGTH, f);
	fclose(f);
#else
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)su_render_song, p1, 0, 0);
#endif

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

#ifndef CAPTURE
	buf->Play(0, 0, 0);
#endif

	long playCursor;	

	do
	{				
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, XRES, YRES, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glRects(-1, -1, 1, 1);

		SwapBuffers(hDC);
		PeekMessage(0, 0, 0, 0, PM_REMOVE);

		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		CHECK_ERRORS();

#ifdef CAPTURE
		playCursor = SaveFrame(hDC,window);
#else
		buf->GetCurrentPosition((DWORD*)&playCursor, NULL);
#endif

		float syncs[1 + RKT_NUMTRACKS + SU_NUMSYNCS];

#ifdef SYNC
		rocket_sync(playCursor / TIME_DIVISOR, buf, syncs);
#else
		minirocket_sync(playCursor / TIME_DIVISOR,syncs);
#endif

		for (int i = 0; i < SU_NUMSYNCS; ++i)
		{
			syncs[i + 1 + RKT_NUMTRACKS] = syncBuf[(playCursor / (2 * sizeof(SUsample)) >> 8) * SU_NUMSYNCS + i];
		}

		PFNGLUNIFORM1FVPROC glUniform1fvProc = ((PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv"));
		glUniform1fvProc(2, RKT_NUMTRACKS + SU_NUMSYNCS + 1, syncs);
		CHECK_ERRORS();

		glRects(-1, -1, 1, 1);
		CHECK_ERRORS();				

		syncs[0] = -syncs[0];
		glUniform1fvProc(2, RKT_NUMTRACKS + SU_NUMSYNCS + 1, syncs);
		CHECK_ERRORS();

		// First time this copies the font to texture unit 0 bound to texture 1
		// Subsequent times this copies the screen to texture unit 1 bound to texture 0 for post processing		
		(((PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture")))(GL_TEXTURE1);		
		glBindTexture(GL_TEXTURE_2D, 0);
	} while (!GetAsyncKeyState(VK_ESCAPE) && playCursor < SU_LENGTH_IN_SAMPLES*2*sizeof(SUsample));

	ExitProcess(0);
}
