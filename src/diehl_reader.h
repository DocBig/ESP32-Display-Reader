#pragma once

#include "app_config.h"
#include "esp_camera.h"
#include <Arduino.h>
#include <map>
#include <set>

struct DiehlResult {
  std::map<int, String> pages;
  std::map<int, std::set<char>> errors;
  bool success = false;
};

class DiehlReader {
public:
  DiehlReader(AppConfig &cfg);

  bool run(DiehlResult &result);

private:
  AppConfig &cfg;

  bool readPage(camera_fb_t *fb, int &page);
  bool readValue(camera_fb_t *fb, String &value);

  void triggerNextPage();
  void setTrigger(bool active);

  void readErrorPage(DiehlResult &result);
};