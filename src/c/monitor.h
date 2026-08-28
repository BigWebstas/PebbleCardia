#pragma once
#include "app.h"

typedef void (*MonitorUpdateHandler)(const MonitorSnapshot *snap);

typedef enum {
  MON_BG_OK,             // worker running / stopped as requested
  MON_BG_ASKING,         // system is showing the "switch worker" confirmation
  MON_BG_NO_WORKER,      // this build has no worker binary
  MON_BG_ERROR,
} MonitorBgResult;

void monitor_init(void);
void monitor_deinit(void);

// Start monitoring. If Settings.background_on and the worker is available, the
// app attaches to the worker's stream instead of running its own engine.
void monitor_start(void);
void monitor_stop(void);
bool monitor_running(void);

// True when the background worker (not the in-app engine) is the data source.
bool monitor_background_active(void);

// Turn the always-on background worker on or off. Persists the choice.
MonitorBgResult monitor_set_background(bool on);

// Re-apply Settings.sample_rate_s to whichever engine is live.
void monitor_apply_sample_rate(void);

void monitor_set_handler(MonitorUpdateHandler handler);
void monitor_get_snapshot(MonitorSnapshot *out);

// True if the app was launched by the worker, or a background episode alert is
// waiting to be shown. Clears the pending flag. Fills `ep` if it returns true.
bool monitor_take_pending_alert(Episode *ep);

#ifdef CARDIA_DEBUG
// Inject a synthetic irregular rhythm (local engine, or forwarded to the worker
// when background mode is active).
void monitor_debug_inject_irregular(void);
void monitor_debug_set_background(bool on);
#endif
