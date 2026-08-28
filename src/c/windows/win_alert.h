#pragma once
#include "../app.h"

// Full-screen alert shown when the background worker flags an episode while the
// app was closed (or in the background).
void win_alert_push(const Episode *ep);
