#include <pebble.h>
#include "win_episode.h"
#include "../app.h"
#include "../episodes.h"

static Window *s_window;
static ScrollLayer *s_scroll;
static TextLayer *s_text;
static char s_body[220];

static void build_body(const Episode *ep) {
  char when[24], dur[12];
  time_t t = ep->start;
  strftime(when, sizeof(when), "%b %e, %Y  %H:%M", localtime(&t));
  episode_format_duration(ep->duration_s, dur, sizeof(dur));

  snprintf(s_body, sizeof(s_body),
           "%s\n\n"
           "Started\n%s\n\n"
           "Duration\n%s\n\n"
           "Heart rate\n%u - %u bpm\n\n"
           "Irregularity score\n%u / 100\n\n"
           "Not a diagnosis. If this repeats, show the log to a clinician.",
           rhythm_name((RhythmStatus)ep->type), when, dur,
           ep->hr_min, ep->hr_peak, ep->score);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_scroll = scroll_layer_create(b);
  scroll_layer_set_click_config_onto_window(s_scroll, w);

  s_text = text_layer_create(GRect(6, 4, b.size.w - 12, 1000));
  text_layer_set_text(s_text, s_body);
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  GSize used = text_layer_get_content_size(s_text);
  text_layer_set_size(s_text, GSize(b.size.w - 12, used.h + 12));
  scroll_layer_set_content_size(s_scroll, GSize(b.size.w, used.h + 20));

  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_text));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

static void window_unload(Window *w) {
  text_layer_destroy(s_text);
  scroll_layer_destroy(s_scroll);
  window_destroy(s_window);
  s_window = NULL;
}

void win_episode_push(int index) {
  const Episode *ep = episodes_get(index);
  if (!ep) return;
  build_body(ep);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
