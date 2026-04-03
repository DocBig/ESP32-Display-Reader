#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct WifiConfig {
  String ssid;
  String password;
  String hostname;
};

struct DeviceConfig {
  String id;
  String name;
};

struct CameraConfig {
  String model;
  int width;
  int height;
  int jpeg_quality;

  bool auto_exposure;
  bool auto_gain;
  bool auto_whitebalance;

  int aec_value;
  int agc_gain;

  bool vflip;
  bool hflip;
};

struct LightConfig {
  bool enabled;
  int gpio;
  int brightness;
  int settle_ms;
  bool capture_only;
  bool invert;
};

struct Light2Config {
  bool enabled;
  int gpio;
  int led_count;
  int brightness;
  int color_r;
  int color_g;
  int color_b;
  int settle_ms;
  bool capture_only;
};

struct PreCaptureConfig {
  bool enabled;
  int delay_ms;
  String mqtt_topic;
};
struct DiehlConfig {
  bool enabled;

  int trigger_gpio;
  int trigger_pulse_ms;
  int trigger_settle_ms;
  bool trigger_invert;

  int max_steps;
  int timeout_ms;

  String page_roi_id;
  String value_roi_id;

  String error_type_roi_id;
  String error_code_roi_id;

  int error_sample_time_ms;
  int error_sample_interval_ms;
};
struct MqttConfig {
  bool enabled;
  bool image_enabled;
  String host;
  uint16_t port;
  String topic;
  String username;
  String password;
};

struct HomeAssistantConfig {
  String discovery_prefix;
};

struct StreamConfig {
  bool enabled;
};

struct SegSample {
  float rx, ry, rw, rh;
};

struct SevenSegProfile {
  String name;
  SegSample segs[7]; // order: a, b, c, d, e, f, g
};

struct Roi {
  String id;
  String label;
  String type;

  int x;
  int y;
  int w;
  int h;

  int threshold;
  int digit_gap_px;
  int digits;
  int decimal_places;
  std::vector<int> gaps; // per-gap spacing; empty = uniform (digit_gap_px for all)

  int threshold_on;
  int threshold_off;

  bool last_state;
  bool invert_logic;
  bool auto_threshold;
  bool stretch_contrast;
  int confidence_margin;

  bool ha_enabled;
  String ha_name;
  String ha_unit;
  String ha_icon;
  String ha_device_class;
  String ha_state_class;

  String seg_profile; // references SevenSegProfile.name; default "standard"
};

struct AppConfig {
  WifiConfig wifi;
  DeviceConfig device;
  CameraConfig camera;
  LightConfig light;
  Light2Config light2;
  PreCaptureConfig pre_capture;
  DiehlConfig diehl;
  MqttConfig mqtt;
  HomeAssistantConfig ha;
  StreamConfig stream;

  int capture_interval_sec;
  int debug_level;

  std::vector<Roi> rois;
  std::vector<SevenSegProfile> seg_profiles;
};

void loadDefaultConfig(AppConfig &cfg);
String jsonStringOrDefault(JsonVariantConst v, const char *def);
String configToJson(const AppConfig &cfg);
bool configFromJson(const String &json, AppConfig &cfg);
