#include "web_server.h"

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <cstring>

#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "debug_log.h"
#include "storage_service.h"
#include "wifi_manager.h"
#include "camera_service.h"
#include "light_service.h"
#include "analyzer.h"
#include "mqtt_service.h"
#include "web_ui.h"

#include "mqtt_ha_discovery.h"

static bool g_updateRunning = false;
static size_t g_updateTotal = 0;
static size_t g_updateWritten = 0;
static bool g_updateOk = false;

bool g_liveStreamTriggerActive = false;
static String g_updateError = "";
static const esp_partition_t *g_updateTargetPartition = nullptr;
static const uint16_t MQTT_BUFFER_SIZE_DEFAULT = 2048;
static const uint16_t MQTT_BUFFER_SIZE_WITH_IMAGE = 32768;

static uint16_t mqttBufferSizeForConfig(const AppConfig &cfg) {
  return cfg.mqtt.image_enabled ? MQTT_BUFFER_SIZE_WITH_IMAGE : MQTT_BUFFER_SIZE_DEFAULT;
}

void setupWebServer(WebServer &server, PubSubClient &mqtt, AppConfig &cfg, String &lastPayload) {
  server.on("/", HTTP_GET, [&]() {
    LOGD("HTTP GET /\n");
    String mode = wifiCurrentModeString();
    if (mode == "AP" || mode == "AP_STA") {
      server.send_P(200, "text/html", WIFI_SETUP_HTML);
    } else {
      server.send_P(200, "text/html", INDEX_HTML);
    }
  });

  server.on("/live", HTTP_GET, [&]() {
    LOGD("HTTP GET /live\n");

    if (!cfg.stream.enabled) {
      server.send(403, "text/plain", "Stream disabled. Enable it in Camera settings.");
      return;
    }

    String html =
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<title>Live Stream</title>"
      "<style>"
      "*{box-sizing:border-box;margin:0;padding:0}"
      "body{background:#111;color:#eee;font-family:sans-serif;"
      "display:flex;flex-direction:column;height:100vh;overflow:hidden}"
      "#img{flex:1;min-height:0;width:100%;object-fit:contain;background:#000;display:block}"
      ".bar{display:flex;align-items:center;gap:10px;padding:8px 12px;"
      "background:#1e1e22;border-top:1px solid #333;flex-shrink:0;min-height:44px}"
      "#btnLight{padding:6px 14px;border-radius:8px;border:1px solid #47474f;"
      "background:#2a2a2e;color:#eee;cursor:pointer;font-size:14px}"
      "#btnLight.on{background:#c26a00;border-color:#e07b00}"
      "#btnTrigger{padding:6px 14px;border-radius:8px;border:1px solid #47474f;"
      "background:#2a2a2e;color:#eee;cursor:pointer;font-size:14px}"
      "#btnTrigger.on{background:#1a6e3a;border-color:#22a050}"
      "#fps{font-size:12px;color:#888;margin-left:auto}"
      "</style></head><body>"
      "<img id='img' alt='Live Stream'>"
      "<div class='bar'>"
      "<button id='btnLight' onclick='toggleLight()'>💡 Light</button>"
      "<button id='btnTrigger' onclick='toggleTrigger()'>⚡ Pre-Capture Trigger</button>"
      "<span id='fps'></span>"
      "</div>"
      "<script>"
      "let lightOn=false,triggerOn=false,fc=0,fts=Date.now(),t=null;"
      "function toggleLight(){"
      "lightOn=!lightOn;"
      "const b=document.getElementById('btnLight');"
      "b.textContent=lightOn?'💡 Light ON':'💡 Light';"
      "b.className=lightOn?'on':'';"
      "}"
      "function setTrigger(state){"
      "fetch('/api/pre_capture?state='+(state?'on':'off'),{method:'POST'});"
      "triggerOn=state;"
      "const b=document.getElementById('btnTrigger');"
      "b.textContent=state?'⚡ Pre-Capture ON':'⚡ Pre-Capture Trigger';"
      "b.className=state?'on':'';"
      "}"
      "function toggleTrigger(){setTrigger(!triggerOn);}"
      "function load(){"
      "const img=document.getElementById('img');"
      "img.onload=()=>{"
      "fc++;const n=Date.now();"
      "if(n-fts>=1000){document.getElementById('fps').textContent=(fc)+' fps';fc=0;fts=n;}"
      "t=setTimeout(load,150);"
      "};"
      "img.onerror=()=>{t=setTimeout(load,500);};"
      "img.src='/api/preview?light='+(lightOn?1:0)+'&t='+Date.now();"
      "}"
      "load();"
      "window.addEventListener('beforeunload',()=>{"
      "clearTimeout(t);"
      "if(triggerOn)navigator.sendBeacon('/api/pre_capture?state=off');"
      "navigator.sendBeacon('/api/preview_light?state=off');"
      "});"
      "</script></body></html>";

    server.send(200, "text/html", html);
  });

  server.on("/api/status", HTTP_GET, [&]() {
    JsonDocument doc;

    doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi"]["ip"] = (WiFi.status() == WL_CONNECTED)
                            ? WiFi.localIP().toString()
                            : WiFi.softAPIP().toString();
    doc["wifi"]["mode"] = wifiCurrentModeString();
    doc["wifi"]["ap_ssid"] = wifiApSsid();
    doc["wifi"]["ssid"] = cfg.wifi.ssid;
    doc["wifi"]["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

    doc["mqtt"]["connected"] = mqtt.connected();
    doc["mqtt"]["image_enabled"] = cfg.mqtt.image_enabled;

    doc["camera"]["width"] = cfg.camera.width;
    doc["camera"]["height"] = cfg.camera.height;
    doc["camera"]["model"] = cfg.camera.model;
    doc["camera"]["sensor"] = cameraSensorName();
    doc["camera"]["frame"] = String(cfg.camera.width) + "x" + String(cfg.camera.height);
    doc["camera"]["jpeg_quality"] = cfg.camera.jpeg_quality;
    doc["camera"]["auto_exposure"] = cfg.camera.auto_exposure;
    doc["camera"]["auto_gain"] = cfg.camera.auto_gain;
    doc["camera"]["auto_whitebalance"] = cfg.camera.auto_whitebalance;
    doc["camera"]["aec_value"] = cfg.camera.aec_value;
    doc["camera"]["agc_gain"] = cfg.camera.agc_gain;
    doc["camera"]["ok"] = cameraIsHealthy();
    doc["camera"]["capture_failures"] = cameraCaptureFailureCount();

    doc["light"]["enabled"] = cfg.light.enabled;
    doc["light"]["gpio"] = cfg.light.gpio;
    doc["light"]["brightness"] = cfg.light.brightness;
    doc["light"]["settle_ms"] = cfg.light.settle_ms;
    doc["light"]["capture_only"] = cfg.light.capture_only;
    doc["light"]["invert"] = cfg.light.invert;
    doc["stream"]["enabled"] = cfg.stream.enabled;

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
    doc["system"]["fw_version"] = FW_VERSION;
    doc["system"]["heap_free"] = ESP.getFreeHeap();
    doc["system"]["uptime_s"] = millis() / 1000UL;
    doc["system"]["temperature"] = (int)temperatureRead();

    doc["ota"]["running"] = g_updateRunning;
    doc["ota"]["total"] = g_updateTotal;
    doc["ota"]["written"] = g_updateWritten;
    doc["ota"]["ok"] = g_updateOk;
    doc["ota"]["error"] = g_updateError;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_GET, [&]() {
    LOGD("HTTP GET /api/config\n");
    server.send(200, "application/json", configToJson(cfg));
  });

  server.on("/api/config", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/config\n");

    String body = server.arg("plain");
    String oldCameraModel = cfg.camera.model;
    int oldCameraWidth = cfg.camera.width;
    int oldCameraHeight = cfg.camera.height;
    int oldLight2Gpio = cfg.light2.gpio;
    int oldLight2Count = cfg.light2.led_count;
    bool oldLight2Enabled = cfg.light2.enabled;

    if (!configFromJson(body, cfg)) {
      LOGE("Invalid JSON in /api/config\n");
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }

    if (!saveConfigToFile(cfg)) {
      LOGE("Failed to save config after /api/config\n");
      server.send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
      return;
    }

    LOGI("Config updated via web\n");

    bool camModelChanged = (oldCameraModel != cfg.camera.model);
    bool camFrameChanged = (oldCameraWidth != cfg.camera.width) ||
                           (oldCameraHeight != cfg.camera.height);

    // RMT-Kanal kann nach cameraStart() nicht mehr neu allokiert werden (DRAM-Heap
    // zu fragmentiert). Neustart erforderlich wenn GPIO/Count sich ändern oder
    // WS2812B erstmals aktiviert wird.
    bool light2NeedsReinit = (cfg.light2.enabled && !oldLight2Enabled) ||
                             (cfg.light2.gpio != oldLight2Gpio) ||
                             (cfg.light2.led_count != oldLight2Count);

    if (camModelChanged || light2NeedsReinit) {
      LOGI("Restarting device (cam_model=%d light2_reinit=%d)\n",
           camModelChanged, light2NeedsReinit);
      server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
      delay(500);
      ESP.restart();
      return;
    }

    if (camFrameChanged) {
      LOGI("Camera frame changed: %dx%d -> %dx%d, restarting camera\n",
           oldCameraWidth,
           oldCameraHeight,
           cfg.camera.width,
           cfg.camera.height);

      if (!cameraRestart(cfg)) {
        LOGE("Camera restart failed after frame change\n");
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"camera restart failed\"}");
        return;
      }
    }

    cameraApplySettings(cfg);
    lightApplyConfig(cfg);
    light2ApplyConfig(cfg);

    // Antwort zuerst senden, dann MQTT — Discovery kann mehrere Sekunden dauern
    // (50 ms delay pro ROI + 500 ms Retry) und würde sonst den HTTP-Client zum
    // Timeout bringen (Failed to fetch).
    server.send(200, "application/json", "{\"ok\":true}");

    mqttApplyConfig(mqtt, cfg);
    mqtt.setBufferSize(mqttBufferSizeForConfig(cfg));
    mqttPublishHomeAssistantDiscovery(mqtt, cfg);
  });


  server.on("/api/wifi_scan", HTTP_GET, [&]() {
    LOGD("HTTP GET /api/wifi_scan\n");

    wifi_mode_t mode = WiFi.getMode();
    if (mode != WIFI_AP_STA && mode != WIFI_STA) {
      // Pure AP mode has no STA interface – switch to AP_STA so scanning works.
      WiFi.mode(WIFI_AP_STA);
      delay(500);
    }

    WiFi.scanDelete();
    // Async scan: the WiFi task continues running (keeps AP alive for connected clients).
    // Blocking scan would starve the AP during channel hops and drop the browser connection.
    WiFi.scanNetworks(true, true);

    unsigned long scanStart = millis();
    int n = WIFI_SCAN_RUNNING;
    while (n == WIFI_SCAN_RUNNING && millis() - scanStart < 10000UL) {
      delay(100);
      n = WiFi.scanComplete();
    }

    if (n < 0) {
      LOGE("WiFi scan failed or timed out (result=%d)\n", n);
      WiFi.scanDelete();
      server.send(200, "application/json", "{\"ok\":false,\"count\":0,\"networks\":[]}");
      return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["count"] = 0;
    JsonArray nets = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;

      JsonObject item = nets.add<JsonObject>();
      item["ssid"] = ssid;
      item["rssi"] = WiFi.RSSI(i);
      item["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    doc["count"] = nets.size();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);

    WiFi.scanDelete();
  });

  server.on("/api/wifi_setup", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/wifi_setup\n");

    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      LOGE("Invalid JSON in /api/wifi_setup\n");
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }

    const String ssid = jsonStringOrDefault(doc["ssid"], "");
    const String password = jsonStringOrDefault(doc["password"], "");
    const String hostname = jsonStringOrDefault(doc["hostname"], "display-reader");

    cfg.wifi.ssid = ssid;
    cfg.wifi.password = password;
    cfg.wifi.hostname = hostname;

    if (!saveConfigToFile(cfg)) {
      LOGE("Failed to save WiFi config\n");
      server.send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
      return;
    }

    LOGI("WiFi setup saved, trying STA connect while AP stays active\n");

    // Keep the setup AP alive so the browser can still receive the result page,
    // while we try to join the target WLAN in parallel.
    WiFi.mode(WIFI_AP_STA);

    if (hostname.length() > 0) {
      WiFi.setHostname(hostname.c_str());
    }

    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
      delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
      LOGE("WiFi setup test connect failed, staying in AP mode\n");
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_AP);
      server.send(200, "application/json",
                  "{\"ok\":false,\"connected\":false,\"error\":\"wifi connect failed\"}");
      return;
    }

    const String ip = WiFi.localIP().toString();
    LOGI("WiFi setup successful, STA IP: %s\n", ip.c_str());

    String resp = String("{\"ok\":true,\"connected\":true,\"ip\":\"") + ip +
                  "\",\"ssid\":\"" + ssid + "\",\"hostname\":\"" + hostname + "\"}";
    server.send(200, "application/json", resp);

    // Give the browser time to display the assigned IP before leaving AP mode.
    delay(12000);
    ESP.restart();
  });

  server.on("/api/snapshot", HTTP_GET, [&]() {
    LOGD("HTTP GET /api/snapshot\n");

    camera_fb_t *fb = captureWithLight(cfg);
    if (!fb) {
      server.send(500, "text/plain", "capture failed");
      return;
    }

    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool ok = cameraToJpeg(fb, cfg.camera.jpeg_quality, &jpg_buf, &jpg_len);
    cameraRelease(fb);

    if (!ok || !jpg_buf) {
      server.send(500, "text/plain", "jpeg conversion failed");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(jpg_len);
    server.send(200, "image/jpeg", "");
    server.client().write(jpg_buf, jpg_len);
    free(jpg_buf);
  });

  server.on("/api/preview", HTTP_GET, [&]() {
    LOGD("HTTP GET /api/preview\n");

    if (!cfg.stream.enabled) {
      server.send(403, "text/plain", "stream disabled");
      return;
    }

    bool withLight = server.hasArg("light") && server.arg("light") == "1";

    camera_fb_t *fb = capturePreview(cfg, withLight);
    if (!fb) {
      server.send(503, "text/plain", "capture failed");
      return;
    }

    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool ok = cameraToJpeg(fb, cfg.camera.jpeg_quality, &jpg_buf, &jpg_len);
    cameraRelease(fb);

    if (!ok || !jpg_buf) {
      if (jpg_buf) free(jpg_buf);
      server.send(503, "text/plain", "jpeg conversion failed");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.setContentLength(jpg_len);
    server.send(200, "image/jpeg", "");
    server.client().write(jpg_buf, jpg_len);
    free(jpg_buf);
  });

  server.on("/api/evaluate", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/evaluate\n");

    String body = server.arg("plain");
    if (body.length() > 2) {
      configFromJson(body, cfg);
      cameraApplySettings(cfg);
      lightApplyConfig(cfg);
    }

    camera_fb_t *fb = captureWithLight(cfg);
    if (!fb) {
      LOGE("capture failed in /api/evaluate\n");
      server.send(500, "application/json", "{\"ok\":false,\"error\":\"capture failed\"}");
      return;
    }

    String out = evaluateCurrent(fb, cfg);
    cameraRelease(fb);

    lastPayload = out;
    server.send(200, "application/json", out);
  });

  server.on("/api/light_test", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/light_test\n");

    AppConfig tmp = cfg;
    String body = server.arg("plain");

    if (body.length() > 2) {
      if (!configFromJson(body, tmp)) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
      }
    }

    if (!tmp.light.enabled || tmp.light.gpio < 0) {
      LOGE("light_test: light not configured (enabled=%d gpio=%d)\n",
           tmp.light.enabled, tmp.light.gpio);
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"Light not enabled or GPIO not set\"}");
      return;
    }

    lightApplyConfig(tmp);
    lightTestPulse(tmp, 1500);
    lightApplyConfig(cfg);

    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/light2_test", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/light2_test\n");

    AppConfig tmp = cfg;
    String body = server.arg("plain");

    if (body.length() > 2) {
      if (!configFromJson(body, tmp)) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
      }
    }

    if (!tmp.light2.enabled || tmp.light2.gpio < 0 || tmp.light2.led_count <= 0) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"Light2 not enabled or not configured\"}");
      return;
    }

    light2ApplyConfig(tmp);
    light2TestPulse(tmp, 1500);
    light2ApplyConfig(cfg);

    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/reboot", HTTP_POST, [&]() {
    LOGI("HTTP POST /api/reboot -> restarting device\n");
    server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    delay(500);
    ESP.restart();
  });

  server.on("/api/preview_light", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/preview_light\n");
    bool on = server.hasArg("state") && server.arg("state") == "on";
    if (on) {
      previewLightForceOn(cfg);
    } else {
      previewLightForceOff(cfg);
    }
    server.send(200, "application/json", on ? "{\"ok\":true,\"light\":1}" : "{\"ok\":true,\"light\":0}");
  });

  server.on("/api/pre_capture", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/pre_capture\n");
    bool state = server.hasArg("state") && server.arg("state") == "on";
    extern void mqttPublishPreCapture(const AppConfig &cfg, bool state);
    g_liveStreamTriggerActive = state;
    mqttPublishPreCapture(cfg, state);
    LOGI("Pre-capture trigger: %s\n", state ? "ON" : "OFF");
    server.send(200, "application/json", state ? "{\"ok\":true,\"state\":\"on\"}" : "{\"ok\":true,\"state\":\"off\"}");
  });

  server.on(
    "/api/update",
    HTTP_POST,
    [&]() {
      bool ok = !Update.hasError() &&
                Update.isFinished() &&
                g_updateError.length() == 0;

      const esp_partition_t *running = esp_ota_get_running_partition();
      const esp_partition_t *boot = esp_ota_get_boot_partition();
      const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

      if (running) {
        LOGI("OTA POST: running partition label=%s subtype=%d address=0x%08x size=%u\n",
             running->label,
             running->subtype,
             (unsigned)running->address,
             (unsigned)running->size);
      }

      if (boot) {
        LOGI("OTA POST: boot partition label=%s subtype=%d address=0x%08x size=%u\n",
             boot->label,
             boot->subtype,
             (unsigned)boot->address,
             (unsigned)boot->size);
      }

      if (next) {
        LOGI("OTA POST: next update partition label=%s subtype=%d address=0x%08x size=%u\n",
             next->label,
             next->subtype,
             (unsigned)next->address,
             (unsigned)next->size);
      }

      if (ok) {
        g_updateOk = true;
        g_updateRunning = false;
        g_updateError = "";

        LOGI("Browser OTA finished successfully\n");

        server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
        delay(1000);
        ESP.restart();
      } else {
        g_updateOk = false;
        g_updateRunning = false;

        if (g_updateError.length() == 0) {
          g_updateError = Update.errorString();
        }

        LOGE("Browser OTA failed: %s\n", g_updateError.c_str());
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"update failed\"}");
      }
    },
    [&]() {
      HTTPUpload &upload = server.upload();

      if (wifiCurrentModeString() != "STA") {
        LOGE("Browser OTA refused in AP mode\n");
        return;
      }

      if (upload.status == UPLOAD_FILE_START) {
        LOGI("Browser OTA upload start: %s size=%u\n", upload.filename.c_str(), (unsigned)upload.totalSize);

        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *boot = esp_ota_get_boot_partition();
        const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
        g_updateTargetPartition = next;

        if (running) {
          LOGI("OTA START: running partition label=%s subtype=%d address=0x%08x size=%u\n",
               running->label,
               running->subtype,
               (unsigned)running->address,
               (unsigned)running->size);
        }

        if (boot) {
          LOGI("OTA START: boot partition label=%s subtype=%d address=0x%08x size=%u\n",
               boot->label,
               boot->subtype,
               (unsigned)boot->address,
               (unsigned)boot->size);
        }

        if (next) {
          LOGI("OTA START: next update partition label=%s subtype=%d address=0x%08x size=%u\n",
               next->label,
               next->subtype,
               (unsigned)next->address,
               (unsigned)next->size);
        } else {
          LOGE("OTA START: no next update partition found\n");
        }

        g_updateRunning = true;
        g_updateOk = false;
        g_updateError = "";
        g_updateWritten = 0;
        g_updateTotal = upload.totalSize;

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          g_updateError = Update.errorString();
          LOGE("Update.begin failed: %s\n", g_updateError.c_str());
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (g_updateWritten == 0 && upload.currentSize > 0) {
          LOGI("OTA first chunk: 0x%02X 0x%02X 0x%02X 0x%02X\n",
               upload.buf[0], upload.buf[1], upload.buf[2], upload.buf[3]);
        }
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
          g_updateError = Update.errorString();
          LOGE("Update.write failed: %s\n", g_updateError.c_str());
        } else {
          g_updateWritten += upload.currentSize;
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) {
          g_updateError = Update.errorString();
          LOGE("Update.end failed: %s\n", g_updateError.c_str());
        } else {
          LOGI("Browser OTA upload complete: %u bytes\n", (unsigned)g_updateWritten);

          if (!g_updateTargetPartition) {
            g_updateError = "no target partition";
            LOGE("OTA END: target partition missing\n");
          } else {
            esp_err_t err = esp_ota_set_boot_partition(g_updateTargetPartition);
            if (err != ESP_OK) {
              g_updateError = String("esp_ota_set_boot_partition failed: ") + String((int)err);
              LOGE("OTA END: esp_ota_set_boot_partition failed: %d\n", (int)err);
            } else {
              const esp_partition_t *bootNow = esp_ota_get_boot_partition();
              if (bootNow) {
                LOGI("OTA END: boot partition now label=%s subtype=%d address=0x%08x size=%u\n",
                     bootNow->label,
                     bootNow->subtype,
                     (unsigned)bootNow->address,
                     (unsigned)bootNow->size);
              }

              if (!bootNow || strcmp(bootNow->label, g_updateTargetPartition->label) != 0) {
                g_updateError = "boot partition did not switch";
                LOGE("OTA END: boot partition did not switch to target\n");
              }
            }
          }
        }
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        g_updateRunning = false;
        g_updateOk = false;
        g_updateError = "aborted";
        LOGE("Browser OTA aborted\n");
      }
    }
  );

  server.on("/api/config/export", HTTP_GET, [&]() {
    LOGD("HTTP GET /api/config/export\n");
    
    String json = configToJson(cfg);
    
    server.sendHeader("Content-Disposition", "attachment; filename=\"display-reader-config.json\"");
    server.send(200, "application/json", json);
  });

  server.on("/api/config/import", HTTP_POST, [&]() {
    LOGD("HTTP POST /api/config/import\n");
    
    String body = server.arg("plain");
    
    if (body.length() == 0) {
      LOGE("Import: empty body\n");
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty body\"}");
      return;
    }
    
    AppConfig newCfg = cfg;
    if (!configFromJson(body, newCfg)) {
      LOGE("Import: invalid JSON or parse error\n");
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    
    cfg = newCfg;
    
    if (!saveConfigToFile(cfg)) {
      LOGE("Import: failed to save config\n");
      server.send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
      return;
    }
    
    LOGI("Config imported successfully\n");
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Config imported. Please restart the device.\"}");
  });

  server.begin();
}
