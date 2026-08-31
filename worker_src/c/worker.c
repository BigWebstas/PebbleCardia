#include "../../src/c/platform.h"      // -> <pebble_worker.h>
#include "../../src/c/config.h"
#include "../../src/c/settings.h"
#include "../../src/c/episodes.h"
#include "../../src/c/engine.h"
#include "../../src/c/wire.h"

// Cardia background worker.
//
// Runs the shared monitoring engine continuously - it keeps going after the app
// exits and is relaunched by PebbleOS on reboot (app_worker_launch sets this as
// the default worker). It streams packed snapshots to the app when the app is
// open, persists finalised episodes itself, and on a confirmed episode leaves a
// "pending alert" record and launches the app so the user gets a visible,
// buzzing alert.

static void send_snapshot(const MonitorSnapshot *snap) {
  AppWorkerMessage m;
  wire_pack_snapshot(snap, &m);
  app_worker_send_message(WMSG_SNAPSHOT, &m);
}

static void episode_open(const Episode *ep) {
  AppWorkerMessage m;
  wire_pack_episode(ep, &m);
  app_worker_send_message(WMSG_EPISODE_OPEN, &m);

  if (settings_get()->alerts_on || settings_get()->notify_on) {
    // Record it and bring the app up: it shows the alert screen and/or asks
    // pkjs to post a watch notification (the worker can't do either itself).
    persist_write_data(PKEY_PENDING_ALERT, ep, sizeof(*ep));
    worker_launch_app();
  }
}

static void episode_close(const Episode *ep) {
  // engine_* already called episodes_add() before this fires.
  AppWorkerMessage m;
  wire_pack_episode(ep, &m);
  app_worker_send_message(WMSG_EPISODE_CLOSE, &m);
}

static const EngineCallbacks s_cb = {
  .on_tick = send_snapshot,
  .on_episode_open = episode_open,
  .on_episode_close = episode_close,
};

static void app_message(uint16_t type, AppWorkerMessage *data) {
  if (type == WMSG_SETTINGS_DIRTY) {
    settings_load();
    engine_apply_sample_rate();
  }
#ifdef CARDIA_DEBUG
  else if (type == WMSG_DEBUG_INJECT) {
    engine_debug_inject_irregular();
  }
#endif
}

int main(void) {
  settings_load();
  episodes_load();
  app_worker_message_subscribe(app_message);

  engine_init(&s_cb);
  engine_start();

  worker_event_loop();

  engine_stop();
  app_worker_message_unsubscribe();
}
