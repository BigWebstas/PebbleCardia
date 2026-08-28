#include "comm.h"
#include "config.h"
#ifdef CARDIA_DEBUG
#include "monitor.h"
#endif

static void inbox_received(DictionaryIterator *iter, void *ctx) {
#ifdef CARDIA_DEBUG
  // pebble send-app-message --phone <ip> --int 10013=<n>
  //   1 = inject a synthetic irregular burst   2 = background on   3 = background off
  Tuple *t = dict_find(iter, MESSAGE_KEY_DBG_TRIGGER);
  if (t) {
    int32_t v = t->value->int32;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "debug trigger %d", (int)v);
    switch (v) {
      case 2:  monitor_debug_set_background(true);  break;
      case 3:  monitor_debug_set_background(false); break;
      default: monitor_debug_inject_irregular();    break;
    }
  }
#endif
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage outbox failed: 0x%x", reason);
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(64, 128);
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}

static bool begin(DictionaryIterator **iter) {
  return app_message_outbox_begin(iter) == APP_MSG_OK;
}

void comm_send_status(const MonitorSnapshot *snap) {
  DictionaryIterator *it;
  if (!begin(&it)) return;
  dict_write_uint8(it, MESSAGE_KEY_MSG_KIND, MSG_KIND_STATUS);
  dict_write_uint8(it, MESSAGE_KEY_BPM, snap->bpm);
  dict_write_uint8(it, MESSAGE_KEY_STATUS, (uint8_t)snap->status);
  dict_write_uint16(it, MESSAGE_KEY_RMSSD, snap->rmssd_ms);
  dict_write_uint8(it, MESSAGE_KEY_PNN50, snap->pnn50_pct);
  dict_write_uint16(it, MESSAGE_KEY_SDNN, snap->sdnn_ms);
  dict_write_uint8(it, MESSAGE_KEY_MOTION, snap->motion);
  app_message_outbox_send();
}

void comm_send_episode(const Episode *ep, bool ongoing) {
  DictionaryIterator *it;
  if (!begin(&it)) return;
  dict_write_uint8(it, MESSAGE_KEY_MSG_KIND, MSG_KIND_EPISODE);
  dict_write_uint32(it, MESSAGE_KEY_EP_START, ep->start);
  dict_write_uint16(it, MESSAGE_KEY_EP_DURATION, ep->duration_s);
  dict_write_uint8(it, MESSAGE_KEY_EP_ONGOING, ongoing ? 1 : 0);
  dict_write_uint8(it, MESSAGE_KEY_EP_HR_PEAK, ep->hr_peak);
  dict_write_uint8(it, MESSAGE_KEY_EP_HR_MIN, ep->hr_min);
  dict_write_uint8(it, MESSAGE_KEY_EP_SCORE, ep->score);
  dict_write_uint8(it, MESSAGE_KEY_STATUS, ep->type);
  app_message_outbox_send();
}
