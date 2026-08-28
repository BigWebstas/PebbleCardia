#include "analysis.h"
#include "config.h"

// --- beat-to-beat interval ring -------------------------------------------
typedef struct { uint32_t t; uint16_t ms; } RRSample;
static RRSample s_rr[RR_BUF_LEN];
static int s_rr_head;   // next write slot
static int s_rr_count;

// --- bpm ring (for the graph) --------------------------------------------
typedef struct { uint32_t t; uint8_t bpm; } BpmSample;
static BpmSample s_bpm[BPM_BUF_LEN];
static int s_bpm_head;
static int s_bpm_count;

static uint8_t s_last_bpm;
static time_t  s_last_bpm_t;
static int32_t s_bpm_ema_x100;   // smoothed bpm * 100
static time_t  s_rr_stream_start; // wall time of the first RR sample since reset

// Rolling acceptance rate of incoming beat intervals (x1000). The optical PPI
// stream is full of half/double-beat artifacts; a low acceptance rate means
// the sensor isn't tracking well and the rhythm can't be judged.
static int s_rr_accept_rate_x1000 = 1000;

// integer square root (Newton) -------------------------------------------
static uint32_t isqrt32(uint32_t n) {
  if (n == 0) return 0;
  uint32_t x = n, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + n / x) / 2; }
  return x;
}

void analysis_reset(void) {
  s_rr_head = s_rr_count = 0;
  s_bpm_head = s_bpm_count = 0;
  s_last_bpm = 0;
  s_last_bpm_t = 0;
  s_bpm_ema_x100 = 0;
  s_rr_stream_start = 0;
  s_rr_accept_rate_x1000 = 1000;
}

void analysis_add_rr(uint16_t ppi_ms, time_t now) {
  bool accept = (ppi_ms >= RR_MIN_MS && ppi_ms <= RR_MAX_MS);

  // Cross-check against the filtered heart rate: a genuine beat interval is
  // close to 60000/bpm. Half-beats, double-beats and dropouts are not.
  if (accept && s_last_bpm >= 30 && s_last_bpm <= 220 &&
      (now - s_last_bpm_t) <= 30) {
    int expected = 60000 / s_last_bpm;
    if (ppi_ms < (expected * 65) / 100 || ppi_ms > (expected * 145) / 100) {
      accept = false;
    }
  }

  // EMA of the acceptance rate (alpha ~0.15).
  s_rr_accept_rate_x1000 += ((accept ? 1000 : 0) - s_rr_accept_rate_x1000) * 15 / 100;

  if (!accept) return;
  if (s_rr_stream_start == 0) s_rr_stream_start = now;
  s_rr[s_rr_head] = (RRSample){ .ms = ppi_ms, .t = now };
  s_rr_head = (s_rr_head + 1) % RR_BUF_LEN;
  if (s_rr_count < RR_BUF_LEN) s_rr_count++;
}

// 0..100: recent fraction of incoming beat intervals that passed artifact
// rejection. Low = the sensor isn't tracking; don't trust the rhythm call.
uint8_t analysis_signal_quality(void) {
  return (uint8_t)((s_rr_accept_rate_x1000 + 5) / 10);
}

void analysis_add_bpm(uint8_t bpm, time_t now) {
  if (bpm == 0) return;
  s_bpm[s_bpm_head] = (BpmSample){ .bpm = bpm, .t = now };
  s_bpm_head = (s_bpm_head + 1) % BPM_BUF_LEN;
  if (s_bpm_count < BPM_BUF_LEN) s_bpm_count++;
  s_last_bpm = bpm;
  s_last_bpm_t = now;
  // EMA with alpha ~ 0.35
  if (s_bpm_ema_x100 == 0) s_bpm_ema_x100 = bpm * 100;
  else s_bpm_ema_x100 += (((int32_t)bpm * 100) - s_bpm_ema_x100) * 35 / 100;
}

uint8_t analysis_last_bpm(void) { return s_last_bpm; }

int analysis_last_bpm_age(time_t now) {
  if (s_last_bpm_t == 0) return INT32_MAX;
  return (int)(now - s_last_bpm_t);
}

uint8_t analysis_smoothed_bpm(void) {
  return (uint8_t)((s_bpm_ema_x100 + 50) / 100);
}

int analysis_rr_stream_age(time_t now) {
  if (s_rr_stream_start == 0) return 0;
  return (int)(now - s_rr_stream_start);
}

// Walk the RR ring newest-to-oldest, collecting samples inside the window.
static int collect_recent_rr(time_t now, uint16_t *dst, int max) {
  int n = 0;
  for (int i = 0; i < s_rr_count && n < max; i++) {
    int idx = (s_rr_head - 1 - i + RR_BUF_LEN * 2) % RR_BUF_LEN;
    if (now - s_rr[idx].t > RR_WINDOW_S) break;
    dst[n++] = s_rr[idx].ms;
  }
  return n;   // dst is newest-first
}

bool analysis_compute(time_t now, AnalysisResult *out) {
  // static, not stack: the background worker runs on a ~2 KB stack.
  static uint16_t rr[RR_BUF_LEN];
  int n = collect_recent_rr(now, rr, RR_BUF_LEN);

  AnalysisResult r = (AnalysisResult){ .n_intervals = (uint8_t)(n > 255 ? 255 : n) };
  if (n < RR_MIN_INTERVALS) { if (out) *out = r; return false; }

  // mean
  uint32_t sum = 0;
  for (int i = 0; i < n; i++) sum += rr[i];
  uint32_t mean = sum / n;

  // variance of intervals + successive differences
  uint64_t var_acc = 0;      // sum of (rr - mean)^2
  uint64_t ssd_acc = 0;      // sum of (rr[i] - rr[i-1])^2
  uint32_t nn50 = 0;
  for (int i = 0; i < n; i++) {
    int32_t d = (int32_t)rr[i] - (int32_t)mean;
    var_acc += (uint64_t)(d * d);
    if (i + 1 < n) {
      int32_t sd = (int32_t)rr[i] - (int32_t)rr[i + 1];
      ssd_acc += (uint64_t)(sd * sd);
      if (sd > 50 || sd < -50) nn50++;
    }
  }
  uint32_t sdnn  = isqrt32((uint32_t)(var_acc / n));
  uint32_t rmssd = isqrt32((uint32_t)(ssd_acc / (n - 1)));
  uint32_t pnn50 = (nn50 * 100) / (n - 1);
  uint32_t cv    = mean ? (sdnn * 100) / mean : 0;

  r.mean_rr_ms = (uint16_t)mean;
  r.sdnn_ms    = (uint16_t)sdnn;
  r.rmssd_ms   = (uint16_t)rmssd;
  r.pnn50_pct  = (uint8_t)(pnn50 > 100 ? 100 : pnn50);
  r.cv_pct     = (uint8_t)(cv > 100 ? 100 : cv);
  r.hr_from_rr = (uint8_t)(mean ? (60000 / mean) : 0);

  // --- irregularity score ------------------------------------------------
  // Sinus rhythm at rest: RMSSD ~20-60 ms, pNN50 well under 20%, CV under ~8%.
  // An irregularly-irregular rhythm pushes all three up together, and the
  // successive differences change sign nearly every beat (low autocorrelation).
  int32_t score = r.pnn50_pct;                       // 0..100
  if (r.rmssd_ms > 80)  score += (r.rmssd_ms - 80) / 3;
  if (r.cv_pct   > 10)  score += (r.cv_pct - 10) * 2;

  // turning-point ratio: fraction of beats where dRR flips sign.
  int flips = 0, cmp = 0;
  for (int i = 1; i + 1 < n; i++) {
    int32_t a = (int32_t)rr[i - 1] - (int32_t)rr[i];
    int32_t b = (int32_t)rr[i] - (int32_t)rr[i + 1];
    if ((a > 0 && b < 0) || (a < 0 && b > 0)) flips++;
    cmp++;
  }
  int tpr = cmp ? (flips * 100) / cmp : 0;           // ~66% expected for noise
  if (tpr < 40) score -= 20;                         // metronomic -> not AF-like

  if (score < 0) score = 0;
  if (score > 100) score = 100;
  r.score = (uint8_t)score;

  if (out) *out = r;
  return true;
}

int analysis_bpm_series(uint8_t *dst, int max) {
  int n = s_bpm_count < max ? s_bpm_count : max;
  int start = (s_bpm_head - n + BPM_BUF_LEN * 2) % BPM_BUF_LEN;
  for (int i = 0; i < n; i++) {
    dst[i] = s_bpm[(start + i) % BPM_BUF_LEN].bpm;
  }
  return n;
}
