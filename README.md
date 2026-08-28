# Cardia

A PebbleOS watchapp that watches your heart rate **more closely than the system
does by default** and flags irregularities: sustained high rate, sustained low
rate, and irregularly-irregular rhythm (the pattern associated with atrial
fibrillation).

> **Not a medical device.** The optical sensor is not an ECG and cannot diagnose
> anything. A flag means "look closer", not "you have a condition". If flags
> repeat, take the history log to a clinician.

## Target watches

`emery` (Pebble Time 2) and `gabbro` (Core Time 2) — the only platforms whose SDK
exposes real HRV peak-to-peak intervals (`health_service_peek_hrv_ppi_ms`).

## What it does

| Piece | How |
|---|---|
| **Closer sampling** | `health_service_set_heart_rate_sample_period()` + `health_service_set_hrv_sample_period()` pull the sample period down from the ~10-minute resting cadence to a fixed 1–10 s (default 2 s). |
| **Rate analysis** | Raw BPM (`HealthMetricHeartRateRawBPM`) is smoothed (EMA) and compared to user thresholds. Extremes only count when the accelerometer says you're still, so exertion isn't flagged. |
| **Rhythm analysis** | Beat-to-beat intervals (PPI ≈ RR) go into a 90 s ring buffer. Each tick computes mean RR, SDNN, **RMSSD**, **pNN50**, CV and a turning-point ratio, combined into a 0–100 irregularity score. Sensitivity picks the score threshold (75 / 60 / 45). |
| **Episodes** | A state machine confirms an abnormal state for 25 s before opening an episode (vibrate + on-watch banner + phone push), and needs 30 s of normal to close it. Peak/min HR and peak score are tracked across the episode. |
| **History** | Last 20 finalised episodes in persistent storage, with a detail card each. |
| **Phone** | `src/pkjs/index.js` keeps a rolling log of HR/HRV/step samples (one per ~15 s) and rhythm episodes in `localStorage`. Each sample carries `bpm, RMSSD, pNN50, SDNN, motion` and cumulative `steps` today. On launch it asks the watch to replay every stored episode (`DUMP`), so episodes the background worker recorded while the app was closed still reach the phone. |
| **Export** | The configuration page (Pebble app → Cardia → gear) builds a **Markdown report** with Mermaid charts (HR / RMSSD / pNN50 line charts, a steps bar chart, an episode Gantt + table) and renders it inline. The Core app's config web view can't save files, so: **Copy Markdown / Copy JSON** (paste into Joplin, Obsidian, …), and **Save / Share report** — opens the report in `tools/report-viewer.html` (which you host at any static https URL and paste into "Report viewer URL"); there `navigator.share` / a real download work. The report travels in the link fragment, never to a server. |
| **Sync** | Optionally set a **sync URL** and every sample + episode is `POST`ed there as JSON as it arrives. `tools/cardia-sheet-sync.gs` is a ready-made Google Apps Script that appends each one to a Sheet. |
| **Graph** | `graph.c` renders the live BPM trace with grid, threshold reference lines, a least-squares trend line and a current-value marker. |
| **Background monitor** | Optional. A PebbleOS **AppWorker** (`worker_src/c/worker.c`) runs the same engine continuously — it keeps monitoring after you leave the app and is relaunched on reboot. On a confirmed episode it persists the episode, streams it to the app, and calls `worker_launch_app()` so you get a buzzing full-screen alert even if the app was closed. Toggle in Settings. |

## Continuous background monitoring

This uses the standard PebbleOS background-worker subsystem (`app_worker_launch` /
`worker_manager`), the same mechanism the built-in pedometer/health worker uses —
not wakeups or timeline. One worker runs system-wide; enabling Cardia's makes it
the default worker, which PebbleOS relaunches at boot until you turn it off.

- **On:** the worker owns monitoring. The app, when open, just renders the packed
  snapshots the worker streams over `app_worker_send_message()` (6 bytes/tick).
  A "BG" marker shows on the monitor screen.
- **Off:** the app runs the engine itself, only while foregrounded; sampling is
  released on exit.
- The worker gets 12 KB RAM (~10 KB heap). The shared analysis code is compiled
  into it with a trimmed BPM history and `-DCARDIA_WORKER` (see `platform.h`).
- Faster sample rates + always-on = meaningfully more battery drain.

## Controls (monitor screen)

- **Up** – episode history
- **Down** – settings (background monitor, alerts, sensitivity, thresholds, sample rate, about/safety)
- **Select** – cycle sample rate (Auto → 1s → 2s → 5s → 10s)

## Build

```sh
pebble build
pebble install --emulator emery       # or gabbro, or --phone
```

## Layout

```
src/c/
  platform.h        <pebble.h> / <pebble_worker.h> switch (CARDIA_WORKER)
  config.h          tunables, persist keys, worker<->app message codes
  app.[ch]          RhythmStatus, Episode, MonitorSnapshot        [shared]
  analysis.[ch]     RR / BPM ring buffers, HRV metrics, score     [shared]
  engine.[ch]       sensor subs, motion gate, classify + episode FSM, callbacks  [shared]
  episodes.[ch]     persistent episode log                        [shared]
  wire.h            pack/unpack engine state into AppWorkerMessage [shared]
  settings.[ch]     user settings + persistence                   [shared]
  monitor.[ch]      app-side: run engine locally OR attach to the worker stream
  episode_format.c  episode -> UI strings (app only)
  comm.[ch]         AppMessage to the phone (app only)
  graph.[ch]        heart-rate graph rendering (app only)
  windows/          win_monitor, win_history, win_episode, win_settings, win_alert
  main.c
worker_src/c/worker.c   AppWorker entry: runs the shared engine, streams to app
src/pkjs/index.js       phone-side logging + export
```

The wscript compiles the `[shared]` sources into both the app and the worker;
the worker copies get `-DCARDIA_WORKER`.

## Hardware validation (Pebble Time 2, 2026-08-28)

Verified on a real watch over `pebble ... --phone <ip>`, foreground engine and
background worker:

- Real HRV peak-to-peak intervals stream from the sensor.
- Full episode path works: classify -> confirm -> vibrate -> red banner ->
  phone push (`pkjs [episode] {...}`) -> persist -> close -> "Normal rhythm".
  Confirmed from the **background worker** too (worker persists, streams to the
  app, calls `worker_launch_app()`).
- Settled resting values on clean signal: RMSSD ~70 ms, pNN50 ~30%, score ~30 -
  under threshold, no false positive.

**The optical PPI stream is very noisy.** Raw `health_service_peek_hrv_ppi_ms()`
routinely returns half-beats and double-beats (e.g. `279, 420, 279, 420` or
`1390, 1390` while the real rate is a steady 80). Mitigations added:

- `analysis_add_rr()` rejects any interval that disagrees with the filtered
  heart rate (outside ~0.65-1.45x of 60000/bpm).
- `analysis_signal_quality()` tracks the rolling acceptance rate; the engine
  refuses to call a rhythm irregular below 70%.
- 20 s acquisition warm-up before any rhythm call.
- Net effect: on this hardware Cardia mostly reports "Normal" and needs a clean
  reading held ~25 s to open a real episode. It won't false-positive, but the
  sensor may not be clean enough to reliably catch real AF either.

Other fixes found only on hardware: worker stack overflow (192-byte array made
`static`); app+worker both running the engine crashes the shared HRM session
(`monitor_start()` strictly picks one, app only ever attaches when background is
on); `app_worker_send_message(type, NULL)` faults; HRV was silently off whenever
the sample rate was "Auto" (now always holds a period for HRV).

`#define CARDIA_DEBUG 1` in `config.h` for on-watch logging, 6/8 s episode
timings, and a synthetic-arrhythmia injector via AppMessage key `DBG_TRIGGER`
(`--int 10013=1` inject, `=2` background on, `=3` off).

## Known limits

- HRV / irregular-rhythm detection needs a fixed sample rate (not "Auto").
- With the background worker **off**, monitoring only runs while the app is
  foregrounded and sampling is released on exit.
- The QEMU emulator injects BPM (`pebble emu-heart-rate <bpm>`) but not PPI, so
  the rhythm/irregular path can only be exercised on real hardware. The rate and
  worker-alert paths do work in the emulator.
