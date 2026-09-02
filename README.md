# Cardia

A PebbleOS watchapp that watches your heart rate more closely than the system
does by default and flags three things: sustained high rate, sustained low rate,
and irregularly-irregular rhythm (the pattern linked to atrial fibrillation).

> **Not a medical device.** The optical sensor is not an ECG and cannot diagnose
> anything. A flag means "look closer", not "you have a condition". If flags
> repeat, take the history log to a clinician.

Runs on **Pebble Time 2 (`emery`)** and **Core Time 2 (`gabbro`)** — the only
watches whose SDK exposes beat-to-beat intervals.

## What it does

- **Samples every 1–10 s** (default 2 s) instead of the ~10-minute resting cadence.
- **Rate:** smooths raw BPM and compares to your thresholds, but only while the
  accelerometer says you're still, so exercise isn't flagged.
- **Rhythm:** feeds beat-to-beat intervals into a 90 s window, computes HRV
  metrics (RMSSD, pNN50, SDNN and more) into a 0–100 irregularity score, with a
  sensitivity setting.
- **Episodes:** an abnormal state must hold for 25 s to open an episode (vibrate
  + on-watch banner + phone notification) and 30 s of normal to close it. The
  last 20 episodes are kept with a detail card each.
- **Background monitor (optional):** a PebbleOS worker runs the same engine
  continuously, so monitoring keeps going after you leave the app and survives a
  reboot. Costs more battery.
- **Phone log:** the companion JS keeps a rolling log of samples and episodes and
  can `POST` each one to a sync URL you set (`tools/cardia-sheet-sync.gs` appends
  them to a Google Sheet).
- **Export:** the config page builds a Markdown report with charts. The watch's
  config view can't save files, so it's copy-to-clipboard — paste into Joplin,
  Obsidian, etc. `tools/report-viewer.html`, hosted, gives working Save/Share.
  The config page follows the phone's light/dark system theme.

## Controls

| Button | Action |
|---|---|
| Up | Episode history |
| Down | Settings |
| Select | Cycle sample rate (Auto → 1s → 2s → 5s → 10s) |

## Build

```sh
pebble build
pebble install --emulator emery      # or gabbro, or --phone <ip>
```

The emulator injects BPM (`pebble emu-heart-rate <bpm>`) but not beat intervals,
so the rhythm path can only be tested on a real watch. Set `CARDIA_DEBUG 1` in
`src/c/config.h` for on-watch logging and a synthetic-arrhythmia injector.

## Limits

- HRV/rhythm detection needs a fixed sample rate, not "Auto".
- With the background monitor off, monitoring only runs while the app is open.
- The optical signal is noisy: Cardia filters hard and needs a clean reading held
  ~25 s to open an episode. It won't false-positive, but it may also miss real
  events on a poor signal.

`CLAUDE.md` covers the code layout and the bugs found on hardware.
