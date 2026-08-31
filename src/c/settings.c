#include "settings.h"
#include "config.h"

static Settings s_settings;

void settings_load(void) {
  s_settings = (Settings){
    .alerts_on = DEF_ALERTS_ON,
    .notify_on = DEF_NOTIFY_ON,
    .sensitivity = DEF_SENSITIVITY,
    .tachy_bpm = DEF_TACHY_BPM,
    .brady_bpm = DEF_BRADY_BPM,
    .sample_rate_s = DEF_SAMPLE_RATE_S,
    .background_on = DEF_BACKGROUND_ON,
  };
  // Only load if a full record of the current layout was stored - a shorter
  // blob is from an older version and would leave new fields indeterminate.
  if (persist_exists(PKEY_SETTINGS) &&
      persist_get_size(PKEY_SETTINGS) == (int)sizeof(s_settings)) {
    persist_read_data(PKEY_SETTINGS, &s_settings, sizeof(s_settings));
  }
  // Guard against corrupt / out-of-range persisted values.
  if (s_settings.sensitivity > 2) s_settings.sensitivity = DEF_SENSITIVITY;
  if (s_settings.tachy_bpm < 90 || s_settings.tachy_bpm > 180) s_settings.tachy_bpm = DEF_TACHY_BPM;
  if (s_settings.brady_bpm < 30 || s_settings.brady_bpm > 60) s_settings.brady_bpm = DEF_BRADY_BPM;
  if (s_settings.sample_rate_s > 10) s_settings.sample_rate_s = DEF_SAMPLE_RATE_S;
}

void settings_save(void) {
  persist_write_data(PKEY_SETTINGS, &s_settings, sizeof(s_settings));
}

Settings *settings_get(void) { return &s_settings; }

void settings_toggle_alerts(void) { s_settings.alerts_on = !s_settings.alerts_on; }

void settings_toggle_notify(void) { s_settings.notify_on = !s_settings.notify_on; }

void settings_cycle_sensitivity(void) {
  s_settings.sensitivity = (s_settings.sensitivity + 1) % 3;
}

void settings_cycle_tachy(void) {
  s_settings.tachy_bpm += 5;
  if (s_settings.tachy_bpm > 160) s_settings.tachy_bpm = 100;
}

void settings_cycle_brady(void) {
  // step down; wrap 35..55
  if (s_settings.brady_bpm <= 35) s_settings.brady_bpm = 55;
  else s_settings.brady_bpm -= 5;
}

void settings_cycle_sample_rate(void) {
  switch (s_settings.sample_rate_s) {
    case 0: s_settings.sample_rate_s = 1; break;
    case 1: s_settings.sample_rate_s = 2; break;
    case 2: s_settings.sample_rate_s = 5; break;
    case 5: s_settings.sample_rate_s = 10; break;
    default: s_settings.sample_rate_s = 0; break;
  }
}

const char *settings_sensitivity_name(void) {
  static const char *names[3] = { "Low", "Medium", "High" };
  return names[s_settings.sensitivity];
}

const char *settings_sample_rate_name(void) {
  static char buf[12];
  if (s_settings.sample_rate_s == 0) return "Auto";
  snprintf(buf, sizeof(buf), "%us", s_settings.sample_rate_s);
  return buf;
}
