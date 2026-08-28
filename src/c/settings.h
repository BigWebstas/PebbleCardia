#pragma once
#include "platform.h"

typedef struct __attribute__((__packed__)) {
  bool    alerts_on;
  uint8_t sensitivity;    // 0 low / 1 medium / 2 high
  uint8_t tachy_bpm;
  uint8_t brady_bpm;
  uint8_t sample_rate_s;  // 0 = auto, else HR + HRV sample period in seconds
  bool    background_on;  // run the background worker so monitoring never stops
} Settings;

void settings_load(void);
void settings_save(void);
Settings *settings_get(void);

// Cycle helpers used by the settings menu; each wraps within a sensible range.
void settings_cycle_sensitivity(void);
void settings_cycle_tachy(void);
void settings_cycle_brady(void);
void settings_cycle_sample_rate(void);
void settings_toggle_alerts(void);

const char *settings_sensitivity_name(void);
const char *settings_sample_rate_name(void);
