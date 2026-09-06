#pragma once
#include "app.h"

// Refresh the launcher glance subtitle with the most recent heart-rate reading.
// App-only (the AppGlance API is not available to the worker). Call on app exit
// so the launcher shows "72 BPM · 3 min ago" until the reading goes stale.
// A snapshot with no usable BPM clears the glance.
void glance_update(const MonitorSnapshot *snap);
