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
#ifdef SYNC
#include "../extern/rocket/lib/sync.h"
#include <minirocket_tracknames.h>
#endif
#ifdef CAPTURE
#include "bitmap.h"
#include <stdlib.h>
#include <stdio.h>
#endif

#pragma data_seg(".shader")
#include <shader.inl>

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

// the song takes ~41954594 bytes, but we allocate more so that:
// 1) the playcursor keeps on playing empty music afterwards
// 2) we allocate aligned number of bytes (log2(41954594) = 25.3
#define BUFFER_SIZE (1 << 27)

#pragma data_seg(".descfmt")
static DSBUFFERDESC bufferDesc = { sizeof(DSBUFFERDESC), DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_TRUEPLAYPOSITION, BUFFER_SIZE, NULL, &WaveFMT, NULL };

#pragma bss_seg(".pid")
// static allocation saves a few bytes
static int pidMain;

#pragma data_seg(".timediv")
static float TIME_DIVISOR = SU_SAMPLES_PER_ROW * 2 * sizeof(SUsample);

#ifdef SYNC
static struct sync_device* device;
static struct sync_cb cb;
static const struct sync_track* s_tracks[RKT_NUMTRACKS];

static void pause(void* data, int flag)
{
	LPDIRECTSOUNDBUFFER buf = *((LPDIRECTSOUNDBUFFER*)data);
	if (flag)
		buf->Stop();
	else
		buf->Play(0, 0, 0);
}

static void set_row(void* data, int row)
{
	LPDIRECTSOUNDBUFFER buf = *((LPDIRECTSOUNDBUFFER*)data);
	DWORD newpos = row * 2 * SU_SAMPLES_PER_ROW * sizeof(SUsample);
	buf->SetCurrentPosition(newpos);	
}

static int is_playing(void* data)
{
	LPDIRECTSOUNDBUFFER buf = *((LPDIRECTSOUNDBUFFER*)data);
	DWORD playStatus;
	buf->GetStatus(&playStatus);
	return playStatus & DSBSTATUS_PLAYING == DSBSTATUS_PLAYING;
}
#endif

#ifdef UNCOMPRESSED
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef CAPTURE
static long int frame_counter = 0;
#ifndef CAPTURE_FRAME_RATE
#define CAPTURE_FRAME_RATE 60
#endif
#endif

void entrypoint(void)
{
	#ifdef SYNC
		device = sync_create_device("localsync");
		if (!device)
		{
			MessageBox(NULL, "Unable to create rocketDevice", NULL, 0x00000000L);
			ExitProcess(0);
		}

		cb.is_playing = is_playing;
		cb.pause = pause;
		cb.set_row = set_row;

		if (sync_tcp_connect(device, "localhost", SYNC_DEFAULT_PORT))
		{
			MessageBox(NULL, "Rocket failed to connect, run Rocket server first", NULL, 0x00000000L);
			ExitProcess(0);
		}

		for (int i = 0; i < RKT_NUMTRACKS; ++i)
			s_tracks[i] = sync_get_track(device, s_trackNames[i]);
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

	LPDIRECTSOUND lpds;
	LPDIRECTSOUNDBUFFER buf;
	DirectSoundCreate(0, &lpds, 0);

	lpds->SetCooperativeLevel(window, DSSCL_PRIORITY);
	lpds->CreateSoundBuffer(&bufferDesc, &buf, NULL);

	LPVOID p1;
	DWORD l1;

	buf->Lock(0, BUFFER_SIZE, &p1, &l1, NULL, NULL, NULL);
	#ifndef CAPTURE
	 	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)su_render_song, p1, 0, 0);
	#else
		CreateDirectory("frames", NULL);
		su_render_song((SUsample*)p1);
		FILE* f;		
		f = fopen("song.raw", "wb");
		fwrite((void*)p1, sizeof(SUsample), SU_BUFFER_LENGTH, f);
		fclose(f);		
	#endif

	// initalize opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));

	// create and compile shader programs
	pidMain = ((PFNGLCREATESHADERPROGRAMVPROC)wglGetProcAddress("glCreateShaderProgramv"))(GL_FRAGMENT_SHADER, 1, &shader_sync_frag);
	CHECK_ERRORS();

	buf->Play(0, 0, 0);

	long playCursor;

	do
	{				
#if !(DESPERATE)
		// do minimal message handling so windows doesn't kill your application
		// not always strictly necessary but increases compatibility and reliability a lot
		// normally you'd pass an msg struct as the first argument but it's just an
		// output parameter and the implementation presumably does a NULL check
		PeekMessage(0, 0, 0, 0, PM_REMOVE);
#endif

		// render with the primary shader
		((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(pidMain);
		CHECK_ERRORS();

#if CAPTURE		
		playCursor = (frame_counter++ * SU_SAMPLE_RATE) / CAPTURE_FRAME_RATE * 2 * sizeof(SUsample);
#else		
		buf->GetCurrentPosition((DWORD*)&playCursor, NULL);
#endif		

		float syncs[1 + RKT_NUMTRACKS];
#ifdef SYNC
		int row = (int)(playCursor / (SU_SAMPLES_PER_ROW * 2 * sizeof(SUsample)));
		float row_f = (float)(playCursor) / (SU_SAMPLES_PER_ROW * 2 * sizeof(SUsample));

		if (sync_update(device, row, &cb, &buf))
			sync_tcp_connect(device, "localhost", SYNC_DEFAULT_PORT);

		syncs[0] = (float)(playCursor) / (SU_SAMPLES_PER_ROW * 2 * sizeof(SUsample));
		for (int i = 0; i < RKT_NUMTRACKS; ++i)
		{
			syncs[i + 1] = sync_get_val(s_tracks[i], row_f);
		}
#else
		minirocket_sync(
			playCursor / TIME_DIVISOR,
			syncs
		);
#endif	
		PFNGLUNIFORM1FVPROC glUniform1fvProc = ((PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv"));
		glUniform1fvProc(0, RKT_NUMTRACKS + 1, syncs);
		CHECK_ERRORS();

		glRects(-1, -1, 1, 1);
		CHECK_ERRORS();			

		SwapBuffers(hDC);

#if CAPTURE
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
#endif

	} while (
		!GetAsyncKeyState(VK_ESCAPE)
		#if !defined(SYNC) && !defined(CAPTURE)
		&& playCursor < 0x2800000 // nice round number
		#endif
		#ifdef CAPTURE
		&& frame_counter < CAPTURE_FRAME_RATE* SU_LENGTH_IN_SAMPLES/SU_SAMPLE_RATE
		#endif
	);

	ExitProcess(0);
}
