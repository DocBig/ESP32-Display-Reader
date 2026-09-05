#pragma once

#include <Arduino.h>
#include "esp_camera.h"
#include "app_config.h"

bool lightApplyConfig(const AppConfig &cfg);
void lightOn(const AppConfig &cfg);
void lightOff(const AppConfig &cfg);  // turns off ch1 + ch2
bool lightIsOn();                      // true if ch1 or ch2 is on
void lightTestPulse(const AppConfig &cfg, unsigned long ms);
camera_fb_t* captureWithLight(const AppConfig &cfg);

// WS2812B Ring (Channel 2)
bool light2ApplyConfig(const AppConfig &cfg);
void light2On(const AppConfig &cfg);
void light2Off(const AppConfig &cfg);
void light2TestPulse(const AppConfig &cfg, unsigned long ms);

// Turns on ch1 and/or ch2 depending on which are configured for capture-only.
// Used in evaluateMedian3State before the 3-frame capture sequence.
void lightsOnForCapture(const AppConfig &cfg);

// Explizite Light-Steuerung für Preview: withLight=true → Licht an; withLight=false → Licht aus.
// Funktioniert unabhängig vom capture_only-Modus.
camera_fb_t* capturePreview(const AppConfig &cfg, bool withLight);

void previewLightForceOff(const AppConfig &cfg);
void previewLightForceOn(const AppConfig &cfg);

void preCaptureTrigger(const AppConfig &cfg);