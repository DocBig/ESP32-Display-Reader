#include "app_config.h"
#include "device_identity.h"

struct CameraFrameOption {
  int w;
  int h;
};

static const CameraFrameOption kCameraFrameOptions[] = {
  {160, 120},
  {320, 240},
  {640, 480},
  {800, 600},
};

static void normalizeCameraFrame(CameraConfig &camera) {
  int bestIdx = 0;
  long bestScore = 0x7fffffffL;

  for (size_t i = 0; i < sizeof(kCameraFrameOptions) / sizeof(kCameraFrameOptions[0]); i++) {
    long dw = (long)camera.width - (long)kCameraFrameOptions[i].w;
    long dh = (long)camera.height - (long)kCameraFrameOptions[i].h;
    long score = dw * dw + dh * dh;

    if (score < bestScore) {
      bestScore = score;
      bestIdx = (int)i;
    }
  }

  camera.width = kCameraFrameOptions[bestIdx].w;
  camera.height = kCameraFrameOptions[bestIdx].h;
}

void loadDefaultConfig(AppConfig &cfg) {

  /* Segment Profiles */

  static const SegSample kStandardSegs[7] = {
    {0.25f, 0.05f, 0.50f, 0.08f}, // a top
    {0.78f, 0.18f, 0.10f, 0.26f}, // b top-right
    {0.78f, 0.56f, 0.10f, 0.26f}, // c bottom-right
    {0.25f, 0.88f, 0.50f, 0.08f}, // d bottom
    {0.12f, 0.56f, 0.10f, 0.26f}, // e bottom-left
    {0.12f, 0.18f, 0.10f, 0.26f}, // f top-left
    {0.25f, 0.46f, 0.50f, 0.08f}, // g middle
  };

  static const SegSample kItalicSegs[7] = {
    {0.32f, 0.05f, 0.50f, 0.08f}, // a top – nach rechts verschoben
    {0.81f, 0.18f, 0.10f, 0.26f}, // b top-right – leicht rechts
    {0.75f, 0.56f, 0.10f, 0.26f}, // c bottom-right – leicht links
    {0.19f, 0.88f, 0.50f, 0.08f}, // d bottom – nach links verschoben
    {0.09f, 0.56f, 0.10f, 0.26f}, // e bottom-left – leicht links
    {0.15f, 0.18f, 0.10f, 0.26f}, // f top-left – leicht rechts
    {0.26f, 0.46f, 0.50f, 0.08f}, // g middle – kaum Versatz
  };

  cfg.seg_profiles.clear();

  SevenSegProfile pStd;
  pStd.name = "standard";
  memcpy(pStd.segs, kStandardSegs, sizeof(kStandardSegs));
  cfg.seg_profiles.push_back(pStd);

  SevenSegProfile pItalic;
  pItalic.name = "italic";
  memcpy(pItalic.segs, kItalicSegs, sizeof(kItalicSegs));
  cfg.seg_profiles.push_back(pItalic);

  SevenSegProfile pCustom;
  pCustom.name = "custom";
  memcpy(pCustom.segs, kStandardSegs, sizeof(kStandardSegs));
  cfg.seg_profiles.push_back(pCustom);

  /* Device */

  cfg.device.id = defaultDeviceId();
  cfg.device.name = defaultDeviceName();

  /* WiFi */

  cfg.wifi.ssid = "";
  cfg.wifi.password = "";
  cfg.wifi.hostname = defaultHostname();

  /* Camera */

  cfg.camera.model = "ai_thinker";
  cfg.camera.width = 320;
  cfg.camera.height = 240;
  cfg.camera.jpeg_quality = 12;

  cfg.camera.auto_exposure = true;
  cfg.camera.auto_gain = true;
  cfg.camera.auto_whitebalance = true;

  cfg.camera.aec_value = 300;
  cfg.camera.agc_gain = 0;
  cfg.camera.vflip = false;
  cfg.camera.hflip = false;
  normalizeCameraFrame(cfg.camera);

  /* Light */

  cfg.light.enabled = false;
  cfg.light.gpio = -1;
  cfg.light.brightness = 255;
  cfg.light.settle_ms = 250;
  cfg.light.capture_only = true;
  cfg.light.invert = false;

  /* Light2 (WS2812B) */

  cfg.light2.enabled = false;
  cfg.light2.gpio = -1;
  cfg.light2.led_count = 16;
  cfg.light2.brightness = 200;
  cfg.light2.color_r = 255;
  cfg.light2.color_g = 255;
  cfg.light2.color_b = 255;
  cfg.light2.settle_ms = 250;
  cfg.light2.capture_only = true;

  /* Pre-Capture */

  cfg.pre_capture.enabled = false;
  cfg.pre_capture.delay_ms = 500;
  cfg.pre_capture.mqtt_topic = "pre_capture";

  /* Diehl */

  cfg.diehl.enabled = false;

  cfg.diehl.trigger_gpio = -1;
  cfg.diehl.trigger_pulse_ms = 300;
  cfg.diehl.trigger_settle_ms = 1200;
  cfg.diehl.trigger_invert = false;

  cfg.diehl.max_steps = 10;
  cfg.diehl.timeout_ms = 15000;

  cfg.diehl.page_roi_id = "diehl_page";
  cfg.diehl.value_roi_id = "diehl_value";

  cfg.diehl.error_type_roi_id = "diehl_error_type";
  cfg.diehl.error_code_roi_id = "diehl_error_code";

  cfg.diehl.error_sample_time_ms = 5000;
  cfg.diehl.error_sample_interval_ms = 500;

  /* Timing */

  cfg.capture_interval_sec = 60;

  /* Debug */

  cfg.debug_level = 1;

  /* MQTT */

  cfg.mqtt.enabled = false;
  cfg.mqtt.image_enabled = false;
  cfg.mqtt.host = "192.168.1.10";
  cfg.mqtt.port = 1883;
  cfg.mqtt.topic = defaultMqttTopicBase() + "/state";
  cfg.mqtt.username = "";
  cfg.mqtt.password = "";

  /* Home Assistant */

  cfg.ha.discovery_prefix = "homeassistant";

  /* Stream */

  cfg.stream.enabled = true;

  /* ROIs */

  cfg.rois.clear();

  cfg.rois.push_back({
    "display_value",
    "Display Value",
    "sevenseg",
    140, 70, 170, 70,
    105,
    8,
    4,
    0,
    {},   // gaps (empty = uniform)
    125,
    105,
    false,
    false,
    false,
    false,
    0,
    true,
    "",
    "",
    "",
    "",
    "",
    "standard"
  });

  cfg.rois.push_back({
    "symbol_1",
    "Symbol Example",
    "symbol",
    80, 180, 40, 40,
    105,
    8,
    2,
    0,
    {},   // gaps (empty = uniform)
    125,
    105,
    false,
    false,
    false,
    false,
    0,
    true,
    "",
    "",
    "",
    "",
    "",
    "standard"
  });
}

String jsonStringOrDefault(JsonVariantConst v, const char *def) {
  if (v.is<const char*>()) {
    const char *p = v.as<const char*>();
    if (p) return String(p);
  }
  return String(def);
}

static void roiToJson(JsonObject o, const Roi &r) {
  o["id"] = r.id;
  o["label"] = r.label;
  o["type"] = r.type;
  o["x"] = r.x;
  o["y"] = r.y;
  o["w"] = r.w;
  o["h"] = r.h;
  o["threshold"] = r.threshold;
  o["digit_gap_px"] = r.digit_gap_px;
  if (!r.gaps.empty()) {
    JsonArray ga = o["gaps"].to<JsonArray>();
    for (int g : r.gaps) ga.add(g);
  }
  o["digits"] = r.digits;
  o["decimal_places"] = r.decimal_places;
  o["threshold_on"] = r.threshold_on;
  o["threshold_off"] = r.threshold_off;
  o["invert_logic"] = r.invert_logic;
  o["auto_threshold"] = r.auto_threshold;
  o["stretch_contrast"] = r.stretch_contrast;
  o["confidence_margin"] = r.confidence_margin;
  o["ha_enabled"] = r.ha_enabled;
  o["ha_name"] = r.ha_name;
  o["ha_unit"] = r.ha_unit;
  o["ha_icon"] = r.ha_icon;
  o["ha_device_class"] = r.ha_device_class;
  o["ha_state_class"] = r.ha_state_class;
  o["seg_profile"] = r.seg_profile;
}

String configToJson(const AppConfig &cfg) {

  JsonDocument doc;

  /* Device */

  doc["device"]["id"] = cfg.device.id;
  doc["device"]["name"] = cfg.device.name;

  /* WiFi */

  doc["wifi"]["ssid"] = cfg.wifi.ssid;
  doc["wifi"]["password"] = cfg.wifi.password;
  doc["wifi"]["hostname"] = cfg.wifi.hostname;

  /* Camera */

  doc["camera"]["model"] = cfg.camera.model;
  doc["camera"]["width"] = cfg.camera.width;
  doc["camera"]["height"] = cfg.camera.height;
  doc["camera"]["jpeg_quality"] = cfg.camera.jpeg_quality;
  doc["camera"]["auto_exposure"] = cfg.camera.auto_exposure;
  doc["camera"]["auto_gain"] = cfg.camera.auto_gain;
  doc["camera"]["auto_whitebalance"] = cfg.camera.auto_whitebalance;
  doc["camera"]["aec_value"] = cfg.camera.aec_value;
  doc["camera"]["agc_gain"] = cfg.camera.agc_gain;
  doc["camera"]["vflip"] = cfg.camera.vflip;
  doc["camera"]["hflip"] = cfg.camera.hflip;

  /* Light */

  doc["light"]["enabled"] = cfg.light.enabled;
  doc["light"]["gpio"] = cfg.light.gpio;
  doc["light"]["brightness"] = cfg.light.brightness;
  doc["light"]["settle_ms"] = cfg.light.settle_ms;
  doc["light"]["capture_only"] = cfg.light.capture_only;
  doc["light"]["invert"] = cfg.light.invert;

  /* Light2 (WS2812B) */

  doc["light2"]["enabled"] = cfg.light2.enabled;
  doc["light2"]["gpio"] = cfg.light2.gpio;
  doc["light2"]["led_count"] = cfg.light2.led_count;
  doc["light2"]["brightness"] = cfg.light2.brightness;
  doc["light2"]["color_r"] = cfg.light2.color_r;
  doc["light2"]["color_g"] = cfg.light2.color_g;
  doc["light2"]["color_b"] = cfg.light2.color_b;
  doc["light2"]["settle_ms"] = cfg.light2.settle_ms;
  doc["light2"]["capture_only"] = cfg.light2.capture_only;

  /* Pre-Capture */

  doc["pre_capture"]["enabled"] = cfg.pre_capture.enabled;
  doc["pre_capture"]["delay_ms"] = cfg.pre_capture.delay_ms;
  doc["pre_capture"]["mqtt_topic"] = cfg.pre_capture.mqtt_topic;

  /* Diehl */

  doc["diehl"]["enabled"] = cfg.diehl.enabled;
  doc["diehl"]["trigger_gpio"] = cfg.diehl.trigger_gpio;
  doc["diehl"]["trigger_pulse_ms"] = cfg.diehl.trigger_pulse_ms;
  doc["diehl"]["trigger_settle_ms"] = cfg.diehl.trigger_settle_ms;
  doc["diehl"]["trigger_invert"] = cfg.diehl.trigger_invert;

  doc["diehl"]["max_steps"] = cfg.diehl.max_steps;
  doc["diehl"]["timeout_ms"] = cfg.diehl.timeout_ms;

  doc["diehl"]["page_roi_id"] = cfg.diehl.page_roi_id;
  doc["diehl"]["value_roi_id"] = cfg.diehl.value_roi_id;

  doc["diehl"]["error_type_roi_id"] = cfg.diehl.error_type_roi_id;
  doc["diehl"]["error_code_roi_id"] = cfg.diehl.error_code_roi_id;

  doc["diehl"]["error_sample_time_ms"] = cfg.diehl.error_sample_time_ms;
  doc["diehl"]["error_sample_interval_ms"] = cfg.diehl.error_sample_interval_ms;

  /* Timing */

  doc["timing"]["capture_interval_sec"] = cfg.capture_interval_sec;

  /* Debug */

  doc["debug"]["level"] = cfg.debug_level;

  /* MQTT */

  doc["mqtt"]["enabled"] = cfg.mqtt.enabled;
  doc["mqtt"]["image_enabled"] = cfg.mqtt.image_enabled;
  doc["mqtt"]["host"] = cfg.mqtt.host;
  doc["mqtt"]["port"] = cfg.mqtt.port;
  doc["mqtt"]["topic"] = cfg.mqtt.topic;
  doc["mqtt"]["username"] = cfg.mqtt.username;
  doc["mqtt"]["password"] = cfg.mqtt.password;

  /* Home Assistant */

  doc["ha"]["discovery_prefix"] = cfg.ha.discovery_prefix;

  /* Stream */

  doc["stream"]["enabled"] = cfg.stream.enabled;

  /* Segment Profiles */

  {
    JsonArray profArr = doc["seg_profiles"].to<JsonArray>();
    for (const auto &p : cfg.seg_profiles) {
      JsonObject po = profArr.add<JsonObject>();
      po["name"] = p.name;
      JsonArray segsArr = po["segs"].to<JsonArray>();
      for (int i = 0; i < 7; i++) {
        JsonArray seg = segsArr.add<JsonArray>();
        seg.add(p.segs[i].rx);
        seg.add(p.segs[i].ry);
        seg.add(p.segs[i].rw);
        seg.add(p.segs[i].rh);
      }
    }
  }

  /* ROIs */

  JsonArray arr = doc["rois"].to<JsonArray>();

  for (const auto &r : cfg.rois) {
    JsonObject o = arr.add<JsonObject>();
    roiToJson(o, r);
  }

  String out;
  serializeJson(doc, out);
  return out;
}

bool configFromJson(const String &json, AppConfig &cfg) {

  JsonDocument doc;

  if (deserializeJson(doc, json)) {
    return false;
  }

  AppConfig next = cfg;

  /* Device */

  next.device.id = jsonStringOrDefault(doc["device"]["id"], cfg.device.id.c_str());
  next.device.name = jsonStringOrDefault(doc["device"]["name"], cfg.device.name.c_str());

  /* WiFi */

  next.wifi.ssid = jsonStringOrDefault(doc["wifi"]["ssid"], cfg.wifi.ssid.c_str());
  next.wifi.password = jsonStringOrDefault(doc["wifi"]["password"], cfg.wifi.password.c_str());
  next.wifi.hostname = jsonStringOrDefault(doc["wifi"]["hostname"], cfg.wifi.hostname.c_str());

  /* Camera */

  next.camera.model = jsonStringOrDefault(doc["camera"]["model"], cfg.camera.model.c_str());
  next.camera.width = doc["camera"]["width"] | cfg.camera.width;
  next.camera.height = doc["camera"]["height"] | cfg.camera.height;
  next.camera.jpeg_quality = doc["camera"]["jpeg_quality"] | cfg.camera.jpeg_quality;
  next.camera.auto_exposure = doc["camera"]["auto_exposure"] | cfg.camera.auto_exposure;
  next.camera.auto_gain = doc["camera"]["auto_gain"] | cfg.camera.auto_gain;
  next.camera.auto_whitebalance = doc["camera"]["auto_whitebalance"] | cfg.camera.auto_whitebalance;
  next.camera.aec_value = doc["camera"]["aec_value"] | cfg.camera.aec_value;
  next.camera.agc_gain = doc["camera"]["agc_gain"] | cfg.camera.agc_gain;
  next.camera.vflip = doc["camera"]["vflip"] | false;
  next.camera.hflip = doc["camera"]["hflip"] | false;
  normalizeCameraFrame(next.camera);

  /* Light */

  next.light.enabled = doc["light"]["enabled"] | cfg.light.enabled;
  next.light.gpio = doc["light"]["gpio"] | cfg.light.gpio;
  next.light.brightness = doc["light"]["brightness"] | cfg.light.brightness;
  next.light.settle_ms = doc["light"]["settle_ms"] | cfg.light.settle_ms;
  next.light.capture_only = doc["light"]["capture_only"] | cfg.light.capture_only;
  next.light.invert = doc["light"]["invert"] | cfg.light.invert;

  if (next.light.brightness < 0) next.light.brightness = 0;
  if (next.light.brightness > 255) next.light.brightness = 255;
  if (next.light.settle_ms < 0) next.light.settle_ms = 0;
  if (next.light.settle_ms > 3000) next.light.settle_ms = 3000;

  /* Light2 (WS2812B) */

  next.light2.enabled = doc["light2"]["enabled"] | cfg.light2.enabled;
  next.light2.gpio = doc["light2"]["gpio"] | cfg.light2.gpio;
  next.light2.led_count = doc["light2"]["led_count"] | cfg.light2.led_count;
  next.light2.brightness = doc["light2"]["brightness"] | cfg.light2.brightness;
  next.light2.color_r = doc["light2"]["color_r"] | cfg.light2.color_r;
  next.light2.color_g = doc["light2"]["color_g"] | cfg.light2.color_g;
  next.light2.color_b = doc["light2"]["color_b"] | cfg.light2.color_b;
  next.light2.settle_ms = doc["light2"]["settle_ms"] | cfg.light2.settle_ms;
  next.light2.capture_only = doc["light2"]["capture_only"] | cfg.light2.capture_only;

  if (next.light2.led_count < 1) next.light2.led_count = 1;
  if (next.light2.led_count > 64) next.light2.led_count = 64;
  if (next.light2.brightness < 0) next.light2.brightness = 0;
  if (next.light2.brightness > 255) next.light2.brightness = 255;
  if (next.light2.color_r < 0) next.light2.color_r = 0;
  if (next.light2.color_r > 255) next.light2.color_r = 255;
  if (next.light2.color_g < 0) next.light2.color_g = 0;
  if (next.light2.color_g > 255) next.light2.color_g = 255;
  if (next.light2.color_b < 0) next.light2.color_b = 0;
  if (next.light2.color_b > 255) next.light2.color_b = 255;
  if (next.light2.settle_ms < 0) next.light2.settle_ms = 0;
  if (next.light2.settle_ms > 3000) next.light2.settle_ms = 3000;

  /* Pre-Capture */

  next.pre_capture.enabled = doc["pre_capture"]["enabled"] | cfg.pre_capture.enabled;
  next.pre_capture.delay_ms = doc["pre_capture"]["delay_ms"] | cfg.pre_capture.delay_ms;
  next.pre_capture.mqtt_topic = jsonStringOrDefault(doc["pre_capture"]["mqtt_topic"], cfg.pre_capture.mqtt_topic.c_str());

  if (next.pre_capture.delay_ms < 0) next.pre_capture.delay_ms = 0;
  if (next.pre_capture.delay_ms > 30000) next.pre_capture.delay_ms = 30000;
  if (next.pre_capture.mqtt_topic.isEmpty()) next.pre_capture.mqtt_topic = "pre_capture";

  /* Diehl */

  next.diehl.enabled = doc["diehl"]["enabled"] | cfg.diehl.enabled;

  next.diehl.trigger_gpio = doc["diehl"]["trigger_gpio"] | cfg.diehl.trigger_gpio;
  next.diehl.trigger_pulse_ms = doc["diehl"]["trigger_pulse_ms"] | cfg.diehl.trigger_pulse_ms;
  next.diehl.trigger_settle_ms = doc["diehl"]["trigger_settle_ms"] | cfg.diehl.trigger_settle_ms;
  next.diehl.trigger_invert = doc["diehl"]["trigger_invert"] | cfg.diehl.trigger_invert;

  next.diehl.max_steps = doc["diehl"]["max_steps"] | cfg.diehl.max_steps;
  next.diehl.timeout_ms = doc["diehl"]["timeout_ms"] | cfg.diehl.timeout_ms;

  next.diehl.page_roi_id =
      jsonStringOrDefault(doc["diehl"]["page_roi_id"], cfg.diehl.page_roi_id.c_str());
  next.diehl.value_roi_id =
      jsonStringOrDefault(doc["diehl"]["value_roi_id"], cfg.diehl.value_roi_id.c_str());

  next.diehl.error_type_roi_id =
      jsonStringOrDefault(doc["diehl"]["error_type_roi_id"], cfg.diehl.error_type_roi_id.c_str());
  next.diehl.error_code_roi_id =
      jsonStringOrDefault(doc["diehl"]["error_code_roi_id"], cfg.diehl.error_code_roi_id.c_str());

  next.diehl.error_sample_time_ms =
      doc["diehl"]["error_sample_time_ms"] | cfg.diehl.error_sample_time_ms;
  next.diehl.error_sample_interval_ms =
      doc["diehl"]["error_sample_interval_ms"] | cfg.diehl.error_sample_interval_ms;

  if (next.diehl.trigger_pulse_ms < 10) next.diehl.trigger_pulse_ms = 10;
  if (next.diehl.trigger_pulse_ms > 5000) next.diehl.trigger_pulse_ms = 5000;

  if (next.diehl.trigger_settle_ms < 0) next.diehl.trigger_settle_ms = 0;
  if (next.diehl.trigger_settle_ms > 10000) next.diehl.trigger_settle_ms = 10000;

  if (next.diehl.max_steps < 1) next.diehl.max_steps = 1;
  if (next.diehl.max_steps > 100) next.diehl.max_steps = 100;

  if (next.diehl.timeout_ms < 1000) next.diehl.timeout_ms = 1000;
  if (next.diehl.timeout_ms > 120000) next.diehl.timeout_ms = 120000;

  if (next.diehl.error_sample_time_ms < 500) next.diehl.error_sample_time_ms = 500;
  if (next.diehl.error_sample_time_ms > 30000) next.diehl.error_sample_time_ms = 30000;

  if (next.diehl.error_sample_interval_ms < 100) next.diehl.error_sample_interval_ms = 100;
  if (next.diehl.error_sample_interval_ms > 5000) next.diehl.error_sample_interval_ms = 5000;

  if (next.diehl.trigger_gpio < -1) next.diehl.trigger_gpio = -1;
  if (next.diehl.trigger_gpio > 39) next.diehl.trigger_gpio = -1;

  if (next.diehl.page_roi_id.isEmpty()) next.diehl.page_roi_id = "diehl_page";
  if (next.diehl.value_roi_id.isEmpty()) next.diehl.value_roi_id = "diehl_value";
  if (next.diehl.error_type_roi_id.isEmpty()) next.diehl.error_type_roi_id = "diehl_error_type";
  if (next.diehl.error_code_roi_id.isEmpty()) next.diehl.error_code_roi_id = "diehl_error_code";
  /* Timing */

  next.capture_interval_sec = doc["timing"]["capture_interval_sec"] | cfg.capture_interval_sec;

  /* Debug */

  next.debug_level = doc["debug"]["level"] | cfg.debug_level;

  /* MQTT */

  next.mqtt.enabled = doc["mqtt"]["enabled"] | cfg.mqtt.enabled;
  next.mqtt.image_enabled = doc["mqtt"]["image_enabled"] | cfg.mqtt.image_enabled;
  next.mqtt.host = jsonStringOrDefault(doc["mqtt"]["host"], cfg.mqtt.host.c_str());
  next.mqtt.port = doc["mqtt"]["port"] | cfg.mqtt.port;
  next.mqtt.topic = jsonStringOrDefault(doc["mqtt"]["topic"], cfg.mqtt.topic.c_str());
  next.mqtt.username = jsonStringOrDefault(doc["mqtt"]["username"], cfg.mqtt.username.c_str());
  next.mqtt.password = jsonStringOrDefault(doc["mqtt"]["password"], cfg.mqtt.password.c_str());

  /* Home Assistant */

  next.ha.discovery_prefix =
      jsonStringOrDefault(doc["ha"]["discovery_prefix"], cfg.ha.discovery_prefix.c_str());

  /* Stream */

  next.stream.enabled = doc["stream"]["enabled"] | cfg.stream.enabled;

  /* Segment Profiles */

  if (doc["seg_profiles"].is<JsonArray>()) {
    next.seg_profiles.clear();
    for (JsonObject po : doc["seg_profiles"].as<JsonArray>()) {
      SevenSegProfile p;
      p.name = jsonStringOrDefault(po["name"], "");
      if (p.name.isEmpty()) continue;

      // "standard" is always read-only – skip overwriting it from JSON,
      // the hardcoded defaults from loadDefaultConfig are kept.
      if (p.name == "standard") {
        // Still push the existing standard profile from next (already set via loadDefaultConfig path or cfg)
        bool found = false;
        for (auto &ep : cfg.seg_profiles) {
          if (ep.name == "standard") { next.seg_profiles.push_back(ep); found = true; break; }
        }
        if (!found && !next.seg_profiles.empty() && next.seg_profiles[0].name == "standard") {
          next.seg_profiles.push_back(next.seg_profiles[0]);
        }
        continue;
      }

      if (po["segs"].is<JsonArray>()) {
        int idx = 0;
        for (JsonArray seg : po["segs"].as<JsonArray>()) {
          if (idx >= 7) break;
          p.segs[idx].rx = seg[0] | 0.0f;
          p.segs[idx].ry = seg[1] | 0.0f;
          p.segs[idx].rw = seg[2] | 0.1f;
          p.segs[idx].rh = seg[3] | 0.1f;
          idx++;
        }
      }
      next.seg_profiles.push_back(p);
    }
  }

  // Ensure "standard" is always present as first profile (fallback).
  bool hasStandard = false;
  for (const auto &p : next.seg_profiles) {
    if (p.name == "standard") { hasStandard = true; break; }
  }
  if (!hasStandard) {
    // Re-insert from original cfg or from hardcoded defaults
    bool found = false;
    for (const auto &p : cfg.seg_profiles) {
      if (p.name == "standard") { next.seg_profiles.insert(next.seg_profiles.begin(), p); found = true; break; }
    }
    if (!found) {
      // Last resort: use hardcoded standard values
      static const SegSample kStd[7] = {
        {0.25f,0.05f,0.50f,0.08f},{0.78f,0.18f,0.10f,0.26f},{0.78f,0.56f,0.10f,0.26f},
        {0.25f,0.88f,0.50f,0.08f},{0.12f,0.56f,0.10f,0.26f},{0.12f,0.18f,0.10f,0.26f},
        {0.25f,0.46f,0.50f,0.08f}
      };
      SevenSegProfile std;
      std.name = "standard";
      memcpy(std.segs, kStd, sizeof(kStd));
      next.seg_profiles.insert(next.seg_profiles.begin(), std);
    }
  }

  /* ROIs */

  if (doc["rois"].is<JsonArray>()) {

    next.rois.clear();

    for (JsonObject o : doc["rois"].as<JsonArray>()) {

      Roi r;

      r.id = jsonStringOrDefault(o["id"], "");
      r.label = jsonStringOrDefault(o["label"], "");
      r.type = jsonStringOrDefault(o["type"], "sevenseg");

      r.x = o["x"] | 0;
      r.y = o["y"] | 0;
      r.w = o["w"] | 1;
      r.h = o["h"] | 1;

      r.threshold = o["threshold"] | 105;
      r.digit_gap_px = o["digit_gap_px"] | 8;
      r.digits = o["digits"] | 2;
      r.decimal_places = o["decimal_places"] | 0;

      r.gaps.clear();
      if (o["gaps"].is<JsonArray>()) {
        for (int g : o["gaps"].as<JsonArray>()) r.gaps.push_back(g);
      }

      r.threshold_on = o["threshold_on"] | 125;
      r.threshold_off = o["threshold_off"] | 105;

      r.last_state = false;
      r.invert_logic = o["invert_logic"] | false;
      r.auto_threshold = o["auto_threshold"] | false;
      r.stretch_contrast = o["stretch_contrast"] | false;
      r.confidence_margin = o["confidence_margin"] | 0;

      r.ha_enabled = o["ha_enabled"] | true;
      r.ha_name = jsonStringOrDefault(o["ha_name"], "");
      r.ha_unit = jsonStringOrDefault(o["ha_unit"], "");
      r.ha_icon = jsonStringOrDefault(o["ha_icon"], "");
      r.ha_device_class = jsonStringOrDefault(o["ha_device_class"], "");
      r.ha_state_class = jsonStringOrDefault(o["ha_state_class"], "");

      r.seg_profile = jsonStringOrDefault(o["seg_profile"], "standard");

      if (r.digits < 1) r.digits = 1;
      if (r.digits > 10) r.digits = 10;
      if (r.decimal_places < 0) r.decimal_places = 0;
      if (r.decimal_places >= r.digits) r.decimal_places = r.digits - 1;

      next.rois.push_back(r);
    }
  }

  // Gerätespezifische Felder werden NICHT aus dem Import übernommen.
  // Sie basieren auf der persistent UID des konkreten Geräts, damit beim
  // Kopieren einer Config zwischen zwei Geräten keine MQTT-Topic-Kollision
  // und keine HA-Discovery-Kollision entsteht.
  next.device.id   = defaultDeviceId();
  next.wifi.hostname = defaultHostname();
  next.mqtt.topic  = defaultMqttTopicBase() + "/state";

  cfg = next;
  return true;
}
