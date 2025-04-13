#include "../extern/rocket/lib/sync.h"
#include <minirocket_tracknames.h>
#include <corecrt_math.h>
#include <dsound.h>
#include <song.h>

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
	return (playStatus & DSBSTATUS_PLAYING) == DSBSTATUS_PLAYING;
}

void rocket_init(void)
{
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
}

void rocket_sync(float row, LPDIRECTSOUNDBUFFER buf, float *syncs) {
	if (sync_update(device, int(row), &cb, &buf))
		sync_tcp_connect(device, "localhost", SYNC_DEFAULT_PORT);

	syncs[0] = row;
	for (int i = 0; i < RKT_NUMTRACKS; ++i)
	{
		syncs[i + 1] = (float)sync_get_val(s_tracks[i], row);
	}
}
