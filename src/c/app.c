#include "app.h"

const char *rhythm_name(RhythmStatus s) {
  switch (s) {
    case RHYTHM_NORMAL:    return "Normal";
    case RHYTHM_ELEVATED:  return "Elevated";
    case RHYTHM_LOW:       return "Low";
    case RHYTHM_IRREGULAR: return "Irregular";
    default:              return "No signal";
  }
}

GColor rhythm_color(RhythmStatus s) {
  switch (s) {
    case RHYTHM_NORMAL:    return GColorIslamicGreen;
    case RHYTHM_ELEVATED:  return GColorFolly;
    case RHYTHM_LOW:       return GColorVividCerulean;
    case RHYTHM_IRREGULAR: return GColorChromeYellow;
    default:              return GColorLightGray;
  }
}
