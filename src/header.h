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

#ifdef UNCOMPRESSED
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
