#pragma once

#include <Arduino.h>
#include "app_config.h"

extern AppConfig cfg;

#define LOGE(...) do { if (cfg.debug_level >= 0) { Serial.print("[ERR] "); Serial.printf(__VA_ARGS__); } } while(0)
#define LOGI(...) do { if (cfg.debug_level >= 1) { Serial.print("[INF] "); Serial.printf(__VA_ARGS__); } } while(0)
#define LOGD(...) do { if (cfg.debug_level >= 2) { Serial.print("[DBG] "); Serial.printf(__VA_ARGS__); } } while(0)
#define LOGA(...) do { if (cfg.debug_level >= 3) { Serial.print("[ANL] "); Serial.printf(__VA_ARGS__); } } while(0)