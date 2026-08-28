#include <pebble.h>
#include "config.h"
#include "settings.h"
#include "episodes.h"
#include "comm.h"
#include "monitor.h"
#include "windows/win_monitor.h"
#include "windows/win_alert.h"

static void init(void) {
  settings_load();
  episodes_load();
  comm_init();
  monitor_init();

  win_monitor_push();

  // If the background worker flagged an episode while we were closed, show it.
  Episode pending;
  if (monitor_take_pending_alert(&pending)) {
    win_alert_push(&pending);
  }

  monitor_start();
}

static void deinit(void) {
  monitor_deinit();
  comm_deinit();
  settings_save();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
