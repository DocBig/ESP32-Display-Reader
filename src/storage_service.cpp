#include "storage_service.h"
#include <LittleFS.h>
#include "debug_log.h"

static const char *CONFIG_FILE = "/config.json";

bool storageBegin() {
  bool ok = LittleFS.begin(true);
  if (!ok) {
    LOGE("LittleFS.begin(true) failed\n");
  }
  return ok;
}

bool loadConfigFromFile(AppConfig &cfg) {
  if (!LittleFS.exists(CONFIG_FILE)) {
    LOGI("Config file not found: %s\n", CONFIG_FILE);
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    LOGE("Failed to open config file for reading\n");
    return false;
  }

  String json = f.readString();
  f.close();

  bool ok = configFromJson(json, cfg);
  if (!ok) {
    LOGE("Config JSON parse failed\n");
  } else {
    LOGI("Config loaded successfully\n");
  }

  return ok;
}

bool saveConfigToFile(const AppConfig &cfg) {
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    LOGE("Failed to open config file for writing\n");
    return false;
  }

  String json = configToJson(cfg);
  size_t written = f.print(json);
  f.close();

  bool ok = written == json.length();
  if (ok) {
    LOGI("Config saved to %s (%u bytes)\n", CONFIG_FILE, (unsigned)written);
  } else {
    LOGE("Config save incomplete (%u/%u bytes)\n", (unsigned)written, (unsigned)json.length());
  }

  return ok;
}