#include <Arduino.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <math.h>
#include <LittleFS.h>

#include "app_config.h"
#include "debug_log.h"
#include "storage_service.h"
#include "wifi_manager.h"
#include "camera_service.h"
#include "analyzer.h"
#include "diehl_reader.h"
#include "mqtt_service.h"
#include "web_server.h"
#include "ota_service.h"
#include "light_service.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
WebServer server(80);

AppConfig cfg;

String evaluateDiehlState(AppConfig &cfg);
static String evaluateMedian3State(AppConfig &cfg);

String lastPayload = "{}";
String lastGoodPayload = "{}";

unsigned long lastEvalMs = 0;
unsigned long lastHealthMs = 0;
unsigned long preCaptureStartedMs = 0;

WifiModeState wifiState = WIFI_MODE_AP_SETUP;

static const int CAMERA_RESTART_AFTER_FAILS = 3;
static const unsigned long HEALTH_INTERVAL_MS = 60000UL;
static const uint16_t MQTT_BUFFER_SIZE_DEFAULT = 2048;
static const uint16_t MQTT_BUFFER_SIZE_WITH_IMAGE = 32768;

static bool preCaptureActive = false;

static bool majority3Bool(bool a, bool b, bool c) {
  return (a + b + c) >= 2;
}

static double median3Double(double a, double b, double c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

String runActiveEvaluation(AppConfig &cfg) {
  if (cfg.diehl.enabled) {
    LOGD("Running Diehl evaluation\n");
    return evaluateDiehlState(cfg);
  }

  LOGD("Running standard median-3 evaluation\n");
  return evaluateMedian3State(cfg);
}

static String buildHealthPayload() {
  JsonDocument doc;

  doc["uptime_s"] = millis() / 1000UL;

  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  doc["ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";

  doc["mqtt_connected"] = mqtt.connected();

  doc["camera_ok"] = cameraIsHealthy();
  doc["camera_model"] = cfg.camera.model;
  doc["camera_sensor"] = cameraSensorName();
  doc["camera_frame"] = String(cfg.camera.width) + "x" + String(cfg.camera.height);

  doc["camera_capture_failures"] = cameraCaptureFailureCount();

  doc["light_enabled"] = cfg.light.enabled;
  doc["light_gpio"] = cfg.light.gpio;
  doc["light_brightness"] = cfg.light.brightness;
  doc["light_settle_ms"] = cfg.light.settle_ms;
  doc["light_capture_only"] = cfg.light.capture_only;

  doc["heap_free"] = ESP.getFreeHeap();

  String out;
  serializeJson(doc, out);
  return out;
}

static bool acquireStateFrame(JsonDocument &doc) {
  camera_fb_t *fb = cameraCapture();

  if (!fb) {
    LOGE("cameraCapture() failed during median acquisition\n");
    cameraMarkCaptureFailure();
    return false;
  }

  bool ok = evaluateStateToJson(fb, cfg, doc);

  cameraRelease(fb);
  cameraMarkCaptureSuccess();

  return ok;
}

static String evaluateMedian3State(AppConfig &cfg) {
  JsonDocument d1, d2, d3;

  bool useSeriesLight = (cfg.light.enabled && cfg.light.capture_only) ||
                        (cfg.light2.enabled && cfg.light2.capture_only);

  if (useSeriesLight) {
    lightsOnForCapture(cfg);

    // Stale Frames aus den Buffern leeren: Mit GRAB_WHEN_EMPTY sind bei Inaktivität
    // alle fb_count Buffer mit alten Frames (vor lightOn) gefüllt.
    // Ohne diesen Flush würden d1 und d2 Frames ohne Beleuchtung enthalten,
    // was den Majority-Vote für Symbol-ROIs kippen kann (last_state-Seiteneffekt).
    for (int i = 0; i < CAMERA_FB_COUNT; i++) {
      camera_fb_t *dummy = cameraCapture();
      if (dummy) cameraRelease(dummy);
    }
  }

  bool ok1 = acquireStateFrame(d1);
  delay(30);

  bool ok2 = acquireStateFrame(d2);
  delay(30);

  bool ok3 = acquireStateFrame(d3);

  // lightOff wird NICHT hier aufgerufen – das Licht bleibt an, damit ein
  // nachfolgender Image-Capture (maybePublishMqttImage) ohne LED-Unterbrechung läuft.
  // Der Aufrufer ist verantwortlich, lightOff() nach dem Image-Capture aufzurufen.

  JsonDocument out;
  out["valid"] = true;

  for (const auto &roi : cfg.rois) {
    if (roi.type == "symbol") {
      bool v1 = ok1 ? (d1[roi.id] | false) : false;
      bool v2 = ok2 ? (d2[roi.id] | false) : false;
      bool v3 = ok3 ? (d3[roi.id] | false) : false;

      bool v = majority3Bool(v1, v2, v3);
      out[roi.id] = v;

      LOGD("median symbol roi=%s values=%d,%d,%d -> %d\n",
           roi.id.c_str(), v1, v2, v3, v);
    } else if (roi.type == "sevenseg") {
      double v1 = ok1 ? (d1[roi.id] | -1.0) : -1.0;
      double v2 = ok2 ? (d2[roi.id] | -1.0) : -1.0;
      double v3 = ok3 ? (d3[roi.id] | -1.0) : -1.0;

      double validVals[3];
      int count = 0;
      if (v1 >= 0) validVals[count++] = v1;
      if (v2 >= 0) validVals[count++] = v2;
      if (v3 >= 0) validVals[count++] = v3;

      if (count == 0) {
        out["valid"] = false;
        out[roi.id] = -1;
        LOGD("median sevenseg roi=%s values=%f,%f,%f -> invalid\n",
             roi.id.c_str(), v1, v2, v3);
      } else if (count == 1) {
        if (roi.decimal_places > 0) {
          out[roi.id] = validVals[0];
        } else {
          out[roi.id] = (int)round(validVals[0]);
        }
      } else if (count == 2) {
        double v = (validVals[0] + validVals[1]) / 2.0;
        if (roi.decimal_places > 0) {
          out[roi.id] = v;
        } else {
          out[roi.id] = (int)round(v);
        }
      } else {
        double v = median3Double(validVals[0], validVals[1], validVals[2]);
        if (roi.decimal_places > 0) {
          out[roi.id] = v;
        } else {
          out[roi.id] = (int)round(v);
        }
      }
    }
  }

  String payload;
  serializeJson(out, payload);
  return payload;
}

static bool payloadIsValidState(const String &payload) {
  JsonDocument doc;

  if (deserializeJson(doc, payload)) return false;

  return doc["valid"] | false;
}

static void maybeRestartCamera() {
  if (cameraCaptureFailureCount() >= CAMERA_RESTART_AFTER_FAILS) {
    LOGI("Camera watchdog triggered after %lu capture failures\n",
         cameraCaptureFailureCount());

    if (cameraRestart(cfg)) {
      LOGI("Camera restart successful\n");
    } else {
      LOGE("Camera restart failed\n");
    }
  }
}

static uint16_t mqttBufferSizeForConfig(const AppConfig &cfg) {
  return cfg.mqtt.image_enabled ? MQTT_BUFFER_SIZE_WITH_IMAGE : MQTT_BUFFER_SIZE_DEFAULT;
}

static void maybePublishMqttImage(PubSubClient &mqtt, const AppConfig &cfg) {
  if (!cfg.mqtt.enabled || !cfg.mqtt.image_enabled) return;

  LOGI("MQTT image snapshot: capturing...\n");
  camera_fb_t *fb = captureWithLight(cfg);
  if (!fb) {
    LOGE("Image capture failed for MQTT snapshot\n");
    return;
  }

  uint8_t *jpg_buf = nullptr;
  size_t jpg_len = 0;
  // Half resolution + lower quality to keep MQTT payload small
  bool ok = cameraToJpegScaled(fb, 40, &jpg_buf, &jpg_len);
  cameraRelease(fb);

  if (!ok || !jpg_buf || jpg_len == 0) {
    if (jpg_buf) free(jpg_buf);
    LOGE("Image JPEG conversion failed for MQTT snapshot\n");
    return;
  }

  mqttPublishImage(mqtt, cfg, jpg_buf, jpg_len);
  free(jpg_buf);
}

static bool isPreCaptureEnabled(const AppConfig &cfg) {
  return cfg.pre_capture.enabled;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  LOGI("Booting Display Reader\n");
  esp_ota_mark_app_valid_cancel_rollback();

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    LOGI("Running partition: label=%s subtype=%d address=0x%08x size=%u\n",
         running->label,
         running->subtype,
         (unsigned)running->address,
         (unsigned)running->size);
  }

  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
  if (next) {
    LOGI("Next OTA partition: label=%s subtype=%d address=0x%08x size=%u\n",
         next->label,
         next->subtype,
         (unsigned)next->address,
         (unsigned)next->size);
  }

  // Mount LittleFS before loadDefaultConfig so devicePersistentUid() is available.
  if (!storageBegin()) {
    LOGE("LittleFS mount failed\n");
  } else {
    LOGI("LittleFS mounted\n");
  }

  loadDefaultConfig(cfg);

  if (LittleFS.exists("/config.json")) {
    if (loadConfigFromFile(cfg)) {
      LOGI("Config loaded from /config.json\n");
    } else {
      LOGI("Config file exists but failed to parse, using defaults\n");
    }
  } else {
    LOGI("No saved config found, using defaults\n");
  }

  LOGI("Debug level: %d\n", cfg.debug_level);

  // NeoPixel/RMT VOR WiFi initialisieren.
  // WiFi belegt ~80 KB internen DRAM; danach findet rmtInit keinen
  // ausreichend großen zusammenhängenden Block mehr → Crash.
  // Muss auch vor cameraStart stehen (DMA-Fragmentation).
  LOGI("Free DRAM before light2 init: %u\n",
       heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  light2ApplyConfig(cfg);
  LOGI("Free DRAM after light2 init: %u\n",
       heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

  wifiState = wifiBegin(cfg);

  mqttApplyConfig(mqtt, cfg);
  mqttSetGlobalClient(&mqtt);
  uint16_t mqttBufferSize = mqttBufferSizeForConfig(cfg);
  mqtt.setBufferSize(mqttBufferSize);

  LOGI("MQTT buffer size set to %u\n", mqttBufferSize);

  LOGI("MQTT config applied: host=%s port=%u enabled=%d\n",
       cfg.mqtt.host.c_str(),
       cfg.mqtt.port,
       cfg.mqtt.enabled);

  if (cfg.light.gpio >= 0) {
    pinMode(cfg.light.gpio, INPUT);
    delay(50);
  }

  if (cfg.diehl.enabled && cfg.diehl.trigger_gpio >= 0) {
    pinMode(cfg.diehl.trigger_gpio, OUTPUT);

    bool idle = cfg.diehl.trigger_invert ? HIGH : LOW;
    digitalWrite(cfg.diehl.trigger_gpio, idle);
  }

  if (!cameraStart(cfg)) {
    LOGE("Camera start failed\n");
  }

  lightApplyConfig(cfg);

  if (wifiState == WIFI_MODE_STA_OK) {
    otaBegin();
  } else {
    LOGI("OTA disabled in AP mode\n");
  }

  setupWebServer(server, mqtt, cfg, lastPayload);

  LOGI("HTTP server started\n");
}

void loop() {
  server.handleClient();
  mqtt.loop();

  if (wifiState == WIFI_MODE_STA_OK) {
    otaHandle();
  }

  unsigned long now = millis();
  unsigned long intervalMs = (unsigned long)cfg.capture_interval_sec * 1000UL;
  unsigned long preCaptureDelayMs =
      (cfg.pre_capture.delay_ms > 0) ? (unsigned long)cfg.pre_capture.delay_ms : 0UL;

  if (!preCaptureActive) {
    if (!g_liveStreamTriggerActive && now - lastEvalMs >= intervalMs) {
      if (!cfg.mqtt.enabled) {
        LOGI("Periodic capture skipped: MQTT disabled\n");
      } else if (isPreCaptureEnabled(cfg)) {
        lastEvalMs = now;
        preCaptureTrigger(cfg);
        preCaptureStartedMs = now;
        preCaptureActive = true;

        LOGD("PreCapture ON, waiting %lu ms before acquisition\n", preCaptureDelayMs);
      } else {
        lastEvalMs = now;
        LOGD("Periodic evaluation triggered\n");

        String newPayload = runActiveEvaluation(cfg);

        if (payloadIsValidState(newPayload)) {
          lastPayload = newPayload;
          lastGoodPayload = newPayload;
          LOGD("New valid payload accepted\n");
        } else {
          LOGE("New payload invalid, keeping last valid payload\n");

          if (lastGoodPayload.length() > 2) {
            lastPayload = lastGoodPayload;
          } else {
            lastPayload = newPayload;
          }
        }

        mqttPublishState(mqtt, cfg, lastPayload);
        maybePublishMqttImage(mqtt, cfg);
        lightOff(cfg);  // capture_only-Kanäle aus; Dauerlicht bleibt an

        LOGD("MQTT state payload: %s\n", lastPayload.c_str());

        maybeRestartCamera();
      }
    }
  } else {
    if (!g_liveStreamTriggerActive && now - preCaptureStartedMs >= preCaptureDelayMs) {
      preCaptureActive = false;
      mqttPublishPreCapture(cfg, false);

      if (!cfg.mqtt.enabled) {
        LOGI("PreCapture evaluation skipped: MQTT disabled\n");
      } else {
        LOGD("PreCapture delay elapsed, starting evaluation\n");

        String newPayload = runActiveEvaluation(cfg);

        if (payloadIsValidState(newPayload)) {
          lastPayload = newPayload;
          lastGoodPayload = newPayload;
          LOGD("New valid payload accepted\n");
        } else {
          LOGE("New payload invalid, keeping last valid payload\n");

          if (lastGoodPayload.length() > 2) {
            lastPayload = lastGoodPayload;
          } else {
            lastPayload = newPayload;
          }
        }

        mqttPublishState(mqtt, cfg, lastPayload);
        maybePublishMqttImage(mqtt, cfg);
        lightOff(cfg);  // capture_only-Kanäle aus; Dauerlicht bleibt an

        LOGD("MQTT state payload: %s\n", lastPayload.c_str());

        maybeRestartCamera();
      }
    }
  }

  if (now - lastHealthMs >= HEALTH_INTERVAL_MS) {
    lastHealthMs = now;

    String health = buildHealthPayload();

    mqttPublishHealth(mqtt, cfg, health);

    LOGD("MQTT health payload: %s\n", health.c_str());
  }
}