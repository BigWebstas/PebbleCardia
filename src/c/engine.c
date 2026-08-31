#include "engine.h"
#include "config.h"
#include "settings.h"
#include "analysis.h"
#include "episodes.h"

#ifdef CARDIA_WORKER
#define IS_WORKER_STR "wkr"
#else
#define IS_WORKER_STR "app"
#endif

static EngineCallbacks s_cb;
static bool s_running;
static AppTimer *s_tick;
static MonitorSnapshot s_snap;

// motion: exponential average of per-batch accel magnitude deviation (milli-g)
static int s_motion_mg;
#define MOTION_HIGH_MG 140

// episode state machine
static RhythmStatus s_candidate;       // abnormal state being confirmed
static time_t       s_candidate_since;
static bool         s_ep_active;
static Episode      s_ep;               // in-progress episode
static time_t       s_ep_normal_since;  // when "normal" first re-appeared

// ---------------------------------------------------------------------------
static void apply_sample_rate(void) {
  uint16_t s = settings_get()->sample_rate_s;   // 0 = auto
  // HRV is only delivered while an explicit period is held; never leave it on
  // "auto" or the irregular-rhythm analysis gets no beat intervals at all.
  uint16_t hrv_s = s ? s : DEF_SAMPLE_RATE_S;
  bool ok_hr = health_service_set_heart_rate_sample_period(s);
  bool ok_hrv = health_service_set_hrv_sample_period(hrv_s);
  (void)ok_hr; (void)ok_hrv;
#if defined(CARDIA_DEBUG)
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s sample_rate s=%u hrv=%u  hr_ok=%d hrv_ok=%d",
          IS_WORKER_STR, s, hrv_s, ok_hr, ok_hrv);
#endif
}

void engine_apply_sample_rate(void) {
  if (s_running) apply_sample_rate();
}

// ---------------------------------------------------------------------------
static void health_handler(HealthEventType event, void *context) {
  time_t now = time(NULL);
  switch (event) {
    case HealthEventHeartRateUpdate: {
      HealthValue raw = health_service_peek_current_value(HealthMetricHeartRateRawBPM);
      HealthValue filt = health_service_peek_current_value(HealthMetricHeartRateBPM);
      uint8_t bpm = raw > 0 ? (uint8_t)raw : (uint8_t)filt;
      analysis_add_bpm(bpm, now);
      break;
    }
    case HealthEventHRVUpdate: {
      uint16_t ppi = health_service_peek_hrv_ppi_ms();
#if defined(CARDIA_DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "%s HRV ppi=%u",
              IS_WORKER_STR, ppi);
#endif
      if (ppi > 0) analysis_add_rr(ppi, now);
      break;
    }
    case HealthEventSignificantUpdate:
      s_snap.hr_available = health_service_metric_accessible(
          HealthMetricHeartRateBPM, now - 1, now) & HealthServiceAccessibilityMaskAvailable;
      break;
    default:
      break;
  }
}

static void accel_handler(AccelData *data, uint32_t num_samples) {
  if (num_samples == 0) return;
  int32_t acc = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    if (data[i].did_vibrate) continue;
    int32_t x = data[i].x, y = data[i].y, z = data[i].z;
    int32_t mag2 = x * x + y * y + z * z;
    int32_t m = 0, b = 1 << 15;
    while (b > 0) { int32_t t = m + b; if (t * t <= mag2) m = t; b >>= 1; }
    acc += (m > 1000) ? (m - 1000) : (1000 - m);
  }
  int32_t batch = acc / (int32_t)num_samples;
  s_motion_mg += (batch - s_motion_mg) * 30 / 100;   // EMA alpha 0.3
}

// ---------------------------------------------------------------------------
static RhythmStatus classify(time_t now, AnalysisResult *ar_out) {
  Settings *cfg = settings_get();

  if (analysis_last_bpm_age(now) > BPM_STALE_S) return RHYTHM_NO_SIGNAL;

  AnalysisResult ar;
  bool have_hrv = analysis_compute(now, &ar);
  if (ar_out) *ar_out = ar;

  uint8_t bpm = analysis_smoothed_bpm();
  bool moving = s_motion_mg > MOTION_HIGH_MG;

  if (!moving && bpm >= cfg->tachy_bpm) return RHYTHM_ELEVATED;
  if (!moving && bpm > 0 && bpm <= cfg->brady_bpm) return RHYTHM_LOW;

  // Only trust the rhythm call when: the sensor has settled (>=20 s of beats),
  // most incoming intervals survive artifact rejection (the optical PPI stream
  // is otherwise full of half/double-beat errors), and we're not moving.
  bool warmed_up = analysis_rr_stream_age(now) >= 20;
  bool clean = analysis_signal_quality() >= 70;

  if (!moving && warmed_up && clean && have_hrv &&
      ar.n_intervals >= RR_MIN_INTERVALS &&
      ar.score >= SENS_SCORE_THRESHOLD[cfg->sensitivity]) {
    return RHYTHM_IRREGULAR;
  }
  return RHYTHM_NORMAL;
}

static bool is_abnormal(RhythmStatus s) {
  return s == RHYTHM_ELEVATED || s == RHYTHM_LOW || s == RHYTHM_IRREGULAR;
}

// BPM graph history, persisted so the graph survives the app closing (in
// background mode the worker owns the real history).
static void bpm_history_load(void) {
  static uint8_t buf[BPM_BUF_LEN];
  if (!persist_exists(PKEY_BPM_HISTORY)) return;
  int n = persist_read_data(PKEY_BPM_HISTORY, buf, sizeof(buf));
  if (n > 0) analysis_bpm_restore(buf, n);
}

void engine_persist_bpm_history(void) {
  static uint8_t buf[BPM_BUF_LEN];
  int n = analysis_bpm_series(buf, BPM_BUF_LEN);
  if (n > 0) persist_write_data(PKEY_BPM_HISTORY, buf, n);
}

// ---------------------------------------------------------------------------
static void tick(void *context) {
  s_tick = NULL;
  time_t now = time(NULL);

  AnalysisResult ar = {0};
  RhythmStatus st = classify(now, &ar);
  uint8_t bpm = analysis_smoothed_bpm();

  // ---- episode state machine ------------------------------------------
  if (is_abnormal(st)) {
    s_ep_normal_since = 0;
    if (!s_ep_active) {
      if (s_candidate != st) { s_candidate = st; s_candidate_since = now; }
      else if (now - s_candidate_since >= CONFIRM_SECS) {
        s_ep_active = true;
        s_ep = (Episode){ .start = s_candidate_since, .type = (uint8_t)st,
                          .hr_peak = bpm, .hr_min = bpm ? bpm : 255, .score = ar.score };
        if (s_cb.on_episode_open) s_cb.on_episode_open(&s_ep);
      }
    } else {
      if (st != s_ep.type) s_ep.type = (uint8_t)st;
      if (bpm > s_ep.hr_peak) s_ep.hr_peak = bpm;
      if (bpm && bpm < s_ep.hr_min) s_ep.hr_min = bpm;
      if (ar.score > s_ep.score) s_ep.score = ar.score;
    }
  } else {
    s_candidate = RHYTHM_NORMAL;
    if (s_ep_active) {
      if (s_ep_normal_since == 0) s_ep_normal_since = now;
      if (now - s_ep_normal_since >= CLEAR_SECS) {
        s_ep_active = false;
        int32_t dur = s_ep_normal_since - s_ep.start;
        s_ep.duration_s = (uint16_t)(dur > 0xFFFF ? 0xFFFF : (dur < 0 ? 0 : dur));
        if (s_ep.hr_min == 255) s_ep.hr_min = 0;
        episodes_add(&s_ep);
        if (s_cb.on_episode_close) s_cb.on_episode_close(&s_ep);
      }
    }
  }

  // ---- publish snapshot ---------------------------------------------
  s_snap.status = st;
  s_snap.bpm = bpm;
  s_snap.bpm_raw = analysis_last_bpm();
  s_snap.rmssd_ms = ar.rmssd_ms;
  s_snap.pnn50_pct = ar.pnn50_pct;
  s_snap.sdnn_ms = ar.sdnn_ms;
  s_snap.score = ar.score;
  s_snap.n_intervals = ar.n_intervals;
  s_snap.motion = (uint8_t)(s_motion_mg > 400 ? 100 : s_motion_mg / 4);
  s_snap.episode_active = s_ep_active;
  s_snap.episode_start = s_ep.start;
  s_snap.episode_type = s_ep.type;

#if defined(CARDIA_DEBUG)
  APP_LOG(APP_LOG_LEVEL_DEBUG,
          "%s st=%d bpm=%u n=%u rmssd=%u pnn50=%u sdnn=%u score=%u sq=%u mot=%d",
          IS_WORKER_STR, (int)st, bpm, ar.n_intervals, ar.rmssd_ms, ar.pnn50_pct,
          ar.sdnn_ms, ar.score, analysis_signal_quality(), s_motion_mg);
#endif

  if (s_cb.on_tick) s_cb.on_tick(&s_snap);

  // Persist the graph history every ~30 s so a returning app picks it up.
  static int persist_div = 0;
  if (++persist_div >= 10) { persist_div = 0; engine_persist_bpm_history(); }

  s_tick = app_timer_register(TICK_MS, tick, NULL);
}

#ifdef CARDIA_DEBUG
// Feed synthetic irregularly-irregular beat intervals so the full IRREGULAR ->
// episode -> alert -> history -> phone path can be tested on hardware without a
// real arrhythmia. Local (in-app) engine only.
void engine_debug_inject_irregular(void) {
  time_t now = time(NULL);
  // bpm first so the artifact cross-check accepts the intervals, then an
  // irregular stream inside the plausible window (~90 bpm, +/-180 ms) so signal
  // quality stays high.
  analysis_add_bpm(90, now);
  uint32_t seed = (uint32_t)now;
  for (int i = 0; i < 45; i++) {
    seed = seed * 1103515245u + 12345u;
    int jitter = (int)((seed >> 18) % 360) - 180;
    analysis_add_rr((uint16_t)(667 + jitter), now - (44 - i));
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "debug: injected irregular RR burst");
}
#endif

// ---------------------------------------------------------------------------
void engine_init(const EngineCallbacks *cb) {
  s_cb = *cb;
  s_snap = (MonitorSnapshot){ .hr_available = true };
  time_t now = time(NULL);
  s_snap.hr_available = health_service_metric_accessible(
      HealthMetricHeartRateBPM, now - 1, now) & HealthServiceAccessibilityMaskAvailable;
}

void engine_start(void) {
  if (s_running) return;
  s_running = true;
  analysis_reset();
  bpm_history_load();           // keep the graph populated across restarts
  s_candidate = RHYTHM_NORMAL;
  s_ep_active = false;
  s_motion_mg = 0;

  health_service_events_subscribe(health_handler, NULL);
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  apply_sample_rate();

  s_tick = app_timer_register(TICK_MS, tick, NULL);
}

void engine_stop(void) {
  if (!s_running) return;
  s_running = false;
  engine_persist_bpm_history();
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
  health_service_set_heart_rate_sample_period(0);
  health_service_set_hrv_sample_period(0);
  health_service_events_unsubscribe();
  accel_data_service_unsubscribe();
}

bool engine_running(void) { return s_running; }

void engine_get_snapshot(MonitorSnapshot *out) { *out = s_snap; }
