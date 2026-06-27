#pragma once
#include "wled.h"
#include <vector>
#include <algorithm>

class HighlightUsermod : public Usermod {
private:
  struct Highlight {
    uint16_t startLed;
    uint16_t endLed;
    uint32_t color;
    uint8_t mode; // 0=static, 1=blink, 2=pulse
    uint8_t brightness;
    unsigned long lastToggle;
    bool visible;
  };

  std::vector<Highlight> highlights;
  bool enabled = true;
  unsigned long now;

public:
  void setup() override {
  server.on("/highlight", HTTP_GET, [this](AsyncWebServerRequest *request){
    this->handleRequest(request);
  });
}


  void loop() override {
    now = millis();
  }

  void handleOverlayDraw() override {
    if (!enabled) return;
  
    for (auto& hl : highlights) {
      bool draw = true;
      uint8_t r = R(hl.color);
      uint8_t g = G(hl.color);
      uint8_t b = B(hl.color);
  
      if (hl.mode == 1) { // blink
        if (now - hl.lastToggle > 500) {
          hl.visible = !hl.visible;
          hl.lastToggle = now;
        }
        draw = hl.visible;
      } else if (hl.mode == 2) { // pulse
        uint16_t cycleTime = 4000; // 4 Sekunden Atmen
      
        float rawSin = sin(2.0f * PI * now / (float)cycleTime);
        float eased = rawSin * rawSin;  // softes "Ease-In-Out" Feeling
        uint8_t scaled = (uint8_t)(((eased + 1.0f) / 2.0f) * hl.brightness);
      
        r = (r * scaled) / 255;
        g = (g * scaled) / 255;
        b = (b * scaled) / 255;
      }
      
       else {
        // static mode, r/g/b unverändert
        r = (r * hl.brightness) / 255;
        g = (g * hl.brightness) / 255;
        b = (b * hl.brightness) / 255;
      }
  
      if (draw) {
        for (uint16_t i = hl.startLed; i <= hl.endLed && i < strip.getLengthTotal(); i++) {
          strip.setPixelColor(i, r, g, b);
        }
      }
    }
  }
  

  bool handleRequest(AsyncWebServerRequest* request) {
    if (!enabled || request->url().indexOf("/highlight") != 0) return false;

    if (request->hasParam("clear")) {
      String val = request->getParam("clear")->value();
      if (val == "all") {
        highlights.clear();
      } else {
        uint16_t target = val.toInt();
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(),
          [target](Highlight& h) {
            return h.startLed <= target && h.endLed >= target;
          }), highlights.end());
      }
    } else if (request->hasParam("start") && request->hasParam("end") && request->hasParam("color")) {
      Highlight hl;
      hl.startLed = request->getParam("start")->value().toInt();
      hl.endLed = request->getParam("end")->value().toInt();
      hl.color = strtoul(request->getParam("color")->value().c_str(), NULL, 16);
      hl.mode = 0;
      if (request->hasParam("mode")) {
        String m = request->getParam("mode")->value();
        if (m == "blink") hl.mode = 1;
        else if (m == "pulse") hl.mode = 2;
      }
      hl.brightness = request->hasParam("brightness") ?
        constrain(request->getParam("brightness")->value().toInt(), 0, 255) : 255;
      hl.visible = true;
      hl.lastToggle = now;
      highlights.push_back(hl);
    }

    request->send(200, "text/plain", "Highlight updated.");
    return true;
  }

  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject("highlightusermod");
    top["enabled"] = enabled;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root["highlightusermod"];
    if (!top.isNull()) {
      enabled = top["enabled"].as<bool>();
    }
    return true;
  }

  void appendConfigData() override {
    oappend(SET_F("addInfo('highlightusermod:enabled',1,'Highlight-Modus aktivieren');"));
  }

  uint16_t getId() override {
    return USERMOD_ID_UNSPECIFIED;
  }
};
