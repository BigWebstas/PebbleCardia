#pragma once
#include "platform.h"

// Rhythm classification produced by the analysis engine each tick.
typedef enum {
  RHYTHM_NO_SIGNAL = 0,  // sensor not producing usable readings
  RHYTHM_NORMAL,
  RHYTHM_ELEVATED,       // sustained high rate (tachycardia range)
  RHYTHM_LOW,            // sustained low rate (bradycardia range)
  RHYTHM_IRREGULAR,      // beat-to-beat pattern is irregularly irregular
} RhythmStatus;

// A finalised abnormal episode, stored on watch and pushed to the phone.
typedef struct __attribute__((__packed__)) {
  uint32_t start;        // UTC seconds
  uint16_t duration_s;
  uint8_t  type;         // RhythmStatus (ELEVATED / LOW / IRREGULAR)
  uint8_t  hr_peak;      // bpm
  uint8_t  hr_min;       // bpm
  uint8_t  score;        // peak irregularity score during the episode (0..100)
} Episode;                // 10 bytes

#define MAX_EPISODES 20   // 20 * 10 = 200 bytes, fits one persist key

// A snapshot of the current monitoring state for the UI.
typedef struct {
  RhythmStatus status;
  uint8_t  bpm;           // smoothed heart rate
  uint8_t  bpm_raw;       // most recent raw sample
  uint16_t rmssd_ms;
  uint8_t  pnn50_pct;
  uint16_t sdnn_ms;
  uint8_t  score;         // current irregularity score
  uint8_t  n_intervals;   // beat intervals in the analysis window
  uint8_t  motion;        // 0..100 relative movement level
  bool     hr_available;  // sensor / user preference allows heart rate
  bool     episode_active;
  uint32_t episode_start; // UTC seconds, valid when episode_active
  uint8_t  episode_type;
  bool     from_worker;   // true if this snapshot came from the background worker
} MonitorSnapshot;

const char *rhythm_name(RhythmStatus s);
#if !defined(CARDIA_WORKER)
GColor rhythm_color(RhythmStatus s);
#endif
