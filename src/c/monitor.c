#include "monitor.h"
#include "config.h"
#include "settings.h"
#include "engine.h"
#include "analysis.h"
#include "episodes.h"
#include "comm.h"
#include "wire.h"

// The app-side orchestrator. Two modes:
//  - LOCAL: run the shared engine in the app process (background monitor off).
//  - ATTACHED: the background worker runs the engine; the app just renders the
//    snapshots and episode events it streams over app_worker_send_message().

static bool s_attached;          // true = worker is the data source
static MonitorUpdateHandler s_handler;
static MonitorSnapshot s_snap;
static int s_status_divider;

// --- shared engine callbacks (LOCAL mode) ---------------------------------
static void on_tick(const MonitorSnapshot *snap) {
  s_snap = *snap;
  s_snap.from_worker = false;
  if (s_handler) s_handler(&s_snap);
  if (++s_status_divider >= 5) { s_status_divider = 0; comm_send_status(&s_snap); }
}

static void vibe_alert(void) {
  if (!settings_get()->alerts_on) return;
  static const uint32_t seg[] = { 120, 80, 120, 80, 250 };
  VibePattern pat = { .durations = seg, .num_segments = ARRAY_LENGTH(seg) };
  vibes_enqueue_custom_pattern(pat);
}

static void on_episode_open(const Episode *ep) {
  vibe_alert();
  comm_send_episode(ep, true);
}

static void on_episode_close(const Episode *ep) {
  comm_send_episode(ep, false);
}

static const EngineCallbacks s_engine_cb = {
  .on_tick = on_tick,
  .on_episode_open = on_episode_open,
  .on_episode_close = on_episode_close,
};

// --- worker message stream (ATTACHED mode) -------------------------------
static void worker_message(uint16_t type, AppWorkerMessage *data) {
  switch (type) {
    case WMSG_SNAPSHOT: {
      MonitorSnapshot s = {0};
      wire_unpack_snapshot(data, &s);
      // keep a stable episode_start for the banner timer
      if (s.episode_active && !s_snap.episode_active) s.episode_start = time(NULL);
      else if (s.episode_active) s.episode_start = s_snap.episode_start;
      s.episode_type = s_snap.episode_type;
      // feed the local BPM ring so the on-screen graph works in background mode
      if (s.bpm > 0) analysis_add_bpm(s.bpm, time(NULL));
      s_snap = s;
      if (s_handler) s_handler(&s_snap);
      if (++s_status_divider >= 5) { s_status_divider = 0; comm_send_status(&s_snap); }
      break;
    }
    case WMSG_EPISODE_OPEN: {
      Episode ep = {0};
      wire_unpack_episode(data, &ep);
      s_snap.episode_active = true;
      s_snap.episode_type = ep.type;
      s_snap.episode_start = time(NULL);
      vibe_alert();
      comm_send_episode(&ep, true);
      if (s_handler) s_handler(&s_snap);
      break;
    }
    case WMSG_EPISODE_CLOSE: {
      Episode ep = {0};
      wire_unpack_episode(data, &ep);
      s_snap.episode_active = false;
      episodes_load();               // worker already persisted it
      const Episode *stored = episodes_get(0);
      comm_send_episode(stored ? stored : &ep, false);
      if (s_handler) s_handler(&s_snap);
      break;
    }
    default:
      break;
  }
}

// --- mode switching -----------------------------------------------------
static void go_local(void) {
  if (s_attached) { app_worker_message_unsubscribe(); s_attached = false; }
  engine_init(&s_engine_cb);
  engine_start();
}

static void notify_worker(uint8_t type) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage m = {0};                 // never pass NULL to the plugin bus
  app_worker_send_message(type, &m);
}

static void go_attached(void) {
  if (engine_running()) engine_stop();
  if (!s_attached) {
    analysis_reset();
    app_worker_message_subscribe(worker_message);
    s_attached = true;
  }
  notify_worker(WMSG_SETTINGS_DIRTY);        // pick up current settings
}

// --- public API -------------------------------------------------------
void monitor_init(void) {
  s_snap = (MonitorSnapshot){ .hr_available = true };
}

void monitor_deinit(void) {
  if (engine_running()) engine_stop();
  if (s_attached) { app_worker_message_unsubscribe(); s_attached = false; }
}

void monitor_start(void) {
  // When background mode is on the worker owns the engine and the app ONLY
  // renders its stream - never its own engine, or the two would both subscribe
  // the shared HRM session and crash. If the worker isn't up yet (e.g. just
  // after an install, before worker_manager relaunches it) make sure it starts,
  // then attach and wait for its first snapshot.
  if (settings_get()->background_on) {
    if (!app_worker_is_running()) app_worker_launch();
    go_attached();
    return;
  }
  go_local();
}

void monitor_stop(void) {
  // Leave the worker running if background mode is on; just detach the UI.
  if (engine_running() && !settings_get()->background_on) engine_stop();
  if (s_attached) { app_worker_message_unsubscribe(); s_attached = false; }
}

bool monitor_running(void) {
  return engine_running() || s_attached;
}

bool monitor_background_active(void) {
  return s_attached && app_worker_is_running();
}

MonitorBgResult monitor_set_background(bool on) {
  Settings *cfg = settings_get();
  if (on) {
    AppWorkerResult r = app_worker_launch();
    switch (r) {
      case APP_WORKER_RESULT_SUCCESS:
      case APP_WORKER_RESULT_ALREADY_RUNNING:
        cfg->background_on = true; settings_save();
        go_attached();
        return MON_BG_OK;
      case APP_WORKER_RESULT_ASKING_CONFIRMATION:
        cfg->background_on = true; settings_save();
        return MON_BG_ASKING;
      case APP_WORKER_RESULT_NO_WORKER:
        return MON_BG_NO_WORKER;
      default:
        return MON_BG_ERROR;
    }
  } else {
    app_worker_kill();
    cfg->background_on = false; settings_save();
    if (s_attached) { app_worker_message_unsubscribe(); s_attached = false; }
    go_local();
    return MON_BG_OK;
  }
}

void monitor_apply_sample_rate(void) {
  if (s_attached) notify_worker(WMSG_SETTINGS_DIRTY);
  else engine_apply_sample_rate();
}

void monitor_set_handler(MonitorUpdateHandler handler) { s_handler = handler; }

void monitor_get_snapshot(MonitorSnapshot *out) {
  if (s_attached) *out = s_snap;
  else engine_get_snapshot(out);
}

#ifdef CARDIA_DEBUG
void monitor_debug_inject_irregular(void) {
  if (s_attached) {
    // Background mode: ask the worker to inject into its own engine so the
    // worker-driven episode + auto-launch path gets exercised.
    notify_worker(WMSG_DEBUG_INJECT);
    return;
  }
  if (!engine_running()) { engine_init(&s_engine_cb); engine_start(); }
  engine_debug_inject_irregular();
}

void monitor_debug_set_background(bool on) {
  MonitorBgResult r = monitor_set_background(on);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "debug set background %d -> result %d", on, (int)r);
}
#endif

bool monitor_take_pending_alert(Episode *ep) {
  if (!persist_exists(PKEY_PENDING_ALERT)) return false;
  Episode e = {0};
  persist_read_data(PKEY_PENDING_ALERT, &e, sizeof(e));
  persist_delete(PKEY_PENDING_ALERT);
  if (ep) *ep = e;
  return true;
}
