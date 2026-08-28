#pragma once
#include "platform.h"

typedef struct {
  const uint8_t *values;   // bpm samples, oldest first
  int   count;
  int   tachy_bpm;         // draw a reference line, 0 to skip
  int   brady_bpm;         // draw a reference line, 0 to skip
  bool  show_trend;        // draw a linear-regression trend line
  GColor line_color;
} GraphConfig;

// Draws a heart-rate graph filling `rect`, with grid, reference lines,
// the sample polyline and (optionally) a trend line.
void graph_render(GContext *ctx, GRect rect, const GraphConfig *cfg);
