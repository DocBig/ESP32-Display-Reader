#include "ota_service.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "app_config.h"
#include "debug_log.h"

extern AppConfig cfg;

void otaBegin() {
  const char *hostname =
      (cfg.wifi.hostname.length() > 0) ? cfg.wifi.hostname.c_str() : "display-reader";

  ArduinoOTA.setHostname(hostname);

  ArduinoOTA.onStart([]() {
    LOGI("OTA start\n");
  });

  ArduinoOTA.onEnd([]() {
    LOGI("OTA end\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned int lastPct = 0;
    unsigned int pct = (progress * 100U) / total;
    if (pct != lastPct && (pct % 10 == 0 || pct == 100)) {
      lastPct = pct;
      LOGI("OTA progress: %u%%\n", pct);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    LOGE("OTA error[%u]\n", (unsigned)error);
    if (error == OTA_AUTH_ERROR) LOGE("OTA auth failed\n");
    else if (error == OTA_BEGIN_ERROR) LOGE("OTA begin failed\n");
    else if (error == OTA_CONNECT_ERROR) LOGE("OTA connect failed\n");
    else if (error == OTA_RECEIVE_ERROR) LOGE("OTA receive failed\n");
    else if (error == OTA_END_ERROR) LOGE("OTA end failed\n");
  });

  ArduinoOTA.begin();
  LOGI("OTA ready: hostname=%s ip=%s\n",
       hostname,
       WiFi.localIP().toString().c_str());
}

void otaHandle() {
  ArduinoOTA.handle();
}