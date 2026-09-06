#include <pebble.h>
#include "glance.h"
#include "config.h"
#include "app.h"

// Held between glance_update() and the reload callback, which PebbleOS runs
// synchronously. AppGlance copies the string, so a static buffer is enough.
static char s_subtitle[80];

// The launcher re-renders {time_since(...)} on its own, so the subtitle ages
// ("14 sec ago" -> "6 min ago") without the app running. %% survives snprintf
// as a literal % for the template parser. Conditional format() predicates are
// not supported on this firmware, so %aT it is.
#define AGE_TEMPLATE "{time_since(%ld)|format('%%aT ago')}"

static void reload_cb(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1 || s_subtitle[0] == '\0') return;
  const AppGlanceSlice slice = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = s_subtitle,
    },
    .expiration_time = time(NULL) + GLANCE_EXPIRY_S,
  };
  app_glance_add_slice(session, slice);
}

void glance_update(const MonitorSnapshot *snap) {
  if (!snap || snap->bpm == 0) {
    s_subtitle[0] = '\0';
    app_glance_reload(NULL, NULL);   // clear a stale slice
    return;
  }

  const long ts = (long)time(NULL);
  bool abnormal = snap->status == RHYTHM_ELEVATED ||
                  snap->status == RHYTHM_LOW ||
                  snap->status == RHYTHM_IRREGULAR;
  if (abnormal) {
    snprintf(s_subtitle, sizeof(s_subtitle), "%u BPM %s · " AGE_TEMPLATE,
             snap->bpm, rhythm_name(snap->status), ts);
  } else {
    snprintf(s_subtitle, sizeof(s_subtitle), "%u BPM · " AGE_TEMPLATE,
             snap->bpm, ts);
  }
  app_glance_reload(reload_cb, NULL);
}
