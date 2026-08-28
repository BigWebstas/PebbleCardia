#include "episodes.h"
#include "config.h"

static Episode s_eps[MAX_EPISODES];   // index 0 = most recent
static int s_count;

void episodes_load(void) {
  s_count = 0;
  if (persist_exists(PKEY_EPISODE_COUNT)) {
    s_count = persist_read_int(PKEY_EPISODE_COUNT);
  }
  if (s_count < 0) s_count = 0;
  if (s_count > MAX_EPISODES) s_count = MAX_EPISODES;
  if (s_count > 0 && persist_exists(PKEY_EPISODE_BLOB)) {
    int want = s_count * sizeof(Episode);
    int got = persist_read_data(PKEY_EPISODE_BLOB, s_eps, sizeof(s_eps));
    if (got < want) s_count = got / sizeof(Episode);
  } else {
    s_count = 0;
  }
}

static void episodes_persist(void) {
  persist_write_int(PKEY_EPISODE_COUNT, s_count);
  if (s_count > 0) {
    persist_write_data(PKEY_EPISODE_BLOB, s_eps, s_count * sizeof(Episode));
  }
}

void episodes_add(const Episode *ep) {
  int keep = s_count < MAX_EPISODES ? s_count : MAX_EPISODES - 1;
  memmove(&s_eps[1], &s_eps[0], keep * sizeof(Episode));
  s_eps[0] = *ep;
  s_count = keep + 1;
  episodes_persist();
}

int episodes_count(void) { return s_count; }

const Episode *episodes_get(int index) {
  if (index < 0 || index >= s_count) return NULL;
  return &s_eps[index];
}

void episodes_clear(void) {
  s_count = 0;
  persist_delete(PKEY_EPISODE_BLOB);
  persist_write_int(PKEY_EPISODE_COUNT, 0);
}
