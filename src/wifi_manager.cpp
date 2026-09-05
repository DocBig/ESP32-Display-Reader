#include "wifi_manager.h"
#include <WiFi.h>
#include "debug_log.h"

static String g_mode = "AP";
static String g_apSsid = "DisplayReader-Setup";

static bool connectToConfiguredWifi(const AppConfig &cfg) {
  if (cfg.wifi.ssid.length() == 0) {
    LOGI("No WiFi configured\n");
    return false;
  }

  WiFi.mode(WIFI_STA);

  if (cfg.wifi.hostname.length() > 0) {
    WiFi.setHostname(cfg.wifi.hostname.c_str());
    LOGD("Hostname set to: %s\n", cfg.wifi.hostname.c_str());
  }

  WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());

  LOGI("Connecting WiFi to SSID: %s\n", cfg.wifi.ssid.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    LOGI("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    LOGD("RSSI: %d dBm\n", WiFi.RSSI());
    g_mode = "STA";
    return true;
  }

  Serial.println();
  LOGE("WiFi connect failed\n");
  return false;
}

static void startSetupAP() {
  // Start in AP_STA so the STA interface is available for network scanning
  // without needing a mode switch later.
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);

  uint64_t mac = ESP.getEfuseMac();
  char buf[32];
  snprintf(buf, sizeof(buf), "DisplayReader-%04X", (uint16_t)(mac & 0xFFFF));
  g_apSsid = String(buf);

  WiFi.softAP(g_apSsid.c_str());

  LOGI("Setup AP started: %s\n", g_apSsid.c_str());
  LOGI("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  g_mode = "AP_STA";
}

WifiModeState wifiBegin(AppConfig &cfg) {
  if (connectToConfiguredWifi(cfg)) {
    return WIFI_MODE_STA_OK;
  }

  startSetupAP();
  return WIFI_MODE_AP_SETUP;
}

String wifiCurrentModeString() {
  return g_mode;
}

String wifiApSsid() {
  return g_apSsid;
}