#include <pebble.h>
#include "win_monitor.h"
#include "../config.h"
#include "win_history.h"
#include "win_settings.h"
#include "../app.h"
#include "../monitor.h"
#include "../analysis.h"
#include "../episodes.h"
#include "../graph.h"
#include "../settings.h"

static Window *s_window;
static Layer *s_canvas;
static char s_bpm_text[8];
static char s_status_text[24];
static char s_metrics_text[36];
static char s_banner_text[32];

static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  MonitorSnapshot snap;
  monitor_get_snapshot(&snap);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int pad = 6;
  int y = pad;

  // --- big heart rate ------------------------------------------------
  if (snap.bpm > 0) snprintf(s_bpm_text, sizeof(s_bpm_text), "%u", snap.bpm);
  else strncpy(s_bpm_text, "--", sizeof(s_bpm_text));

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_bpm_text, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
                     GRect(pad, y - 4, b.size.w - 60, 44),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "BPM", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(b.size.w - 52, y + 14, 46, 20),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
  if (monitor_background_active()) {
    graphics_context_set_text_color(ctx, GColorIslamicGreen);
    graphics_draw_text(ctx, "BG", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(b.size.w - 52, y - 2, 46, 16),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);
  }
  y += 44;

  // --- rhythm status ----------------------------------------------
  if (!snap.hr_available) {
    strncpy(s_status_text, "Enable Heart Rate", sizeof(s_status_text));
  } else {
    const char *n = rhythm_name(snap.status);
    if (snap.status == RHYTHM_NORMAL && snap.n_intervals >= 20)
      snprintf(s_status_text, sizeof(s_status_text), "%s rhythm", n);
    else
      strncpy(s_status_text, n, sizeof(s_status_text));
  }
  graphics_context_set_text_color(ctx, rhythm_color(snap.status));
  graphics_draw_text(ctx, s_status_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(pad, y, b.size.w - 2 * pad, 26),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  y += 28;

  // --- episode banner ----------------------------------------------
  if (snap.episode_active) {
    uint32_t dur = time(NULL) - snap.episode_start;
    char d[10]; episode_format_duration(dur, d, sizeof(d));
    snprintf(s_banner_text, sizeof(s_banner_text), "● %s  %s",
             rhythm_name((RhythmStatus)snap.episode_type), d);
    graphics_context_set_fill_color(ctx, GColorFolly);
    graphics_fill_rect(ctx, GRect(0, y, b.size.w, 22), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_banner_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(pad, y, b.size.w - 2 * pad, 22),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    y += 24;
  }

  // --- graph ------------------------------------------------------
  int graph_h = b.size.h - y - 26 - pad;
  if (graph_h > 40) {
    static uint8_t series[BPM_BUF_LEN];
    int n = analysis_bpm_series(series, BPM_BUF_LEN);
    Settings *cfg = settings_get();
    GraphConfig gc = {
      .values = series, .count = n,
      .tachy_bpm = cfg->tachy_bpm, .brady_bpm = cfg->brady_bpm,
      .show_trend = true, .line_color = GColorWhite,
    };
    graph_render(ctx, GRect(pad, y, b.size.w - 2 * pad, graph_h), &gc);
    y += graph_h + 4;
  }

  // --- HRV metrics footer -------------------------------------------
  if (snap.n_intervals >= 20) {
    snprintf(s_metrics_text, sizeof(s_metrics_text), "RMSSD %ums  pNN50 %u%%  n%u",
             snap.rmssd_ms, snap.pnn50_pct, snap.n_intervals);
  } else {
    snprintf(s_metrics_text, sizeof(s_metrics_text), "collecting beats  n%u", snap.n_intervals);
  }
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_metrics_text, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(pad, b.size.h - 20, b.size.w - 2 * pad, 18),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // motion dot
  if (snap.motion > 40) {
    graphics_context_set_fill_color(ctx, GColorChromeYellow);
    graphics_fill_circle(ctx, GPoint(b.size.w - pad - 4, b.size.h - 11), 4);
  }
}

static void on_monitor_update(const MonitorSnapshot *snap) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef r, void *ctx)   { win_history_push(); }
static void down_click(ClickRecognizerRef r, void *ctx)  { win_settings_push(); }
static void select_click(ClickRecognizerRef r, void *ctx) {
  settings_cycle_sample_rate();
  settings_save();
  monitor_apply_sample_rate();
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, update_proc);
  layer_add_child(root, s_canvas);
}

static void window_appear(Window *w) {
  monitor_set_handler(on_monitor_update);
  if (!monitor_running()) monitor_start();
  layer_mark_dirty(s_canvas);
}

static void window_unload(Window *w) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  s_window = NULL;
}

void win_monitor_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load, .appear = window_appear, .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}
