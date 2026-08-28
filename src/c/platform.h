#pragma once

// Cardia compiles the analysis/engine/storage code into two binaries: the
// foreground app and the background worker. The worker uses a different SDK
// umbrella header (no UI, no AppMessage, no vibes). The worker build defines
// CARDIA_WORKER (see wscript).

#if defined(CARDIA_WORKER)
#include <pebble_worker.h>
#else
#include <pebble.h>
#endif
