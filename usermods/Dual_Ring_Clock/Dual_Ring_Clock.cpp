#include "wled.h"

/*
 * Dual-Ring Clock overlay usermod (Usermod v2)
 *
 * Renders an analog clock as an overlay ON TOP of whatever WLED effect is
 * currently running. The effect keeps rendering in the background; only the
 * LEDs used by the clock hands and the tick marks are overwritten (or blended).
 *
 * Two independently configurable rings:
 *   - Minute ring  : default 60 LEDs (one LED per minute), carries the
 *                    minute hand, the (optional) second hand and the minute /
 *                    5-minute tick marks.
 *   - Hour ring    : default 24 LEDs, treated as 12 hour positions of
 *                    (count / 12) LEDs each. The current hour lights up as a
 *                    short segment; the 12 hour positions can be marked dimly.
 *
 * Everything (LED counts, ring start offsets, 12-o'clock position, direction,
 * all colours and the overlay brightness) is configurable from the WLED UI
 * under Config -> Usermods and is persisted to flash via addToConfig() /
 * readFromConfig(). The current time and state are also shown in the Info tab
 * via addToJsonInfo().
 */

class DualRingClockUsermod : public Usermod {
private:
  static constexpr uint32_t refreshRate  = 50;               // overlay updates per second
  static constexpr uint32_t refreshDelay = 1000 / refreshRate;

  // A ring of LEDs mapped onto a contiguous slice of the physical strip.
  struct Ring {
    uint16_t start = 0;     // physical index of the first LED of the ring
    uint16_t count = 60;    // number of LEDs in the ring
    uint16_t top   = 0;     // offset (within the ring) that represents 12 o'clock
    bool     cw    = true;  // true: time advances clockwise (increasing LED index)

    // Keep the ring inside the strip; never let count become 0 while validating.
    void validate(uint16_t defCount) {
      uint16_t total = strip.getLengthTotal();
      if (total == 0) return;
      if (start >= total) start = 0;
      if (count == 0) count = defCount;
      if (start + count > total) count = total - start;
      if (count == 0) count = 1;
      if (top >= count) top = top % count;
    }

    // Map a logical offset (0..count-1, measured clockwise from 12 o'clock)
    // to a physical LED index, honouring direction and 12-o'clock offset.
    int16_t ledAtOffset(int32_t off) const {
      if (count == 0) return -1;
      off %= (int32_t)count;
      if (off < 0) off += count;
      int32_t dir = cw ? off : (int32_t)(count - off) % count;
      int32_t idx = ((int32_t)top + dir) % count;
      return (int16_t)(start + idx);
    }

    // Map a fractional progress around the dial [0,1) to the nearest LED.
    int16_t ledAtProgress(double progress) const {
      if (count == 0) return -1;
      int32_t off = (int32_t)lround(progress * count);
      return ledAtOffset(off);
    }
  };

  // ---- configuration (exposed in the UI and stored in flash) ----
  bool     enabled            = false;

  Ring     minuteRing;                       // defaults: start 0, count 60
  Ring     hourRing;                         // set in ctor: start 60, count 24

  bool     secondsEnabled     = true;
  bool     minuteMarksEnabled = false;       // a dim mark on every minute LED
  bool     hourMarksEnabled   = true;        // a brighter mark every 5 minutes (12 marks)
  bool     hourRingMarks      = true;        // dim mark at each of the 12 hour positions

  uint32_t hourColor          = 0x0000FF;    // hour hand
  uint32_t minuteColor        = 0x00FF00;    // minute hand
  uint32_t secondColor        = 0xFF0000;    // second hand
  uint32_t minuteMarkColor    = 0x101010;    // per-minute tick marks
  uint32_t hourMarkColor      = 0x303030;    // 5-minute / hour tick marks

  uint8_t  overlayBrightness  = 255;         // scales every overlay element
  bool     blendColors        = false;       // false: overwrite, true: additive blend

  // ---- runtime ----
  bool     initDone           = false;
  uint32_t lastOverlayDraw    = 0;

  void validateAndUpdate() {
    minuteRing.validate(60);
    hourRing.validate(24);
    if (overlayBrightness == 0) overlayBrightness = 1; // 0 would hide everything; keep usable
  }

  static inline uint32_t qadd32(uint32_t c1, uint32_t c2) {
    return RGBW32(qadd8(R(c1), R(c2)), qadd8(G(c1), G(c2)), qadd8(B(c1), B(c2)), qadd8(W(c1), W(c2)));
  }
  static inline uint32_t scale32(uint32_t c, uint8_t scale) {
    return RGBW32(scale8(R(c), scale), scale8(G(c), scale), scale8(B(c), scale), scale8(W(c), scale));
  }

  // Write one overlay pixel, applying global overlay brightness and blend mode.
  void putPixel(int16_t n, uint32_t c) {
    if (n < 0 || n >= (int16_t)strip.getLengthTotal()) return;
    c = scale32(c, overlayBrightness);
    if (blendColors) c = qadd32(strip.getPixelColor(n), c);
    strip.setPixelColor((uint16_t)n, c);
  }

  static String colorToHexString(uint32_t c) {
    char buffer[9];
    sprintf(buffer, "%06X", (unsigned)(c & 0xFFFFFF));
    return buffer;
  }

  static bool hexStringToColor(String const& s, uint32_t& c, uint32_t def) {
    char *ep;
    unsigned long long r = strtoull(s.c_str(), &ep, 16);
    if (*ep == 0) { c = (uint32_t)r; return true; }
    c = def;
    return false;
  }

public:
  DualRingClockUsermod() {
    // Default layout: minute ring first (0..59), hour ring right after it.
    minuteRing.start = 0;  minuteRing.count = 60;
    hourRing.start   = 60; hourRing.count   = 24;
  }

  void setup() override {
    initDone = true;
    validateAndUpdate();
  }

  // Keep the overlay smooth (so the second hand ticks) by re-triggering show().
  void loop() override {
    if (!enabled) return;
    if (millis() - lastOverlayDraw > refreshDelay) strip.trigger();
  }

  void handleOverlayDraw() override {
    if (!enabled) return;
    lastOverlayDraw = millis();

    uint8_t  s    = second(localTime);
    uint8_t  m    = minute(localTime);
    uint8_t  h    = hour(localTime) % 12;

    double secondP = s / 60.0;
    double minuteP = (m + secondP) / 60.0;        // minute hand creeps with seconds
    double hourP   = (h + m / 60.0) / 12.0;       // hour hand creeps with minutes

    // --- minute ring: tick marks (drawn first, hands overwrite them) ---
    if (minuteMarksEnabled) {
      for (uint8_t i = 0; i < 60; ++i)
        putPixel(minuteRing.ledAtProgress(i / 60.0), minuteMarkColor);
    }
    if (hourMarksEnabled) {
      for (uint8_t i = 0; i < 60; i += 5)
        putPixel(minuteRing.ledAtProgress(i / 60.0), hourMarkColor);
    }

    // --- minute ring: second + minute hands ---
    if (secondsEnabled)
      putPixel(minuteRing.ledAtProgress(secondP), secondColor);
    putPixel(minuteRing.ledAtProgress(minuteP), minuteColor);

    // --- hour ring: 12 hour positions as segments of (count/12) LEDs ---
    uint16_t ledsPerHour = hourRing.count >= 12 ? hourRing.count / 12 : 1;
    if (hourRingMarks) {
      for (uint8_t hp = 0; hp < 12; ++hp)
        putPixel(hourRing.ledAtOffset((int32_t)hp * ledsPerHour), hourMarkColor);
    }
    // Light the whole segment for the current hour (rounded with the minute fraction).
    uint16_t hourSegStart = (uint16_t)lround(hourP * 12.0) % 12 * ledsPerHour;
    for (uint16_t k = 0; k < ledsPerHour; ++k)
      putPixel(hourRing.ledAtOffset((int32_t)hourSegStart + k), hourColor);
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root[F("u")];
    if (user.isNull()) user = root.createNestedObject(F("u"));

    JsonArray arr = user.createNestedArray(F("Dual-Ring Clock"));
    if (!enabled) { arr.add(F("disabled")); return; }
    char buf[12];
    sprintf(buf, "%02d:%02d:%02d", hour(localTime), minute(localTime), second(localTime));
    arr.add(buf);
  }

  void addToConfig(JsonObject& root) override {
    validateAndUpdate();

    JsonObject top = root.createNestedObject(F("Dual-Ring Clock"));
    top[F("Overlay Enabled")]            = enabled;

    top[F("Minute Ring First LED")]      = minuteRing.start;
    top[F("Minute Ring LED Count")]      = minuteRing.count;
    top[F("Minute Ring 12oClock LED")]   = minuteRing.top;
    top[F("Minute Ring Clockwise")]      = minuteRing.cw;

    top[F("Hour Ring First LED")]        = hourRing.start;
    top[F("Hour Ring LED Count")]        = hourRing.count;
    top[F("Hour Ring 12oClock LED")]     = hourRing.top;
    top[F("Hour Ring Clockwise")]        = hourRing.cw;

    top[F("Show Seconds")]               = secondsEnabled;
    top[F("Show Minute Marks")]          = minuteMarksEnabled;
    top[F("Show Hour Marks (5min)")]     = hourMarksEnabled;
    top[F("Show Hour Ring Marks")]       = hourRingMarks;

    top[F("Hour Hand Color (RRGGBB)")]   = colorToHexString(hourColor);
    top[F("Minute Hand Color (RRGGBB)")] = colorToHexString(minuteColor);
    top[F("Second Hand Color (RRGGBB)")] = colorToHexString(secondColor);
    top[F("Minute Mark Color (RRGGBB)")] = colorToHexString(minuteMarkColor);
    top[F("Hour Mark Color (RRGGBB)")]   = colorToHexString(hourMarkColor);

    top[F("Overlay Brightness (0-255)")] = overlayBrightness;
    top[F("Blend With Effect")]          = blendColors;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("Dual-Ring Clock")];
    bool configComplete = !top.isNull();

    String color;
    configComplete &= getJsonValue(top[F("Overlay Enabled")], enabled, false);

    configComplete &= getJsonValue(top[F("Minute Ring First LED")],    minuteRing.start, 0);
    configComplete &= getJsonValue(top[F("Minute Ring LED Count")],    minuteRing.count, 60);
    configComplete &= getJsonValue(top[F("Minute Ring 12oClock LED")], minuteRing.top, 0);
    configComplete &= getJsonValue(top[F("Minute Ring Clockwise")],    minuteRing.cw, true);

    configComplete &= getJsonValue(top[F("Hour Ring First LED")],    hourRing.start, 60);
    configComplete &= getJsonValue(top[F("Hour Ring LED Count")],    hourRing.count, 24);
    configComplete &= getJsonValue(top[F("Hour Ring 12oClock LED")], hourRing.top, 0);
    configComplete &= getJsonValue(top[F("Hour Ring Clockwise")],    hourRing.cw, true);

    configComplete &= getJsonValue(top[F("Show Seconds")],           secondsEnabled, true);
    configComplete &= getJsonValue(top[F("Show Minute Marks")],      minuteMarksEnabled, false);
    configComplete &= getJsonValue(top[F("Show Hour Marks (5min)")], hourMarksEnabled, true);
    configComplete &= getJsonValue(top[F("Show Hour Ring Marks")],   hourRingMarks, true);

    configComplete &= getJsonValue(top[F("Hour Hand Color (RRGGBB)")],   color, F("0000FF")) && hexStringToColor(color, hourColor, 0x0000FF);
    configComplete &= getJsonValue(top[F("Minute Hand Color (RRGGBB)")], color, F("00FF00")) && hexStringToColor(color, minuteColor, 0x00FF00);
    configComplete &= getJsonValue(top[F("Second Hand Color (RRGGBB)")], color, F("FF0000")) && hexStringToColor(color, secondColor, 0xFF0000);
    configComplete &= getJsonValue(top[F("Minute Mark Color (RRGGBB)")], color, F("101010")) && hexStringToColor(color, minuteMarkColor, 0x101010);
    configComplete &= getJsonValue(top[F("Hour Mark Color (RRGGBB)")],   color, F("303030")) && hexStringToColor(color, hourMarkColor, 0x303030);

    configComplete &= getJsonValue(top[F("Overlay Brightness (0-255)")], overlayBrightness, 255);
    configComplete &= getJsonValue(top[F("Blend With Effect")],          blendColors, false);

    if (initDone) validateAndUpdate();
    return configComplete;
  }

  uint16_t getId() override {
    return USERMOD_ID_UNSPECIFIED;
  }
};

static DualRingClockUsermod dual_ring_clock;
REGISTER_USERMOD(dual_ring_clock);
