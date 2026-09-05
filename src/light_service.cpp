#include "light_service.h"
#include "camera_service.h"
#include "debug_log.h"
#include <Adafruit_NeoPixel.h>
#include "driver/gpio.h"

static const int LIGHT_PWM_FREQ = 5000;
static const int LIGHT_PWM_RESOLUTION = 8;

// === Channel 1 (PWM / digital) ===
static bool g_lightConfigured = false;
static int g_lightGpio = -1;
static bool g_lightUsePwm = false;
static bool g_previewLightOn = false;
static bool g_captureLightOn = false;

// === Channel 2 (WS2812B) ===
static Adafruit_NeoPixel *g_neopixel = nullptr;
static int g_light2Gpio = -1;
static int g_light2Count = 0;
static bool g_light2CaptureLightOn = false;
static bool g_light2PreviewOn = false;

static bool lightIsValid(const AppConfig &cfg) {
  return cfg.light.enabled && cfg.light.gpio >= 0;
}

static bool light2IsValid(const AppConfig &cfg) {
  return cfg.light2.enabled && cfg.light2.gpio >= 0 && cfg.light2.led_count > 0;
}

static int clampBrightness(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v;
}

static bool shouldUseDigitalOnly(const AppConfig &cfg) {
  (void)cfg;
  return false;  // ledcAttachChannel() mit explizitem Channel vermeidet Kamera-Konflikt
}

static int pwmForOn(const AppConfig &cfg) {
  const int b = clampBrightness(cfg.light.brightness);
  return cfg.light.invert ? (255 - b) : b;
}

static int pwmForOff(const AppConfig &cfg) {
  return cfg.light.invert ? 255 : 0;
}

static int digitalLevelForOn(const AppConfig &cfg) {
  return cfg.light.invert ? LOW : HIGH;
}

static int digitalLevelForOff(const AppConfig &cfg) {
  return cfg.light.invert ? HIGH : LOW;
}

bool lightApplyConfig(const AppConfig &cfg) {
  const int newGpio = lightIsValid(cfg) ? cfg.light.gpio : -1;

  if (g_lightConfigured && g_lightGpio >= 0 && g_lightGpio != newGpio) {
#if defined(ARDUINO_ARCH_ESP32)
    if (g_lightUsePwm) {
      ledcWrite(g_lightGpio, 0);
      ledcDetach(g_lightGpio);
    }
#endif
    pinMode(g_lightGpio, INPUT);
  }

  if (newGpio < 0) {
    g_lightConfigured = true;
    g_lightGpio = -1;
    g_lightUsePwm = false;
    return true;
  }

  const bool digitalOnly = shouldUseDigitalOnly(cfg);
  bool pwmOk = false;

  pinMode(newGpio, OUTPUT);

#if defined(ARDUINO_ARCH_ESP32)
  if (!digitalOnly) {
    // Channel 5: Kameratreiber (XCLK) belegt Channel 0 – expliziter Channel verhindert Konflikt
    pwmOk = ledcAttachChannel(newGpio, LIGHT_PWM_FREQ, LIGHT_PWM_RESOLUTION, 5);
    if (!pwmOk) {
      LOGE("ledcAttachChannel failed for GPIO %d, falling back to digital mode\n", newGpio);
    }
  }
#endif

  g_lightConfigured = true;
  g_lightGpio = newGpio;
  g_lightUsePwm = (!digitalOnly && pwmOk);

  if (cfg.light.capture_only) {
    if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
      ledcWrite(g_lightGpio, pwmForOff(cfg));
#endif
    } else {
      digitalWrite(g_lightGpio, digitalLevelForOff(cfg));
    }
  } else {
    if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
      ledcWrite(g_lightGpio, pwmForOn(cfg));
#endif
    } else {
      digitalWrite(g_lightGpio, digitalLevelForOn(cfg));
    }
  }

  LOGI("Light config applied: enabled=%d gpio=%d brightness=%d settle_ms=%d capture_only=%d invert=%d mode=%s\n",
       cfg.light.enabled,
       cfg.light.gpio,
       cfg.light.brightness,
       cfg.light.settle_ms,
       cfg.light.capture_only,
       cfg.light.invert,
       g_lightUsePwm ? "pwm" : "digital");

  return true;
}

void lightOn(const AppConfig &cfg) {
  if (!lightIsValid(cfg)) {
    LOGE("lightOn: not valid (enabled=%d gpio=%d)\n", cfg.light.enabled, cfg.light.gpio);
    return;
  }

  if (!g_lightConfigured || g_lightGpio != cfg.light.gpio) {
    if (!lightApplyConfig(cfg)) return;
  }

  if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
    ledcWrite(g_lightGpio, pwmForOn(cfg));
#endif
  } else {
    digitalWrite(g_lightGpio, digitalLevelForOn(cfg));
  }

  LOGI("Light ON: gpio=%d level=%d mode=%s\n",
       g_lightGpio, digitalLevelForOn(cfg), g_lightUsePwm ? "pwm" : "digital");

  g_captureLightOn = true;

  if (cfg.light.settle_ms > 0) {
    delay(cfg.light.settle_ms);
  }
}

bool lightIsOn() {
  return g_captureLightOn || g_light2CaptureLightOn;
}

// Internal: turns off ch1 only, does NOT affect ch2 state.
static void lightOffCh1(const AppConfig &cfg) {
  if (!g_lightConfigured || g_lightGpio < 0) return;

  if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
    ledcWrite(g_lightGpio, pwmForOff(cfg));
#endif
  } else {
    digitalWrite(g_lightGpio, digitalLevelForOff(cfg));
  }

  g_captureLightOn = false;
  g_previewLightOn = false;
  LOGI("Light OFF (ch1): gpio=%d\n", g_lightGpio);
}

void lightOff(const AppConfig &cfg) {
  // Ch1 — nur ausschalten wenn capture_only; Dauerlicht (capture_only=false) bleibt an.
  if (!lightIsValid(cfg) || cfg.light.capture_only) {
    lightOffCh1(cfg);
  }

  // Ch2 — nur ausschalten wenn capture_only; Dauerlicht bleibt an.
  if (!light2IsValid(cfg) || cfg.light2.capture_only) {
    if (g_neopixel && (g_light2CaptureLightOn || g_light2PreviewOn)) {
      g_neopixel->clear();
      g_neopixel->show();
      g_light2CaptureLightOn = false;
      g_light2PreviewOn = false;
      LOGI("Light OFF (ch2): gpio=%d\n", g_light2Gpio);
    }
  }
}

void lightTestPulse(const AppConfig &cfg, unsigned long ms) {
  if (!lightIsValid(cfg)) return;
  lightOn(cfg);
  delay(ms);
  lightOff(cfg);
}

camera_fb_t* captureWithLight(const AppConfig &cfg) {
  const bool doLight1 = lightIsValid(cfg) && cfg.light.capture_only;
  const bool doLight2 = light2IsValid(cfg) && cfg.light2.capture_only;

  if (doLight1 || doLight2) {
    const bool wasOn1 = g_captureLightOn;
    const bool wasOn2 = g_light2CaptureLightOn;

    if (doLight1 && !wasOn1) lightOn(cfg);    // includes ch1 settle delay
    if (doLight2 && !wasOn2) light2On(cfg);   // includes ch2 settle delay

    // Alle Framebuffer leeren: Mit GRAB_WHEN_EMPTY füllen sich bei Inaktivität
    // alle fb_count Buffer mit alten Frames. Nur durch Verwerfen aller Buffer
    // wird sichergestellt, dass der nächste Frame wirklich aktuell ist.
    for (int i = 0; i < CAMERA_FB_COUNT; i++) {
      camera_fb_t *dummy = cameraCapture();
      if (dummy) cameraRelease(dummy);
    }

    delay(30);

    camera_fb_t *fb = cameraCapture();

    // Nur die Kanäle ausschalten, die wir eingeschaltet haben
    if (doLight1 && !wasOn1) lightOffCh1(cfg);
    if (doLight2 && !wasOn2) light2Off(cfg);

    return fb;
  }

  // Alle Framebuffer leeren (s.o.)
  for (int i = 0; i < CAMERA_FB_COUNT; i++) {
    camera_fb_t *dummy = cameraCapture();
    if (dummy) cameraRelease(dummy);
  }

  delay(30);

  return cameraCapture();
}

// Licht für Preview: funktioniert unabhängig von light.enabled, nur gpio >= 0 nötig.
// So kann das Licht im Livemodus genutzt werden auch wenn es für den Capture-Zyklus
// deaktiviert ist (z.B. zum Einrichten von ROIs ohne permanentes Licht).
// Setzt das Licht für das Popup mit den vollen Einstellungen aus Lighting (PWM + Helligkeit)
// auf, unabhängig davon ob light.enabled gesetzt ist.
static void previewLightOn(const AppConfig &cfg) {
  if (cfg.light.gpio < 0) return;

  const int gpio = cfg.light.gpio;

  // Alten Pin aufräumen wenn GPIO gewechselt hat
  if (g_lightConfigured && g_lightGpio >= 0 && g_lightGpio != gpio) {
#if defined(ARDUINO_ARCH_ESP32)
    if (g_lightUsePwm) { ledcWrite(g_lightGpio, 0); ledcDetach(g_lightGpio); }
#endif
    pinMode(g_lightGpio, INPUT);
    g_lightConfigured = false;
  }

  // GPIO + LEDC aufsetzen wenn noch nicht konfiguriert
  if (!g_lightConfigured || g_lightGpio != gpio) {
    const bool digitalOnly = shouldUseDigitalOnly(cfg);
    bool pwmOk = false;
    pinMode(gpio, OUTPUT);
#if defined(ARDUINO_ARCH_ESP32)
    if (!digitalOnly) {
      pwmOk = ledcAttachChannel(gpio, LIGHT_PWM_FREQ, LIGHT_PWM_RESOLUTION, 5);
    }
#endif
    g_lightConfigured = true;
    g_lightGpio = gpio;
    g_lightUsePwm = (!digitalOnly && pwmOk);
  }

  // Einschalten mit Helligkeitseinstellung aus Config
  if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
    ledcWrite(g_lightGpio, pwmForOn(cfg));
#endif
  } else {
    digitalWrite(g_lightGpio, digitalLevelForOn(cfg));
  }

  if (cfg.light.settle_ms > 0) delay(cfg.light.settle_ms);

  // Ch2 — nur wenn NeoPixel bereits initialisiert (vor cameraStart).
  // Kein Reinit hier: RMT kann nach cameraStart nicht mehr allokiert werden.
  if (g_neopixel && light2IsValid(cfg)) {
    int r2 = (cfg.light2.color_r * cfg.light2.brightness) / 255;
    int g2 = (cfg.light2.color_g * cfg.light2.brightness) / 255;
    int b2 = (cfg.light2.color_b * cfg.light2.brightness) / 255;
    g_neopixel->fill(Adafruit_NeoPixel::Color(r2, g2, b2));
    g_neopixel->show();
    if (!g_light2PreviewOn && cfg.light2.settle_ms > 0) delay(cfg.light2.settle_ms);
    g_light2PreviewOn = true;
  }
}

static void previewLightOff(const AppConfig &cfg) {
  if (cfg.light.gpio >= 0 && g_lightGpio >= 0) {
    if (g_lightUsePwm) {
#if defined(ARDUINO_ARCH_ESP32)
      ledcWrite(g_lightGpio, pwmForOff(cfg));
#endif
    } else {
      digitalWrite(g_lightGpio, digitalLevelForOff(cfg));
    }
  }

  // Ch2
  if (g_neopixel && g_light2PreviewOn) {
    g_neopixel->clear();
    g_neopixel->show();
    g_light2PreviewOn = false;
  }
}

camera_fb_t* capturePreview(const AppConfig &cfg, bool withLight) {
  // Im Livemodus: Licht nutzbar wenn gpio gesetzt UND enabled.
  const bool gpioAvail = cfg.light.gpio >= 0 && cfg.light.enabled;
  // Dauerlicht-Modus (enabled=true + capture_only=false): Licht permanent an.
  const bool lightAlwaysOn = lightIsValid(cfg) && !cfg.light.capture_only;

  // Licht-State nur bei Änderung anpassen – kein An/Aus pro Frame.
  if (gpioAvail && !lightAlwaysOn) {
    if (withLight && !g_previewLightOn) {
      previewLightOn(cfg);   // einmalig einschalten inkl. settle_ms
      g_previewLightOn = true;
    } else if (!withLight && g_previewLightOn) {
      previewLightOff(cfg);  // einmalig ausschalten
      g_previewLightOn = false;
    }
  }

  if (gpioAvail && lightAlwaysOn && !withLight) {
    // Dauerlicht kurz ausschalten für Preview ohne Licht
    previewLightOff(cfg);
  }

  // Alle Framebuffer leeren, damit der nächste Frame aktuell ist.
  // Mit GRAB_WHEN_EMPTY füllen sich bei Inaktivität alle fb_count Buffer mit
  // alten Frames – ein einzelner Dummy reicht nicht aus.
  for (int i = 0; i < CAMERA_FB_COUNT; i++) {
    camera_fb_t *dummy = cameraCapture();
    if (dummy) cameraRelease(dummy);
  }
  delay(80);

  camera_fb_t *fb = cameraCapture();

  if (gpioAvail && lightAlwaysOn && !withLight) {
    // Dauerlicht wiederherstellen
    previewLightOn(cfg);
  }

  return fb;
}

void previewLightForceOff(const AppConfig &cfg) {
  previewLightOff(cfg);
  g_previewLightOn = false;
}

void previewLightForceOn(const AppConfig &cfg) {
  if (cfg.light.gpio < 0 || !cfg.light.enabled) return;
  previewLightOn(cfg);
  g_previewLightOn = true;
}

// =============================================================================
// Channel 2 – WS2812B Ring
// =============================================================================

static void light2InitStrip(const AppConfig &cfg) {
  const int gpio = cfg.light2.gpio;
  const int count = cfg.light2.led_count;

  if (g_neopixel && (g_light2Gpio != gpio || g_light2Count != count)) {
    g_neopixel->clear();
    g_neopixel->show();
    delete g_neopixel;
    g_neopixel = nullptr;
  }

  if (!g_neopixel) {
    gpio_reset_pin((gpio_num_t)gpio);
    g_neopixel = new Adafruit_NeoPixel(count, gpio, NEO_GRB + NEO_KHZ800);
    if (!g_neopixel) { LOGE("L2init: alloc failed!\n"); return; }
    g_neopixel->begin();
    g_neopixel->clear();
    g_light2Gpio = gpio;
    g_light2Count = count;
    LOGI("Light2 strip init: gpio=%d leds=%d\n", gpio, count);
  }
}

bool light2ApplyConfig(const AppConfig &cfg) {
  if (!light2IsValid(cfg)) {
    if (g_neopixel) {
      g_neopixel->clear();
      g_neopixel->show();
      delete g_neopixel;
      g_neopixel = nullptr;
    }
    g_light2Gpio = -1;
    g_light2Count = 0;
    return true;
  }

  light2InitStrip(cfg);

  if (cfg.light2.capture_only) {
    g_neopixel->clear();
    g_neopixel->show();
  } else {
    light2On(cfg);
  }

  LOGI("Light2 config applied: enabled=%d gpio=%d leds=%d brightness=%d settle_ms=%d capture_only=%d\n",
       cfg.light2.enabled, cfg.light2.gpio, cfg.light2.led_count,
       cfg.light2.brightness, cfg.light2.settle_ms, cfg.light2.capture_only);
  return true;
}

void light2On(const AppConfig &cfg) {
  if (!light2IsValid(cfg)) return;
  if (!g_neopixel || g_light2Gpio != cfg.light2.gpio || g_light2Count != cfg.light2.led_count) {
    light2InitStrip(cfg);
  }

  int r = (cfg.light2.color_r * cfg.light2.brightness) / 255;
  int g = (cfg.light2.color_g * cfg.light2.brightness) / 255;
  int b = (cfg.light2.color_b * cfg.light2.brightness) / 255;
  g_neopixel->fill(Adafruit_NeoPixel::Color(r, g, b));
  g_neopixel->show();
  g_light2CaptureLightOn = true;

  LOGI("Light2 ON: gpio=%d leds=%d r=%d g=%d b=%d\n",
       cfg.light2.gpio, cfg.light2.led_count, r, g, b);

  if (cfg.light2.settle_ms > 0) delay(cfg.light2.settle_ms);
}

void light2Off(const AppConfig &cfg) {
  (void)cfg;
  if (!g_neopixel) return;
  g_neopixel->clear();
  g_neopixel->show();
  g_light2CaptureLightOn = false;
  g_light2PreviewOn = false;
  LOGI("Light2 OFF: gpio=%d\n", g_light2Gpio);
}

void light2TestPulse(const AppConfig &cfg, unsigned long ms) {
  if (!light2IsValid(cfg)) return;
  light2On(cfg);
  delay(ms);
  light2Off(cfg);
}

// Turns on whichever channels are configured for capture-only mode.
void lightsOnForCapture(const AppConfig &cfg) {
  if (lightIsValid(cfg) && cfg.light.capture_only) lightOn(cfg);
  if (light2IsValid(cfg) && cfg.light2.capture_only) light2On(cfg);
}

// =============================================================================

void preCaptureTrigger(const AppConfig &cfg) {
  if (!cfg.pre_capture.enabled || cfg.pre_capture.delay_ms < 0) {
    return;
  }

  extern void mqttPublishPreCapture(const AppConfig &cfg, bool state);
  
  LOGI("Pre-capture trigger: publishing state=ON with delay=%d ms\n", cfg.pre_capture.delay_ms);
  
  mqttPublishPreCapture(cfg, true);
  
  if (cfg.pre_capture.delay_ms > 0) {
    delay(cfg.pre_capture.delay_ms);
  }
}