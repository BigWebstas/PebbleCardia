#include <pebble.h>
#include "win_alert.h"
#include "../config.h"
#include "../app.h"
#include "../settings.h"

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_auto_dismiss;
static Episode s_ep;
static char s_line1[24];
static char s_line2[48];

static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GColor accent = rhythm_color((RhythmStatus)s_ep.type);

  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, 40), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "HEART RHYTHM", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 8, b.size.w, 24), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_line1, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(6, 54, b.size.w - 12, 34), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_line2, fonts_get_system_font(FONT_KEY_GOTHIC_24),
                     GRect(6, 92, b.size.w - 12, 60), GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, "Not a diagnosis", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(6, b.size.h - 42, b.size.w - 12, 18), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, "press any button", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(6, b.size.h - 24, b.size.w - 12, 18), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void dismiss(ClickRecognizerRef r, void *ctx) {
  window_stack_remove(s_window, true);
}

static void auto_dismiss(void *ctx) {
  s_auto_dismiss = NULL;
  if (s_window) window_stack_remove(s_window, true);
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, dismiss);
  window_single_click_subscribe(BUTTON_ID_UP, dismiss);
  window_single_click_subscribe(BUTTON_ID_DOWN, dismiss);
  window_single_click_subscribe(BUTTON_ID_BACK, dismiss);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, update_proc);
  layer_add_child(root, s_canvas);
}

static void window_appear(Window *w) {
  if (settings_get()->alerts_on) {
    static const uint32_t seg[] = { 200, 120, 200, 120, 400 };
    VibePattern pat = { .durations = seg, .num_segments = ARRAY_LENGTH(seg) };
    vibes_enqueue_custom_pattern(pat);
  }
  s_auto_dismiss = app_timer_register(30000, auto_dismiss, NULL);
}

static void window_unload(Window *w) {
  if (s_auto_dismiss) { app_timer_cancel(s_auto_dismiss); s_auto_dismiss = NULL; }
  layer_destroy(s_canvas);
  window_destroy(s_window);
  s_window = NULL;
}

void win_alert_push(const Episode *ep) {
  s_ep = *ep;
  snprintf(s_line1, sizeof(s_line1), "%s", rhythm_name((RhythmStatus)s_ep.type));
  if (s_ep.type == RHYTHM_IRREGULAR) {
    snprintf(s_line2, sizeof(s_line2), "Irregular beat pattern while you were still");
  } else if (s_ep.type == RHYTHM_ELEVATED) {
    snprintf(s_line2, sizeof(s_line2), "Rate stayed high (peak %u) at rest", s_ep.hr_peak);
  } else {
    snprintf(s_line2, sizeof(s_line2), "Rate stayed low (down to %u)", s_ep.hr_min);
  }

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .appear = window_appear, .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
