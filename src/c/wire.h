#pragma once
#include "app.h"
#include "config.h"

// Compact packing of engine state into the 6-byte AppWorkerMessage that the
// background worker and the foreground app exchange.

static inline void wire_pack_snapshot(const MonitorSnapshot *s, AppWorkerMessage *m) {
  m->data0 = (uint16_t)s->bpm
           | ((uint16_t)(s->status & 0x7) << 8)
           | ((uint16_t)(s->episode_active ? 1 : 0) << 11)
           | ((uint16_t)(s->hr_available ? 1 : 0) << 12);
  m->data1 = s->rmssd_ms;
  m->data2 = (uint16_t)s->pnn50_pct | ((uint16_t)s->score << 8);
}

static inline void wire_unpack_snapshot(const AppWorkerMessage *m, MonitorSnapshot *s) {
  s->bpm = m->data0 & 0xFF;
  s->bpm_raw = s->bpm;
  s->status = (RhythmStatus)((m->data0 >> 8) & 0x7);
  s->episode_active = (m->data0 >> 11) & 1;
  s->hr_available = (m->data0 >> 12) & 1;
  s->rmssd_ms = m->data1;
  s->pnn50_pct = m->data2 & 0xFF;
  s->score = (m->data2 >> 8) & 0xFF;
  s->from_worker = true;
  s->n_intervals = (s->rmssd_ms > 0) ? RR_MIN_INTERVALS : 0;
}

static inline void wire_pack_episode(const Episode *e, AppWorkerMessage *m) {
  m->data0 = e->type;
  m->data1 = e->duration_s;
  m->data2 = (uint16_t)e->hr_peak | ((uint16_t)e->hr_min << 8);
}

static inline void wire_unpack_episode(const AppWorkerMessage *m, Episode *e) {
  e->type = m->data0 & 0xFF;
  e->duration_s = m->data1;
  e->hr_peak = m->data2 & 0xFF;
  e->hr_min = (m->data2 >> 8) & 0xFF;
}
