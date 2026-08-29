#pragma once
#include "app.h"

// The monitoring engine: sensor subscriptions, motion gating, rhythm
// classification and the episode state machine. Compiled into BOTH the
// foreground app and the background worker. It never vibrates or talks to the
// phone - the embedder does that from the callbacks below.

typedef struct {
  // Called every tick (~TICK_MS) with the freshly computed state.
  void (*on_tick)(const MonitorSnapshot *snap);
  // Called once when an abnormal state has been confirmed. The episode is
  // still open; `ep->duration_s` is 0.
  void (*on_episode_open)(const Episode *ep);
  // Called once when the episode has cleared. The engine has already persisted
  // it via episodes_add() before this fires.
  void (*on_episode_close)(const Episode *ep);
} EngineCallbacks;

void engine_init(const EngineCallbacks *cb);
void engine_start(void);            // subscribe sensors, apply sample rate, start tick
void engine_stop(void);             // unsubscribe, release the sensor
bool engine_running(void);
void engine_apply_sample_rate(void); // re-read Settings.sample_rate_s and apply

void engine_get_snapshot(MonitorSnapshot *out);

// Save the BPM graph ring to persistent storage (also done automatically on
// stop and every ~30 s while running).
void engine_persist_bpm_history(void);

#ifdef CARDIA_DEBUG
void engine_debug_inject_irregular(void);
#endif
