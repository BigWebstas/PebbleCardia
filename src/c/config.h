#pragma once
#include "platform.h"

// ---------------------------------------------------------------------------
// Cardia - close heart-rate monitoring with irregularity detection
// ---------------------------------------------------------------------------
// This is NOT a medical device. The optical sensor is not an ECG and cannot
// diagnose atrial fibrillation or any other condition. Treat every flag as a
// prompt to pay attention and, if it repeats, to see a clinician.
// ---------------------------------------------------------------------------

#define APP_VERSION "1.1.0"

// Set to 1 for on-watch debug logging, shortened episode timings, and the
// synthetic-arrhythmia injector (long-press SELECT, or AppMessage key
// DBG_TRIGGER: 1 = inject irregular, 2 = background on, 3 = background off).
// 0 for release.
#define CARDIA_DEBUG 0
#if CARDIA_DEBUG == 0
#undef CARDIA_DEBUG
#endif

// --- Ring buffers -------------------------------------------------------------
// The worker runs in 12 KB of RAM, so it keeps a smaller beat-interval window.
// The BPM graph ring is just a byte per sample now, so both keep the full 180
// - the worker persists it (PKEY_BPM_HISTORY) so the app's graph survives
// leaving/returning while the worker runs.
#define BPM_BUF_LEN          180
#if defined(CARDIA_WORKER)
#define RR_BUF_LEN           72
#else
#define RR_BUF_LEN           96
#endif

// --- Beat-interval sanity window (ms) -------------------------------------
#define RR_MIN_MS            300    // 200 bpm - faster is almost certainly noise
#define RR_MAX_MS           2000    // 30 bpm  - slower is almost certainly a miss

// --- Analysis window ------------------------------------------------------
#define RR_WINDOW_S           90    // only intervals newer than this are analysed
#define RR_MIN_INTERVALS      20    // fewer than this: not enough to judge rhythm
#define BPM_STALE_S           25    // no sample newer than this -> "no signal"

// --- Episode state machine ----------------------------------------------
#ifdef CARDIA_DEBUG
#define CONFIRM_SECS           6    // shortened so the episode path is quick to test
#define CLEAR_SECS             8
#else
#define CONFIRM_SECS          25    // an abnormal state must persist this long
#define CLEAR_SECS            30    // ...and normal must persist this long to end
#endif
#define TICK_MS             3000    // engine re-classifies this often

// --- Defaults (user-adjustable in Settings) --------------------------------
#define DEF_ALERTS_ON       true
#define DEF_SENSITIVITY     1       // 0 = low, 1 = medium, 2 = high
#define DEF_TACHY_BPM       120     // sustained resting rate at/above -> Elevated
#define DEF_BRADY_BPM       45      // sustained rate at/below       -> Low
#define DEF_SAMPLE_RATE_S   2       // 0 = auto (battery), else HR+HRV period (s)
#define DEF_BACKGROUND_ON   false   // continuous background worker

// Irregularity score threshold per sensitivity (higher score = more irregular)
static const uint8_t SENS_SCORE_THRESHOLD[3] = { 75, 60, 45 };

// --- Persist keys --------------------------------------------------------
#define PKEY_SETTINGS         1
#define PKEY_EPISODE_COUNT    2
#define PKEY_EPISODE_BLOB     3
#define PKEY_PENDING_ALERT    4   // worker -> app: an episode opened while app was closed
#define PKEY_BPM_HISTORY      5   // rolling BPM graph ring, so the graph survives restarts

// --- AppMessage MSG_KIND (watch -> phone) -------------------------------
#define MSG_KIND_STATUS      0
#define MSG_KIND_EPISODE     1

// --- Worker <-> app messages (app_worker_send_message type codes) ----------
#define WMSG_SNAPSHOT        0   // worker -> app: packed MonitorSnapshot
#define WMSG_EPISODE_OPEN    1   // worker -> app: an episode was confirmed
#define WMSG_EPISODE_CLOSE   2   // worker -> app: an episode finalised (already persisted)
#define WMSG_SETTINGS_DIRTY 10   // app -> worker: re-read settings from persist
#define WMSG_DEBUG_INJECT   11   // app -> worker: inject a synthetic irregular burst (debug)
