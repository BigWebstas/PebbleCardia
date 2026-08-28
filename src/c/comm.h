#pragma once
#include "platform.h"
#include "app.h"

void comm_init(void);
void comm_deinit(void);

// Periodic monitoring heartbeat.
void comm_send_status(const MonitorSnapshot *snap);

// An episode boundary: ongoing = true when it opens, false when it finalises.
void comm_send_episode(const Episode *ep, bool ongoing);
