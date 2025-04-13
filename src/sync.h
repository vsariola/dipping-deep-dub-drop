#include "../extern/rocket/lib/sync.h"
#include <minirocket_tracknames.h>
#include <dsound.h>

void rocket_init(void);
void rocket_sync(float row, LPDIRECTSOUNDBUFFER buf, float* syncs);