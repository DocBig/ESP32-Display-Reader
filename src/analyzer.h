#pragma once

#include <Arduino.h>
#include "esp_camera.h"
#include "app_config.h"

String evaluateCurrent(camera_fb_t *fb, AppConfig &cfg);
String evaluateStateOnly(camera_fb_t *fb, AppConfig &cfg);

// kompakte Einzelbild-Auswertung für Median/Voting
bool evaluateStateToJson(camera_fb_t *fb, AppConfig &cfg, JsonDocument &doc);

// --- Diehl / direkte ROI-Helfer ---
bool readSevenSegIntById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, int &value);
bool readSevenSegStringById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, String &value);
bool readSevenSegCharById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, char &value);