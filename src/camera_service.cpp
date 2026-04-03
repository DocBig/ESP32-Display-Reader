#include "camera_service.h"
#include "img_converters.h"
#include "debug_log.h"
#include "light_service.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static bool g_cameraHealthy = false;
static unsigned long g_captureFailureCount = 0;
static AppConfig g_lastCfg;

static framesize_t cameraFrameSizeFromConfig(const AppConfig &cfg, int &resolvedW, int &resolvedH) {
  if (cfg.camera.width == 160 && cfg.camera.height == 120) {
    resolvedW = 160;
    resolvedH = 120;
    return FRAMESIZE_QQVGA;
  }

  if (cfg.camera.width == 320 && cfg.camera.height == 240) {
    resolvedW = 320;
    resolvedH = 240;
    return FRAMESIZE_QVGA;
  }

  if (cfg.camera.width == 640 && cfg.camera.height == 480) {
    resolvedW = 640;
    resolvedH = 480;
    return FRAMESIZE_VGA;
  }

  resolvedW = 800;
  resolvedH = 600;
  return FRAMESIZE_SVGA;
}

static bool fillCameraPins(camera_config_t &c, const String &model) {
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;

  if (model == "freenove_wrover") {
    c.pin_pwdn = -1;
    c.pin_reset = -1;
    c.pin_xclk = 21;
    c.pin_sccb_sda = 26;
    c.pin_sccb_scl = 27;

    c.pin_d7 = 35;
    c.pin_d6 = 34;
    c.pin_d5 = 39;
    c.pin_d4 = 36;
    c.pin_d3 = 19;
    c.pin_d2 = 18;
    c.pin_d1 = 5;
    c.pin_d0 = 4;

    c.pin_vsync = 25;
    c.pin_href = 23;
    c.pin_pclk = 22;
    return true;
  }

  if (model == "ai_thinker") {
    c.pin_pwdn = 32;
    c.pin_reset = -1;
    c.pin_xclk = 0;
    c.pin_sccb_sda = 26;
    c.pin_sccb_scl = 27;

    c.pin_d7 = 35;
    c.pin_d6 = 34;
    c.pin_d5 = 39;
    c.pin_d4 = 36;
    c.pin_d3 = 21;
    c.pin_d2 = 19;
    c.pin_d1 = 18;
    c.pin_d0 = 5;

    c.pin_vsync = 25;
    c.pin_href = 23;
    c.pin_pclk = 22;
    return true;
  }

  return false;
}

static void fillCameraConfig(camera_config_t &c, const AppConfig &cfg) {
  memset(&c, 0, sizeof(c));

  bool ok = fillCameraPins(c, cfg.camera.model);
  if (!ok) {
    LOGE("Unknown camera model: %s\n", cfg.camera.model.c_str());
  }

  c.pixel_format = PIXFORMAT_GRAYSCALE;
  int resolvedW = 320;
  int resolvedH = 240;
  c.frame_size = cameraFrameSizeFromConfig(cfg, resolvedW, resolvedH);

  if (cfg.camera.model == "ai_thinker") {
    // SVGA (800x600): 20 MHz ist bei OV2640 oft instabil (DMA-Timing, PSRAM-Bus).
    // 10 MHz ist stabiler auf Kosten von etwas langsamerer Frame-Rate.
    c.xclk_freq_hz = (c.frame_size == FRAMESIZE_SVGA) ? 10000000 : 20000000;
  } else {
    c.xclk_freq_hz = 10000000;
  }
  c.jpeg_quality = cfg.camera.jpeg_quality;
  c.fb_count = CAMERA_FB_COUNT;
  // GRAB_WHEN_EMPTY: Kamera pausiert wenn alle Buffer voll sind, statt ununterbrochen zu laufen.
  // Verhindert EV-VSYNC-OVF bei unserem Snapshot-auf-Abruf Muster.
  // GRAB_LATEST würde die Kamera kontinuierlich betreiben → Overflow wenn ESP32 kurz geblockt ist.
  // WICHTIG: Beim Capture müssen alle fb_count Buffer als Dummy verworfen werden, damit kein
  // veralteter Frame zurückgegeben wird (bei Inaktivität sind alle Buffer mit alten Frames gefüllt).
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  LOGD("Camera frame request=%dx%d resolved=%dx%d\n",
       cfg.camera.width,
       cfg.camera.height,
       resolvedW,
       resolvedH);
}

bool cameraApplySettings(const AppConfig &cfg) {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    LOGE("cameraApplySettings: sensor not available\n");
    return false;
  }

  s->set_exposure_ctrl(s, cfg.camera.auto_exposure ? 1 : 0);
  s->set_gain_ctrl(s, cfg.camera.auto_gain ? 1 : 0);
  s->set_whitebal(s, cfg.camera.auto_whitebalance ? 1 : 0);

  s->set_vflip(s, cfg.camera.vflip ? 1 : 0);
  s->set_hmirror(s, cfg.camera.hflip ? 1 : 0);

  int aec = cfg.camera.aec_value;
  if (aec < 0) aec = 0;
  if (aec > 1200) aec = 1200;

  int agc = cfg.camera.agc_gain;
  if (agc < 0) agc = 0;
  if (agc > 30) agc = 30;

  s->set_aec_value(s, aec);
  s->set_agc_gain(s, agc);

  LOGI("Camera tuning applied\n");
  LOGD("Camera tuning: auto_exp=%d auto_gain=%d auto_wb=%d aec=%d agc=%d\n",
       cfg.camera.auto_exposure,
       cfg.camera.auto_gain,
       cfg.camera.auto_whitebalance,
       aec,
       agc);

  return true;
}

bool cameraStart(const AppConfig &cfg) {
  g_lastCfg = cfg;

  camera_config_t c;
  fillCameraConfig(c, cfg);

  LOGI("Initializing camera, model=%s\n", cfg.camera.model.c_str());
  LOGD("Camera init params: xclk=%u frame=%dx%d jpeg_quality=%d\n",
       c.xclk_freq_hz,
       cfg.camera.width,
       cfg.camera.height,
       cfg.camera.jpeg_quality);

  // PWDN-Pin Power-Cycle vor jedem Init.
  // Nach Software-Reset (OTA, Watchdog) bleibt der OV2640 in einem undefinierten
  // Zustand und antwortet nicht auf dem I2C-Bus (Fehler 0x105).
  // Ein expliziter HIGH→LOW-Zyklus setzt den Sensor sauber zurück.
  if (cfg.camera.model == "ai_thinker") {
    pinMode(32, OUTPUT);
    digitalWrite(32, HIGH); // Sensor power-down
    delay(200);
    digitalWrite(32, LOW);  // Sensor power-up
    delay(200);
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    LOGE("esp_camera_init failed: 0x%x, retrying...\n", err);
    esp_camera_deinit();
    delay(300);
    if (cfg.camera.model == "ai_thinker") {
      digitalWrite(32, HIGH);
      delay(200);
      digitalWrite(32, LOW);
      delay(200);
    }
    err = esp_camera_init(&c);
    if (err != ESP_OK) {
      LOGE("esp_camera_init retry failed: 0x%x\n", err);
      g_cameraHealthy = false;
      return false;
    }
    LOGI("Camera init succeeded on retry\n");
  }

  g_cameraHealthy = true;
  g_captureFailureCount = 0;

  cameraApplySettings(cfg);

  // OV2640 AEC/AGC braucht mehrere Frames zum Einschwingen.
  // Warm-up-Frames verwerfen, damit der erste echte Capture korrekte Helligkeitswerte liefert.
  // Ohne Warm-up: alle Segment-Mittelwerte nahe 0 → bitsToDigit() → -1 → payload invalid.
  for (int i = 0; i < 5; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(100);
  }

  LOGI("Camera init OK\n");
  LOGD("Camera settings: model=%s frame=%dx%d jpeg_quality=%d\n",
       cfg.camera.model.c_str(),
       cfg.camera.width,
       cfg.camera.height,
       cfg.camera.jpeg_quality);
  return true;
}

bool cameraRestart(const AppConfig &cfg) {
  LOGI("Restarting camera\n");

  // Failure-Counter zurücksetzen, damit der Watchdog nicht sofort wieder auslöst.
  g_captureFailureCount = 0;

  // Licht-Pin vor dem Kamera-Restart freigeben (AI-Thinker GPIO4 Flash-LED).
  if (cfg.light.enabled && cfg.light.gpio >= 0) {
    pinMode(cfg.light.gpio, INPUT);
    delay(20);
  }

  esp_camera_deinit();
  delay(300);

  bool ok = cameraStart(cfg);

  // Licht immer wiederherstellen – auch wenn Camera-Restart fehlschlug.
  // Sonst bleibt GPIO4 dauerhaft als INPUT und die LED lässt sich nicht mehr schalten.
  lightApplyConfig(cfg);
  light2ApplyConfig(cfg);
  return ok;
}

camera_fb_t* cameraCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    // EV-VSYNC-OVF: WiFi-DMA hat den PSRAM-SPI-Bus belegt → kurz yielden
    // und einmal wiederholen. Nach einem WiFi-Burst (typ. <10 ms) ist der
    // Bus frei und die Kamera kann einen sauberen Frame liefern.
    LOGI("esp_camera_fb_get() failed, retrying after yield\n");
    vTaskDelay(pdMS_TO_TICKS(25));
    fb = esp_camera_fb_get();
  }
  if (!fb) {
    LOGE("esp_camera_fb_get() failed after retry\n");
    g_cameraHealthy = false;
    g_captureFailureCount++;
  } else {
    g_cameraHealthy = true;
  }
  return fb;
}

void cameraRelease(camera_fb_t *fb) {
  if (fb) esp_camera_fb_return(fb);
}

bool cameraToJpeg(camera_fb_t *fb, int quality, uint8_t **jpg_buf, size_t *jpg_len) {
  bool ok = frame2jpg(fb, quality, jpg_buf, jpg_len);
  if (!ok) {
    LOGE("frame2jpg() failed\n");
  } else {
    LOGD("frame2jpg() ok, len=%u\n", (unsigned)*jpg_len);
  }
  return ok;
}

// Downscale grayscale image by 2x (average 2x2 blocks) then encode as JPEG.
bool cameraToJpegScaled(camera_fb_t *fb, int quality, uint8_t **jpg_buf, size_t *jpg_len) {
  if (!fb || !fb->buf || fb->len == 0) return false;

  uint16_t sw = fb->width / 2;
  uint16_t sh = fb->height / 2;
  size_t sbytes = (size_t)sw * sh;

  uint8_t *sbuf = (uint8_t *)malloc(sbytes);
  if (!sbuf) {
    LOGE("cameraToJpegScaled: malloc failed\n");
    return false;
  }

  const uint8_t *src = fb->buf;
  uint16_t stride = fb->width;
  for (uint16_t y = 0; y < sh; y++) {
    for (uint16_t x = 0; x < sw; x++) {
      uint16_t r0 = y * 2, c0 = x * 2;
      uint16_t avg = (uint16_t)src[r0 * stride + c0]
                   + src[r0 * stride + c0 + 1]
                   + src[(r0+1) * stride + c0]
                   + src[(r0+1) * stride + c0 + 1];
      sbuf[y * sw + x] = (uint8_t)(avg >> 2);
    }
  }

  bool ok = fmt2jpg(sbuf, sbytes, sw, sh, PIXFORMAT_GRAYSCALE, quality, jpg_buf, jpg_len);
  free(sbuf);

  if (!ok) {
    LOGE("cameraToJpegScaled: fmt2jpg failed\n");
  } else {
    LOGD("cameraToJpegScaled: ok, %ux%u -> len=%u\n", sw, sh, (unsigned)*jpg_len);
  }
  return ok;
}

bool cameraIsHealthy() {
  return g_cameraHealthy;
}

void cameraMarkCaptureFailure() {
  g_cameraHealthy = false;
  g_captureFailureCount++;
}

void cameraMarkCaptureSuccess() {
  g_cameraHealthy = true;
}

unsigned long cameraCaptureFailureCount() {
  return g_captureFailureCount;
}

const char* cameraSensorName() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return "unknown";

  switch (s->id.PID) {
    case OV2640_PID:
      return "OV2640";
    case OV3660_PID:
      return "OV3660";
    case OV5640_PID:
      return "OV5640";
    default:
      return "unknown";
  }
}
