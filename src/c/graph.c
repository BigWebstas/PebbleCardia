#include "graph.h"

// Pick a rounded [min,max] bpm range that covers the data and both
// reference lines, with a little headroom.
static void pick_range(const GraphConfig *cfg, int *out_min, int *out_max) {
  int lo = 255, hi = 0;
  for (int i = 0; i < cfg->count; i++) {
    int v = cfg->values[i];
    if (v == 0) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (hi == 0) { lo = 50; hi = 100; }             // no data yet
  if (cfg->tachy_bpm) hi = hi > cfg->tachy_bpm ? hi : cfg->tachy_bpm;
  if (cfg->brady_bpm) lo = lo < cfg->brady_bpm ? lo : cfg->brady_bpm;
  lo -= 8; hi += 8;
  lo = (lo / 10) * 10;
  hi = ((hi + 9) / 10) * 10;
  if (hi - lo < 30) hi = lo + 30;
  if (lo < 30) lo = 30;
  *out_min = lo; *out_max = hi;
}

void graph_render(GContext *ctx, GRect r, const GraphConfig *cfg) {
  int vmin, vmax;
  pick_range(cfg, &vmin, &vmax);
  const int span = vmax - vmin;
  const int x0 = r.origin.x, y0 = r.origin.y, w = r.size.w, h = r.size.h;

#define Y_OF(v) (y0 + h - 1 - ((int)((v) - vmin) * (h - 1)) / span)

  // frame + horizontal grid every ~20 bpm
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_rect(ctx, r);
  for (int g = ((vmin + 19) / 20) * 20; g < vmax; g += 20) {
    int y = Y_OF(g);
    for (int x = x0 + 1; x < x0 + w - 1; x += 4) graphics_draw_pixel(ctx, GPoint(x, y));
  }

  // reference lines
  graphics_context_set_stroke_color(ctx, GColorFolly);
  if (cfg->tachy_bpm >= vmin && cfg->tachy_bpm <= vmax) {
    int y = Y_OF(cfg->tachy_bpm);
    for (int x = x0; x < x0 + w; x += 3) graphics_draw_pixel(ctx, GPoint(x, y));
  }
  graphics_context_set_stroke_color(ctx, GColorVividCerulean);
  if (cfg->brady_bpm >= vmin && cfg->brady_bpm <= vmax) {
    int y = Y_OF(cfg->brady_bpm);
    for (int x = x0; x < x0 + w; x += 3) graphics_draw_pixel(ctx, GPoint(x, y));
  }

  if (cfg->count < 2) return;

  // map sample index -> x (right-aligned; newest at the right edge)
  const int n = cfg->count;
#define X_OF(i) (x0 + w - 1 - ((n - 1 - (i)) * (w - 1)) / (n - 1 > 0 ? n - 1 : 1))

  // sample polyline
  graphics_context_set_stroke_color(ctx, cfg->line_color);
  graphics_context_set_stroke_width(ctx, 2);
  int px = 0, py = 0; bool have_prev = false;
  for (int i = 0; i < n; i++) {
    if (cfg->values[i] == 0) { have_prev = false; continue; }
    int cx = X_OF(i), cy = Y_OF(cfg->values[i]);
    if (have_prev) graphics_draw_line(ctx, GPoint(px, py), GPoint(cx, cy));
    px = cx; py = cy; have_prev = true;
  }
  graphics_context_set_stroke_width(ctx, 1);

  // trend line: least-squares fit over valid samples
  if (cfg->show_trend) {
    int64_t sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
    for (int i = 0; i < n; i++) {
      if (cfg->values[i] == 0) continue;
      sx += i; sy += cfg->values[i];
      sxx += (int64_t)i * i; sxy += (int64_t)i * cfg->values[i];
      m++;
    }
    if (m >= 3) {
      int64_t denom = (int64_t)m * sxx - sx * sx;
      if (denom != 0) {
        // slope and intercept scaled by 1000 to keep integer math honest
        int64_t slope_k = ((int64_t)m * sxy - sx * sy) * 1000 / denom;
        int64_t icept_k = (sy * 1000 - slope_k * sx) / m;
        int v_first = (int)((slope_k * 0 + icept_k) / 1000);
        int v_last  = (int)((slope_k * (n - 1) + icept_k) / 1000);
        v_first = v_first < vmin ? vmin : (v_first > vmax ? vmax : v_first);
        v_last  = v_last  < vmin ? vmin : (v_last  > vmax ? vmax : v_last);
        graphics_context_set_stroke_color(ctx, GColorYellow);
        graphics_draw_line(ctx, GPoint(X_OF(0), Y_OF(v_first)),
                                GPoint(X_OF(n - 1), Y_OF(v_last)));
      }
    }
  }

  // current-value marker
  for (int i = n - 1; i >= 0; i--) {
    if (cfg->values[i] == 0) continue;
    graphics_context_set_fill_color(ctx, cfg->line_color);
    graphics_fill_circle(ctx, GPoint(X_OF(i), Y_OF(cfg->values[i])), 3);
    break;
  }
}
