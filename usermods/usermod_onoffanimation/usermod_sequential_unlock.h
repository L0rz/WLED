#pragma once
#include "wled.h"

extern uint16_t transitionDelay;  // globale WLED-Überblendzeit


class UsermodSequentialUnlock : public Usermod {
private:
  uint8_t savedBri = 128;

  enum UnlockState {
    UNLOCK_IDLE,
    UNLOCK_UP,
    UNLOCK_DOWN
  };

  UnlockState state = UNLOCK_IDLE;
  int unlockedLeds = 0;
  unsigned long lastStepTime = 0;
  bool shutdownRequested = false;

  bool enableUnlockUp = true;
  bool enableUnlockDown = true;
  uint16_t unlockDelay = 30;

  constexpr static uint16_t UNLOCK_DELAY_MIN = 5;
  constexpr static uint16_t UNLOCK_DELAY_MAX = 10000;
  constexpr static uint16_t UNLOCK_DELAY_DEFAULT = 30;

  bool restorePending = false;
  unsigned long restoreTime = 0;

  uint16_t originalTransition = 0;

public:
  void setup() override {
    unlockedLeds = 0;
    lastStepTime = millis();
  }

  void loop() override {
    // 🔄 Helligkeit automatisch merken
    static uint8_t lastBri = 0;
    if (bri != lastBri && bri > 1 && !shutdownRequested) {
      savedBri = bri;
      lastBri = bri;
      Serial.print("savedBri aktualisiert: ");
      Serial.println(savedBri);
    }

    static bool lastRealOn = false;
    bool realNowOn = (bri > 1);

    // 🟢 Einschalten erkannt
    if (!lastRealOn && realNowOn && enableUnlockUp && !shutdownRequested) {
      state = UNLOCK_UP;
      unlockedLeds = 0;
      lastStepTime = millis();
      Serial.println("Einschaltanimation gestartet");
    }

    // 🔴 Ausschalten erkannt
    if (lastRealOn && !realNowOn && enableUnlockDown && !shutdownRequested) {
      shutdownRequested = true;
      state = UNLOCK_DOWN;
      unlockedLeds = strip.getLengthTotal();
      lastStepTime = millis();

      // Transition anpassen
      originalTransition = transitionDelay;
      transitionDelay = unlockDelay * strip.getLengthTotal() + 300;
      Serial.print("Transition temporär gesetzt auf: ");
      Serial.println(transitionDelay);

      bri = 1;
      Serial.println("Ausschaltanimation gestartet");
    }

    // 💤 Ausschaltanimation abgeschlossen
    static unsigned long shutdownDoneTime = 0;
    if (shutdownRequested && state == UNLOCK_DOWN && unlockedLeds == 0 && shutdownDoneTime == 0) {
      shutdownDoneTime = millis();
    }
    if (shutdownDoneTime > 0 && millis() - shutdownDoneTime > 500) {
      for (int i = 0; i < strip.getLengthTotal(); i++) {
        strip.setPixelColor(i, 0);
      }
      strip.show();
      bri = 0;

      transitionDelay = originalTransition;
      Serial.print("Transition zurückgesetzt auf: ");
      Serial.println(transitionDelay);

      stateUpdated(CALL_MODE_DIRECT_CHANGE);
      shutdownRequested = false;
      state = UNLOCK_IDLE;
      shutdownDoneTime = 0;
      Serial.println("Ausschaltanimation abgeschlossen");
    }

    // ✨ Einschaltanimation abgeschlossen – Helligkeit & Power wiederherstellen
    if (state == UNLOCK_UP && unlockedLeds >= strip.getLengthTotal()) {
      if (savedBri < 1) savedBri = 128;
      restorePending = true;
      restoreTime = millis() + 100;
      state = UNLOCK_IDLE;
    }

    if (restorePending && millis() > restoreTime) {
      bri = savedBri;
      nightlightActive = false;
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
      restorePending = false;
      Serial.print("Helligkeit und Power wiederhergestellt auf: ");
      Serial.println(savedBri);
    }
    

    lastRealOn = realNowOn;
  }

  void handleOverlayDraw() override {
    if (bri == 0 && state == UNLOCK_IDLE && !shutdownRequested) {
      for (int i = 0; i < strip.getLengthTotal(); i++) {
        strip.setPixelColor(i, 0);
      }
      return;
    }

    unsigned long now = millis();

    if ((state == UNLOCK_UP || state == UNLOCK_DOWN) && now - lastStepTime >= unlockDelay) {
      if (state == UNLOCK_UP && unlockedLeds < strip.getLengthTotal()) {
        unlockedLeds++;
      } else if (state == UNLOCK_DOWN && unlockedLeds > 0) {
        unlockedLeds--;
      }
      lastStepTime = now;
    }

    if (state == UNLOCK_UP) {
      for (int i = unlockedLeds; i < strip.getLengthTotal(); i++) {
        strip.setPixelColor(i, 0);
      }
    }

    if (state == UNLOCK_DOWN && shutdownRequested) {
      for (int i = unlockedLeds; i < strip.getLengthTotal(); i++) {
        strip.setPixelColor(i, 0);
      }
    }
  }

  void addToJsonInfo(JsonObject &root) override {
    root["SequentialUnlock"] = unlockedLeds;
    const char* stateStr[] = {"IDLE", "UP", "DOWN"};
    root["seqUnlockState"] = stateStr[state];
  }

  void addToJsonState(JsonObject &root) override {
    root["enableUnlockUp"] = enableUnlockUp;
    root["enableUnlockDown"] = enableUnlockDown;
    root["unlockDelay"] = unlockDelay;
  }

  void readFromJsonState(JsonObject &root) override {
    if (root.containsKey("enableUnlockUp")) enableUnlockUp = root["enableUnlockUp"];
    if (root.containsKey("enableUnlockDown")) enableUnlockDown = root["enableUnlockDown"];
    if (root.containsKey("unlockDelay")) {
      uint16_t val = root["unlockDelay"];
      if (val >= UNLOCK_DELAY_MIN && val <= UNLOCK_DELAY_MAX) unlockDelay = val;
    }
  }

  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject("sequentialunlock");
    top["enableUp"] = enableUnlockUp;
    top["enableDown"] = enableUnlockDown;
    top["delay"] = unlockDelay;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root["sequentialunlock"];
    if (!top.isNull()) {
      enableUnlockUp = top["enableUp"].isNull() ? true : top["enableUp"];
      enableUnlockDown = top["enableDown"].isNull() ? true : top["enableDown"];
      if (top.containsKey("delay") && top["delay"].is<uint16_t>()) {
        uint16_t val = top["delay"];
        if (val >= UNLOCK_DELAY_MIN && val <= UNLOCK_DELAY_MAX) {
          unlockDelay = val;
        } else {
          unlockDelay = UNLOCK_DELAY_DEFAULT;
        }
      } else {
        unlockDelay = UNLOCK_DELAY_DEFAULT;
      }
    }
    return true;
  }

  void appendConfigData() override {
    oappend(SET_F("addCheckbox('enableUnlockUp','Einschaltanimation aktivieren',"));
    oappend(enableUnlockUp ? "true" : "false");
    oappend(SET_F(");"));

    oappend(SET_F("addCheckbox('enableUnlockDown','Ausschaltanimation aktivieren',"));
    oappend(enableUnlockDown ? "true" : "false");
    oappend(SET_F(");"));

    oappend(SET_F("addNumber('unlockDelay','Verzögerung (ms)',5,1000,5,"));
    oappend(String(unlockDelay).c_str());
    oappend(SET_F(");"));
  }

  uint16_t getId() override {
    return USERMOD_ID_UNSPECIFIED;
  }
};
