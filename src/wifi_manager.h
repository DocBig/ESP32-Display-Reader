#pragma once

#include <Arduino.h>
#include "app_config.h"

enum WifiModeState {
  WIFI_MODE_STA_OK,
  WIFI_MODE_AP_SETUP
};

WifiModeState wifiBegin(AppConfig &cfg);
String wifiCurrentModeString();
String wifiApSsid();