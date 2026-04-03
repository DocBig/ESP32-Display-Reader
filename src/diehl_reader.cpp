#include "diehl_reader.h"
#include "analyzer.h"
#include "debug_log.h"

DiehlReader::DiehlReader(AppConfig &cfg) : cfg(cfg) {}

bool DiehlReader::run(DiehlResult &result) {
  result.pages.clear();
  result.errors.clear();
  result.success = false;

  unsigned long t0 = millis();
  int steps = 0;

  while (steps < cfg.diehl.max_steps) {
    if (millis() - t0 > (unsigned long)cfg.diehl.timeout_ms) {
      LOGE("Diehl timeout reached\n");
      return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      LOGE("Diehl: failed to get camera frame\n");
      steps++;
      delay(100);
      continue;
    }

    int page = -1;
    bool pageOk = readPage(fb, page);

    if (!pageOk) {
      esp_camera_fb_return(fb);
      steps++;
      continue;
    }

    LOGD("Diehl page detected: %d\n", page);

    // Seiten 01–04
    if (page >= 1 && page <= 4) {
      if (result.pages.count(page) == 0) {
        String value;
        if (readValue(fb, value)) {
          result.pages[page] = value;
          LOGD("Diehl page %02d value=%s\n", page, value.c_str());
        } else {
          LOGD("Diehl page %02d value read failed\n", page);
        }
      }

      esp_camera_fb_return(fb);

      if (result.pages.size() >= 4) {
        result.success = true;
        return true;
      }

      triggerNextPage();
      delay(cfg.diehl.trigger_settle_ms);
      steps++;
      continue;
    }

    esp_camera_fb_return(fb);

    // Seite 05 → Fehlerseite samplen
    if (page == 5) {
      readErrorPage(result);
      result.success = true;
      return true;
    }

    // Unbekannte Seite → weiter schalten
    triggerNextPage();
    delay(cfg.diehl.trigger_settle_ms);
    steps++;
  }

  return false;
}

// ------------------------------------------------------------

bool DiehlReader::readPage(camera_fb_t *fb, int &page) {
  return readSevenSegIntById(fb, cfg, cfg.diehl.page_roi_id, page);
}

bool DiehlReader::readValue(camera_fb_t *fb, String &value) {
  return readSevenSegStringById(fb, cfg, cfg.diehl.value_roi_id, value);
}

// ------------------------------------------------------------

void DiehlReader::triggerNextPage() {
  setTrigger(true);
  delay(cfg.diehl.trigger_pulse_ms);
  setTrigger(false);
}

void DiehlReader::setTrigger(bool active) {
  bool level = cfg.diehl.trigger_invert ? !active : active;
  digitalWrite(cfg.diehl.trigger_gpio, level ? HIGH : LOW);
}

// ------------------------------------------------------------

void DiehlReader::readErrorPage(DiehlResult &result) {
  unsigned long t0 = millis();

  while (millis() - t0 < (unsigned long)cfg.diehl.error_sample_time_ms) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      LOGE("Diehl error sampling: failed to get frame\n");
      delay(cfg.diehl.error_sample_interval_ms);
      continue;
    }

    char type = '\0';
    String code;

    bool okType = readSevenSegCharById(fb, cfg, cfg.diehl.error_type_roi_id, type);
    bool okCode = readSevenSegStringById(fb, cfg, cfg.diehl.error_code_roi_id, code);

    esp_camera_fb_return(fb);

    if (okType && okCode && code.length() >= 2) {
      int num = code.toInt();

      if (num > 0) {
        result.errors[num].insert(type);
        LOGD("Diehl error sampled: %c ... %s\n", type, code.c_str());
      }
    }

    delay(cfg.diehl.error_sample_interval_ms);
  }
}