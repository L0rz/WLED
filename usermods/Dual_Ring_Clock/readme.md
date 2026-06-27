# Dual-Ring Clock (overlay usermod)

Renders an analog clock as an **overlay on top of the running WLED effect**.
The effect keeps animating in the background — only the LEDs used by the clock
hands and tick marks are overwritten (or additively blended). Drawing happens in
`handleOverlayDraw()`, i.e. after the effect has been rendered and just before
`strip.show()`.

It supports two independently configured rings:

| Ring        | Default | Purpose                                                                 |
|-------------|---------|-------------------------------------------------------------------------|
| Minute ring | 60 LEDs | Minute hand, optional second hand, minute / 5-minute tick marks          |
| Hour ring   | 24 LEDs | 12 hour positions of `count / 12` LEDs each; current hour lit as segment |

The hour ring is treated as 12 hour positions. With the default 24 LEDs that is
**2 LEDs per hour** (12 segments à 2 LEDs); the current hour lights up as a short
segment. Any `count` divisible by 12 works; otherwise it falls back to 1 LED per
hour.

## Installing / building

Add the folder name to `custom_usermods` of your build env in `platformio.ini`,
e.g.:

```ini
[env:nodemcuv2_l0rz]
extends = env:nodemcuv2
custom_usermods = audioreactive
  usermod_highlight
  usermod_onoffanimation
  Dual_Ring_Clock
```

Then build that env (`pio run -e nodemcuv2_l0rz`).

## Configuration (Config → Usermods)

All values persist to flash via `addToConfig()` / `readFromConfig()`.

**Global**
- `Overlay Enabled` — master on/off for the clock overlay.
- `Overlay Brightness (0-255)` — scales every overlay element.
- `Blend With Effect` — `false` overwrites the clock LEDs (default), `true`
  blends the clock additively over the effect.

**Minute ring**
- `Minute Ring First LED` — physical index of the ring's first LED.
- `Minute Ring LED Count` — number of LEDs (default 60).
- `Minute Ring 12oClock LED` — offset within the ring that represents 12 o'clock.
- `Minute Ring Clockwise` — direction time advances.

**Hour ring** — same four parameters (`First LED` default 60, `LED Count`
default 24).

**Marks / hands**
- `Show Seconds` — draw the second hand on the minute ring.
- `Show Minute Marks` — a dim mark on every minute LED of the minute ring.
- `Show Hour Marks (5min)` — a brighter mark every 5 minutes (12 marks) on the
  minute ring.
- `Show Hour Ring Marks` — a dim mark at each of the 12 hour positions on the
  hour ring.

**Colors** (each `RRGGBB` hex)
- `Hour Hand Color`, `Minute Hand Color`, `Second Hand Color`,
  `Minute Mark Color`, `Hour Mark Color`.

## Info panel

The Info tab shows a `Dual-Ring Clock` row with the current `HH:MM:SS`
(or `disabled` when the overlay is off).

## Notes

- Both rings are validated against the actual strip length on save; an out-of-range
  start resets to 0 and the count is clamped so the ring fits the strip.
- The usermod re-triggers `strip.show()` ~50×/s while enabled so the second hand
  ticks smoothly even when the background effect is static.
