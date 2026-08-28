#include <pebble.h>
#include "win_history.h"
#include "win_episode.h"
#include "../app.h"
#include "../episodes.h"

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t get_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  int c = episodes_count();
  return c == 0 ? 1 : c + 1;   // +1 for "Clear history"
}

static int16_t cell_height(MenuLayer *m, MenuIndex *idx, void *ctx) {
  return 44;
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *c) {
  int count = episodes_count();
  GRect b = layer_get_bounds(cell);

  if (count == 0) {
    menu_cell_basic_draw(ctx, cell, "No episodes yet", "Watch is monitoring", NULL);
    return;
  }
  if (idx->row == (uint16_t)count) {
    menu_cell_basic_draw(ctx, cell, "Clear history", NULL, NULL);
    return;
  }
  const Episode *ep = episodes_get(idx->row);
  if (!ep) return;
  char title[28], sub[40];
  episode_format_title(ep, title, sizeof(title));
  episode_format_when(ep, sub, sizeof(sub));

  graphics_context_set_fill_color(ctx, rhythm_color((RhythmStatus)ep->type));
  graphics_fill_rect(ctx, GRect(0, 0, 4, b.size.h), 0, GCornerNone);
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
}

static void select_click(MenuLayer *m, MenuIndex *idx, void *ctx) {
  int count = episodes_count();
  if (count == 0) return;
  if (idx->row == (uint16_t)count) {
    episodes_clear();
    menu_layer_reload_data(m);
    return;
  }
  win_episode_push(idx->row);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_appear(Window *w) {
  menu_layer_reload_data(s_menu);
}

static void window_unload(Window *w) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void win_history_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .appear = window_appear, .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
