#include <pebble.h>
#include "win_settings.h"
#include "../config.h"
#include "../settings.h"
#include "../monitor.h"

enum { ROW_BACKGROUND, ROW_ALERTS, ROW_NOTIFY, ROW_SENS, ROW_TACHY, ROW_BRADY, ROW_RATE, ROW_ABOUT, ROW_COUNT };

#define COL_BG        GColorBlack
#define COL_HILITE    GColorFolly
#define COL_ACCENT    GColorVividCerulean
#define COL_ON        GColorIslamicGreen
#define COL_PENDING   GColorChromeYellow
#define COL_OFF       GColorLightGray

static Window *s_window;
static MenuLayer *s_menu;
static Window *s_about;
static TextLayer *s_about_text;
static char s_bg_sub[24];

static const char s_about_body[] =
  "Cardia v" APP_VERSION "\n\n"
  "Samples heart rate and beat-to-beat intervals more often than the watch "
  "does by default, and flags sustained high or low rate and irregular "
  "rhythm.\n\n"
  "This is NOT a medical device. The optical sensor is not an ECG and cannot "
  "diagnose atrial fibrillation. A flag means \"look closer\", not "
  "\"you have a condition\". If flags repeat, take the history log to a "
  "clinician.\n\n"
  "Faster sampling uses more battery. 'Auto' lets the system pace the heart "
  "rate but still keeps beat intervals coming for rhythm analysis.\n\n"
  "Background monitor: when on, a worker keeps monitoring after you leave the "
  "app and across reboots. On an episode it launches the app to alert you. "
  "When off, monitoring only runs while the app is open.\n\n"
  "Alerts vibrate the watch and show the rhythm screen. Notification posts a "
  "card to the watch's notification feed (and the phone) that stays until you "
  "dismiss it. Either can be turned off independently.\n\n"
  "The optical sensor's beat-to-beat data is noisy. Cardia rejects "
  "implausible intervals and stays quiet when the signal is poor, so a real "
  "irregular-rhythm flag needs a clean reading held for ~25 s.";

static uint16_t get_num_rows(MenuLayer *m, uint16_t s, void *c) { return ROW_COUNT; }
static int16_t cell_height(MenuLayer *m, MenuIndex *i, void *c) { return 46; }

// One row: coloured left stripe, white title, state-coloured value line.
static void row(GContext *ctx, const Layer *cell, const char *title,
                const char *value, GColor accent) {
  GRect b = layer_get_bounds(cell);
  bool sel = menu_cell_layer_is_highlighted(cell);

  if (!sel) {
    graphics_context_set_fill_color(ctx, accent);
    graphics_fill_rect(ctx, GRect(0, 0, 4, b.size.h), 0, GCornerNone);
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(11, 2, b.size.w - 16, 22),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (value) {
    graphics_context_set_text_color(ctx, sel ? GColorWhite : accent);
    graphics_draw_text(ctx, value, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(11, 24, b.size.w - 16, 18),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *c) {
  Settings *cfg = settings_get();
  char val[20];
  switch (idx->row) {
    case ROW_BACKGROUND: {
      GColor accent;
      if (!cfg->background_on) {
        strncpy(s_bg_sub, "Off - app must be open", sizeof(s_bg_sub));
        accent = COL_OFF;
      } else if (monitor_background_active()) {
        strncpy(s_bg_sub, "On - worker running", sizeof(s_bg_sub));
        accent = COL_ON;
      } else {
        strncpy(s_bg_sub, "On - starting...", sizeof(s_bg_sub));
        accent = COL_PENDING;
      }
      row(ctx, cell, "Background monitor", s_bg_sub, accent);
      break;
    }
    case ROW_ALERTS:
      row(ctx, cell, "Alerts", cfg->alerts_on ? "Vibrate + screen" : "Off",
          cfg->alerts_on ? COL_ON : COL_OFF);
      break;
    case ROW_NOTIFY:
      row(ctx, cell, "Notification", cfg->notify_on ? "On" : "Off",
          cfg->notify_on ? COL_ON : COL_OFF);
      break;
    case ROW_SENS:
      row(ctx, cell, "Sensitivity", settings_sensitivity_name(), COL_ACCENT);
      break;
    case ROW_TACHY:
      snprintf(val, sizeof(val), "%u bpm", cfg->tachy_bpm);
      row(ctx, cell, "High-rate flag", val, COL_ACCENT);
      break;
    case ROW_BRADY:
      snprintf(val, sizeof(val), "%u bpm", cfg->brady_bpm);
      row(ctx, cell, "Low-rate flag", val, COL_ACCENT);
      break;
    case ROW_RATE:
      row(ctx, cell, "Sample rate", settings_sample_rate_name(), COL_ACCENT);
      break;
    case ROW_ABOUT:
      row(ctx, cell, "About & safety", NULL, COL_PENDING);
      break;
  }
}

static void about_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  window_set_background_color(w, GColorBlack);
  s_about_text = text_layer_create(GRect(6, 4, b.size.w - 12, b.size.h - 8));
  text_layer_set_text(s_about_text, s_about_body);
  text_layer_set_font(s_about_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_about_text, GColorWhite);
  text_layer_set_background_color(s_about_text, GColorClear);
  // wrap in a scroll layer
  ScrollLayer *sl = scroll_layer_create(b);
  scroll_layer_set_click_config_onto_window(sl, w);
  GSize used = text_layer_get_content_size(s_about_text);
  text_layer_set_size(s_about_text, GSize(b.size.w - 12, used.h + 200));
  scroll_layer_set_content_size(sl, GSize(b.size.w, used.h + 24));
  scroll_layer_add_child(sl, text_layer_get_layer(s_about_text));
  layer_add_child(root, scroll_layer_get_layer(sl));
  layer_set_hidden((Layer *)sl, false);
  window_set_user_data(w, sl);
}

static void about_unload(Window *w) {
  ScrollLayer *sl = window_get_user_data(w);
  text_layer_destroy(s_about_text);
  scroll_layer_destroy(sl);
  window_destroy(s_about);
  s_about = NULL;
}

static void select_click(MenuLayer *m, MenuIndex *idx, void *ctx) {
  switch (idx->row) {
    case ROW_BACKGROUND: {
      Settings *cfg = settings_get();
      MonitorBgResult r = monitor_set_background(!cfg->background_on);
      if (r == MON_BG_NO_WORKER) {
        strncpy(s_bg_sub, "No worker in build", sizeof(s_bg_sub));
      } else if (r == MON_BG_ASKING) {
        strncpy(s_bg_sub, "Confirm on watch...", sizeof(s_bg_sub));
      } else if (r == MON_BG_ERROR) {
        strncpy(s_bg_sub, "Could not start", sizeof(s_bg_sub));
      }
      menu_layer_reload_data(m);
      return;
    }
    case ROW_ALERTS: settings_toggle_alerts(); break;
    case ROW_NOTIFY: settings_toggle_notify(); break;
    case ROW_SENS:   settings_cycle_sensitivity(); break;
    case ROW_TACHY:  settings_cycle_tachy(); break;
    case ROW_BRADY:  settings_cycle_brady(); break;
    case ROW_RATE:   settings_cycle_sample_rate(); monitor_apply_sample_rate(); break;
    case ROW_ABOUT: {
      s_about = window_create();
      window_set_window_handlers(s_about, (WindowHandlers){
        .load = about_load, .unload = about_unload });
      window_stack_push(s_about, true);
      return;
    }
  }
  settings_save();
  menu_layer_reload_data(m);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  window_set_background_color(w, COL_BG);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, COL_BG, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, COL_HILITE, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_appear(Window *w) {
  // The worker launch may have completed (or been confirmed) while we were away.
  if (settings_get()->background_on && !monitor_running()) monitor_start();
  menu_layer_reload_data(s_menu);
}

static void window_unload(Window *w) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void win_settings_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .appear = window_appear, .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
