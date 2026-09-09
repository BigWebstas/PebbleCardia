# Store assets

| File | Size | Notes |
|---|---|---|
| `cardia-banner-720x320.png` | 720x320 | Store banner. Source: `banner.py` → `banner.svg` → `rsvg-convert`. |
| `emery-200x228-monitor.png` | 200x228 | Live monitor, 74 BPM, Normal. |
| `emery-200x228-history.png` | 200x228 | Episode history list. |
| `emery-200x228-settings.png` | 200x228 | Settings menu. |
| `emery-200x228-episode.png` | 200x228 | One episode detail card. |

Captured from `pebble install --emulator emery` with `pebble emu-heart-rate 74`.

## gabbro (Core Time 2) — no shots yet

The gabbro build target is a round 260x260 screen. Two blockers:

- `pebble emu-heart-rate` does not inject into the gabbro emulator, so the
  monitor screen stays on `no signal`.
- UI text is drawn flush to the left edge and the round bezel clips it.

Regenerate the banner:

```sh
cd screenshots && python3 banner.py && rsvg-convert -w 720 -h 320 banner.svg -o cardia-banner-720x320.png
```
