#include "mqtt_ha_discovery.h"

#include <ArduinoJson.h>
#include <ctype.h>

#include "debug_log.h"
#include "device_identity.h"

static String sanitizeId(const String &in) {
  String out;
  out.reserve(in.length());

  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (isalnum((unsigned char)c)) {
      out += (char)tolower((unsigned char)c);
    } else {
      out += '_';
    }
  }

  while (out.indexOf("__") >= 0) {
    out.replace("__", "_");
  }

  return out;
}

static String toLowerCopy(const String &in) {
  String out = in;
  out.toLowerCase();
  return out;
}

static bool containsAny(const String &text, const char *const patterns[], size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (text.indexOf(patterns[i]) >= 0) {
      return true;
    }
  }
  return false;
}

static String roiSearchText(const Roi &roi) {
  return toLowerCopy(roi.id + " " + roi.label);
}

static bool isErrorLikeRoi(const Roi &roi) {
  String s = roiSearchText(roi);
  static const char *errorKeys[] = {
    "error", "err", "fault", "alarm", "warn", "warning", "fehler", "stoerung"
  };
  return containsAny(s, errorKeys, sizeof(errorKeys) / sizeof(errorKeys[0]));
}

static String haComponentForRoi(const Roi &roi) {
  if (roi.type == "symbol") {
    return "binary_sensor";
  }
  return "sensor";
}

static String haObjectIdForRoi(const AppConfig &cfg, const Roi &roi) {
  return sanitizeId(devicePersistentUid() + "_" + roi.id);
}

static String haConfigTopicForRoi(const AppConfig &cfg, const Roi &roi) {
  String component = haComponentForRoi(roi);
  String objectId = haObjectIdForRoi(cfg, roi);
  return cfg.ha.discovery_prefix + "/" + component + "/" + objectId + "/config";
}

static String mqttAvailabilityTopic(const AppConfig &cfg) {
  return cfg.mqtt.topic + "/availability";
}

static String mqttImageTopic(const AppConfig &cfg) {
  return cfg.mqtt.topic + "/snapshot";
}

static String haCameraObjectId(const AppConfig &cfg) {
  return sanitizeId(devicePersistentUid() + "_snapshot");
}

static String haCameraConfigTopic(const AppConfig &cfg) {
  return cfg.ha.discovery_prefix + "/image/" + haCameraObjectId(cfg) + "/config";
}

static String haEntityNameForRoi(const Roi &roi) {
  if (roi.ha_name.length() > 0) {
    return roi.ha_name;
  }
  if (roi.label.length() > 0) {
    return roi.label;
  }
  return roi.id;
}

static String haValueTemplateForRoi(const Roi &roi) {
  if (roi.type == "symbol") {
    return "{{ 'ON' if value_json['" + roi.id + "'] else 'OFF' }}";
  }
  return "{{ value_json['" + roi.id + "'] }}";
}

static String autoUnitForRoi(const Roi &roi) {
  if (roi.ha_unit.length() > 0) {
    return roi.ha_unit;
  }

  if (isErrorLikeRoi(roi)) {
    if (roi.type == "symbol") {
      return "";
    }
    return "code";
  }

  String s = roiSearchText(roi);

  static const char *tempKeys[] = {"temp", "temperature", "celsius", "flow temp", "water temp"};
  static const char *humidKeys[] = {"humidity", "humid", "rh"};
  static const char *pressureKeys[] = {"pressure", "press", "bar", "mbar"};
  static const char *powerKeys[] = {"power", "watt", "kw", "kilowatt"};
  static const char *voltageKeys[] = {"voltage", "volt"};
  static const char *currentKeys[] = {"current", "amp", "amps", "ampere"};
  
  static const char *energyKeys[] = {"energy", "kwh", "wh"};
  static const char *timeKeys[] = {"runtime", "run time", "hours", "hour", "time"};
  static const char *volumeKeys[] = {"liter", "litre", "volume", "tank", "fill", "level"};

  if (containsAny(s, tempKeys, sizeof(tempKeys) / sizeof(tempKeys[0]))) return "°C";
  if (containsAny(s, humidKeys, sizeof(humidKeys) / sizeof(humidKeys[0]))) return "%";
  if (containsAny(s, pressureKeys, sizeof(pressureKeys) / sizeof(pressureKeys[0]))) return "bar";
  if (containsAny(s, powerKeys, sizeof(powerKeys) / sizeof(powerKeys[0]))) return "W";
  if (containsAny(s, voltageKeys, sizeof(voltageKeys) / sizeof(voltageKeys[0]))) return "V";
  if (containsAny(s, currentKeys, sizeof(currentKeys) / sizeof(currentKeys[0]))) return "A";
  if (containsAny(s, energyKeys, sizeof(energyKeys) / sizeof(energyKeys[0]))) return "kWh";
  if (containsAny(s, timeKeys, sizeof(timeKeys) / sizeof(timeKeys[0]))) return "h";
  if (containsAny(s, volumeKeys, sizeof(volumeKeys) / sizeof(volumeKeys[0]))) return "l";

  return "";
}

static String autoIconForRoi(const Roi &roi) {
  if (roi.ha_icon.length() > 0) {
    return roi.ha_icon;
  }

  if (isErrorLikeRoi(roi)) return "mdi:alert";

  String s = roiSearchText(roi);

  static const char *tempKeys[] = {"temp", "temperature", "celsius", "flow temp", "water temp"};
  static const char *humidKeys[] = {"humidity", "humid", "rh"};
  static const char *pressureKeys[] = {"pressure", "press", "bar", "mbar"};
  static const char *powerKeys[] = {"power", "watt", "kw", "kilowatt"};
  static const char *voltageKeys[] = {"voltage", "volt"};
  static const char *currentKeys[] = {"current", "amp", "amps", "ampere"};
  static const char *energyKeys[] = {"energy", "kwh", "wh"};
  static const char *timeKeys[] = {"runtime", "run time", "hours", "hour", "time"};
  static const char *volumeKeys[] = {"liter", "litre", "volume", "tank", "fill", "level"};
  static const char *heatKeys[] = {"heat", "heating", "burner"};
  static const char *flowKeys[] = {"flow"};
  if (containsAny(s, tempKeys, sizeof(tempKeys) / sizeof(tempKeys[0]))) return "mdi:thermometer";
  if (containsAny(s, humidKeys, sizeof(humidKeys) / sizeof(humidKeys[0]))) return "mdi:water-percent";
  if (containsAny(s, pressureKeys, sizeof(pressureKeys) / sizeof(pressureKeys[0]))) return "mdi:gauge";
  if (containsAny(s, powerKeys, sizeof(powerKeys) / sizeof(powerKeys[0]))) return "mdi:flash";
  if (containsAny(s, voltageKeys, sizeof(voltageKeys) / sizeof(voltageKeys[0]))) return "mdi:sine-wave";
  if (containsAny(s, currentKeys, sizeof(currentKeys) / sizeof(currentKeys[0]))) return "mdi:current-ac";
  if (containsAny(s, energyKeys, sizeof(energyKeys) / sizeof(energyKeys[0]))) return "mdi:lightning-bolt";
  if (containsAny(s, timeKeys, sizeof(timeKeys) / sizeof(timeKeys[0]))) return "mdi:timer-outline";
  if (containsAny(s, volumeKeys, sizeof(volumeKeys) / sizeof(volumeKeys[0]))) return "mdi:cup-water";
  if (containsAny(s, heatKeys, sizeof(heatKeys) / sizeof(heatKeys[0]))) return "mdi:radiator";
  if (containsAny(s, flowKeys, sizeof(flowKeys) / sizeof(flowKeys[0]))) return "mdi:waves-arrow-right";

  if (roi.type == "symbol") {
    return "mdi:toggle-switch";
  }

  return "mdi:counter";
}

static String autoDeviceClassForRoi(const Roi &roi) {
  if (roi.ha_device_class.length() > 0) {
    return roi.ha_device_class;
  }

  if (isErrorLikeRoi(roi)) {
    if (roi.type == "symbol") return "problem";
    return "";
  }

  String s = roiSearchText(roi);

  static const char *tempKeys[] = {"temp", "temperature", "celsius"};
  static const char *humidKeys[] = {"humidity", "humid", "rh"};
  static const char *pressureKeys[] = {"pressure", "press", "bar", "mbar"};
  static const char *powerKeys[] = {"power", "watt", "kw", "kilowatt"};
  static const char *voltageKeys[] = {"voltage", "volt"};
  static const char *currentKeys[] = {"current", "amp", "amps", "ampere"};
  static const char *energyKeys[] = {"energy", "kwh", "wh"};
  static const char *durationKeys[] = {"runtime", "run time", "hours", "hour", "time"};
  static const char *heatKeys[] = {"heat", "heating", "burner"};
  if (roi.type == "symbol") {
    if (containsAny(s, heatKeys, sizeof(heatKeys) / sizeof(heatKeys[0]))) return "heat";
    return "";
  }

  if (containsAny(s, tempKeys, sizeof(tempKeys) / sizeof(tempKeys[0]))) return "temperature";
  if (containsAny(s, humidKeys, sizeof(humidKeys) / sizeof(humidKeys[0]))) return "humidity";
  if (containsAny(s, pressureKeys, sizeof(pressureKeys) / sizeof(pressureKeys[0]))) return "pressure";
  if (containsAny(s, powerKeys, sizeof(powerKeys) / sizeof(powerKeys[0]))) return "power";
  if (containsAny(s, voltageKeys, sizeof(voltageKeys) / sizeof(voltageKeys[0]))) return "voltage";
  if (containsAny(s, currentKeys, sizeof(currentKeys) / sizeof(currentKeys[0]))) return "current";
  if (containsAny(s, energyKeys, sizeof(energyKeys) / sizeof(energyKeys[0]))) return "energy";
  if (containsAny(s, durationKeys, sizeof(durationKeys) / sizeof(durationKeys[0]))) return "duration";

  return "";
}

static String autoStateClassForRoi(const Roi &roi) {
  if (roi.ha_state_class.length() > 0) {
    return roi.ha_state_class;
  }

  if (isErrorLikeRoi(roi)) {
    return "";
  }

  if (roi.type == "symbol") {
    return "";
  }

  String s = roiSearchText(roi);

  static const char *energyKeys[] = {"energy", "kwh", "wh"};
  if (containsAny(s, energyKeys, sizeof(energyKeys) / sizeof(energyKeys[0]))) {
    return "total_increasing";
  }

  return "measurement";
}


static bool publishOnce(PubSubClient &mqtt, const String &topic, const String &payload) {
  bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
  if (!ok) {
    LOGE("MQTT publish failed (state=%d len=%u), retrying in 500ms: topic=%s\n",
         mqtt.state(), (unsigned)payload.length(), topic.c_str());
    delay(500);  // longer pause so lwIP can fully drain TCP write buffer
    ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
    if (!ok) {
      LOGE("MQTT publish retry failed (state=%d): topic=%s\n",
           mqtt.state(), topic.c_str());
    }
  }
  return ok;
}

static bool publishDiscoveryForRoi(PubSubClient &mqtt, const AppConfig &cfg, const Roi &roi) {
  if (!roi.ha_enabled) {
    LOGI("HA discovery skipped for roi=%s (ha_enabled=false)\n", roi.id.c_str());
    return true;
  }

  String component = haComponentForRoi(roi);
  String topic = haConfigTopicForRoi(cfg, roi);
  String objectId = haObjectIdForRoi(cfg, roi);

  JsonDocument doc;

  doc["name"] = haEntityNameForRoi(roi);
  doc["object_id"] = objectId;
  doc["unique_id"] = devicePersistentUid() + "_" + sanitizeId(roi.id);
  doc["state_topic"] = cfg.mqtt.topic;
  doc["value_template"] = haValueTemplateForRoi(roi);
  doc["availability_topic"] = mqttAvailabilityTopic(cfg);
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  String unit = autoUnitForRoi(roi);
  String icon = autoIconForRoi(roi);
  String deviceClass = autoDeviceClassForRoi(roi);
  String stateClass = autoStateClassForRoi(roi);

  if (unit.length() > 0 && component == "sensor") {
    doc["unit_of_measurement"] = unit;
  }

  if (icon.length() > 0) {
    doc["icon"] = icon;
  }

  if (deviceClass.length() > 0) {
    doc["device_class"] = deviceClass;
  }

  if (stateClass.length() > 0 && component == "sensor" && unit.length() > 0) {
    doc["state_class"] = stateClass;
  }

  if (component == "binary_sensor") {
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
  }

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(devicePersistentUid());
  device["name"] = cfg.device.name;
  device["manufacturer"] = "Dr.Big";
  device["model"] = "ESP32 Display Reader";

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
  device["sw_version"] = FW_VERSION;

  String payload;
  serializeJson(doc, payload);

  LOGI("HA discovery publish: roi=%s component=%s payload_len=%u topic=%s\n",
       roi.id.c_str(),
       component.c_str(),
       (unsigned)payload.length(),
       topic.c_str());

  bool ok = publishOnce(mqtt, topic, payload);

  if (ok) {
    LOGI("HA discovery publish ok: roi=%s\n", roi.id.c_str());
  }

  return ok;
}

static void publishDiscoveryForSnapshot(PubSubClient &mqtt, const AppConfig &cfg) {
  String topic = haCameraConfigTopic(cfg);  // now points to /image/

  if (!cfg.mqtt.image_enabled) {
    // Clear both old camera and new image entries
    String oldCameraTopic = cfg.ha.discovery_prefix + "/camera/" + haCameraObjectId(cfg) + "/config";
    mqtt.publish(oldCameraTopic.c_str(), "", true);
    mqtt.publish(topic.c_str(), "", true);
    LOGI("HA image discovery removed (image publishing disabled)\n");
    return;
  }

  JsonDocument doc;
  doc["name"] = "Snapshot";
  doc["object_id"] = haCameraObjectId(cfg);
  doc["unique_id"] = devicePersistentUid() + "_snapshot";
  // HA MQTT image entity (homeassistant/image/) uses "image_topic" for raw binary JPEG
  doc["image_topic"] = mqttImageTopic(cfg);
  doc["availability_topic"] = mqttAvailabilityTopic(cfg);
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  doc["icon"] = "mdi:image";

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(devicePersistentUid());
  device["name"] = cfg.device.name;
  device["manufacturer"] = "Dr.Big";
  device["model"] = "ESP32 Display Reader";

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
  device["sw_version"] = FW_VERSION;

  String payload;
  serializeJson(doc, payload);

  bool ok = publishOnce(mqtt, topic, payload);
  if (ok) {
    LOGI("HA camera discovery publish ok: topic=%s\n", topic.c_str());
  }
}

void mqttPublishHomeAssistantDiscovery(PubSubClient &mqtt, const AppConfig &cfg) {
  if (!cfg.mqtt.enabled) {
    LOGD("HA discovery skipped: MQTT disabled\n");
    return;
  }

  if (!mqtt.connected()) {
    LOGD("HA discovery skipped: MQTT not connected\n");
    return;
  }

  LOGI("Publishing Home Assistant discovery for %u ROIs\n",
       (unsigned)cfg.rois.size());

  for (const auto &roi : cfg.rois) {
    publishDiscoveryForRoi(mqtt, cfg, roi);
    delay(50);  // give WiFi/lwIP time to drain TCP write buffer (AP+STA mode needs more)
  }

  publishDiscoveryForSnapshot(mqtt, cfg);
  delay(50);

  // Publish pre-capture binary sensor discovery
  if (cfg.pre_capture.enabled) {
    String topic = cfg.ha.discovery_prefix + "/binary_sensor/" +
                   sanitizeId(devicePersistentUid() + "_pre_capture") + "/config";
    
    JsonDocument doc;
    doc["name"] = "Pre-Capture Trigger";
    doc["object_id"] = sanitizeId(devicePersistentUid() + "_pre_capture");
    doc["unique_id"] = devicePersistentUid() + "_pre_capture";
    doc["state_topic"] = cfg.mqtt.topic + "/" + cfg.pre_capture.mqtt_topic;
    doc["value_template"] = "{{ 'ON' if value_json.state == 'on' else 'OFF' }}";
    doc["availability_topic"] = cfg.mqtt.topic + "/availability";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:camera-timer";
    // omit device_class entirely — empty string causes HA to reject the config

    JsonObject device = doc["device"].to<JsonObject>();
    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(devicePersistentUid());
    device["name"] = cfg.device.name;
    device["manufacturer"] = "Dr.Big";
    device["model"] = "ESP32 Display Reader";

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
    device["sw_version"] = FW_VERSION;

    String payload;
    serializeJson(doc, payload);

    bool ok = publishOnce(mqtt, topic, payload);
    if (ok) {
      LOGI("HA pre-capture discovery publish ok: topic=%s\n", topic.c_str());
    }
  }
}
