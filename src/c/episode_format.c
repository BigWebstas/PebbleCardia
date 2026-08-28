#include "episodes.h"
#include "config.h"

// UI-facing formatting for episodes. App only - the worker never calls these,
// which keeps rhythm_name() (defined in app.c) out of the worker link.

void episode_format_duration(uint32_t seconds, char *buf, size_t len) {
  uint32_t m = seconds / 60, s = seconds % 60;
  if (m >= 60) {
    snprintf(buf, len, "%u:%02u:%02u", (unsigned)(m / 60), (unsigned)(m % 60), (unsigned)s);
  } else {
    snprintf(buf, len, "%u:%02u", (unsigned)m, (unsigned)s);
  }
}

void episode_format_title(const Episode *ep, char *buf, size_t len) {
  char dur[12];
  episode_format_duration(ep->duration_s, dur, sizeof(dur));
  snprintf(buf, len, "%s  %s", rhythm_name((RhythmStatus)ep->type), dur);
}

void episode_format_when(const Episode *ep, char *buf, size_t len) {
  time_t t = ep->start;
  struct tm *lt = localtime(&t);
  char stamp[20];
  strftime(stamp, sizeof(stamp), "%b %e, %H:%M", lt);
  snprintf(buf, len, "%s  HR %u-%u", stamp, ep->hr_min, ep->hr_peak);
}
