// Build shim. The appstore build service regenerates the wscript and only
// globs worker_src/ for the worker binary, so the shared engine sources are
// pulled in here rather than added to the worker build in wscript. The app
// binary still compiles the original directly from src/c/.
#ifndef CARDIA_WORKER
#define CARDIA_WORKER 1
#endif
#include "../../src/c/analysis.c"
