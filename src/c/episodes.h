#pragma once
#include "platform.h"
#include "app.h"

void episodes_load(void);

// Append a finalised episode (newest kept, oldest dropped) and persist.
void episodes_add(const Episode *ep);

int  episodes_count(void);

// index 0 = most recent.
const Episode *episodes_get(int index);

void episodes_clear(void);

// Human-readable helpers for the UI (defined in episode_format.c, app only).
void episode_format_title(const Episode *ep, char *buf, size_t len);
void episode_format_when(const Episode *ep, char *buf, size_t len);
void episode_format_duration(uint32_t seconds, char *buf, size_t len);
