#pragma once
#include "platform.h"
#include "app.h"

typedef struct {
  uint8_t  n_intervals;
  uint16_t mean_rr_ms;
  uint16_t sdnn_ms;      // standard deviation of intervals
  uint16_t rmssd_ms;     // root mean square of successive differences
  uint8_t  pnn50_pct;    // % of successive differences > 50 ms
  uint8_t  cv_pct;       // coefficient of variation (sdnn / mean)
  uint8_t  score;        // 0..100 irregularity score
  uint8_t  hr_from_rr;   // bpm implied by mean interval (0 if n too small)
} AnalysisResult;

void analysis_reset(void);

// Feed data. `now` is UTC seconds. BPM values of 0 are ignored.
void analysis_add_rr(uint16_t ppi_ms, time_t now);
void analysis_add_bpm(uint8_t bpm, time_t now);

// Most recent raw bpm and its age in seconds (age = INT32_MAX if none).
uint8_t analysis_last_bpm(void);
int     analysis_last_bpm_age(time_t now);

// Exponentially smoothed bpm (0 until first sample).
uint8_t analysis_smoothed_bpm(void);

// Seconds since the first beat-interval sample after a reset (0 if none yet).
// Used to ignore the noisy sensor-acquisition period.
int analysis_rr_stream_age(time_t now);

// 0..100: recent fraction of incoming beat intervals that survived artifact
// rejection. Low means the optical sensor isn't tracking cleanly.
uint8_t analysis_signal_quality(void);

// Compute HRV metrics over the recent window. Returns false if not enough data.
bool analysis_compute(time_t now, AnalysisResult *out);

// Copy the last `max` bpm samples (oldest first) into `dst`.
// Returns the count written.
int analysis_bpm_series(uint8_t *dst, int max);

// Seed the graph ring from a saved series (oldest first) - keeps the graph
// populated across app restarts and when attaching to the background worker.
void analysis_bpm_restore(const uint8_t *src, int n);
