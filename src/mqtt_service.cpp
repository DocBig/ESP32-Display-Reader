#include "mqtt_service.h"
#include <WiFi.h>
#include "debug_log.h"
#include "mqtt_ha_discovery.h"
#include "device_identity.h"

static String mqttAvailabilityTopic(const AppConfig &cfg) {
  return cfg.mqtt.topic + "/availability";
}

static String mqttImageTopic(const AppConfig &cfg) {
  return cfg.mqtt.topic + "/snapshot";
}

bool mqttEnsureConnected(PubSubClient &mqtt, const AppConfig &cfg) {
  if (!cfg.mqtt.enabled) {
    LOGD("MQTT disabled\n");
    return false;
  }

  if (mqtt.connected()) {
    return true;
    
  }

  String cid = "display-reader-" + devicePersistentUid();
  String availTopic = mqttAvailabilityTopic(cfg);

  LOGI("Connecting MQTT to %s:%u\n", cfg.mqtt.host.c_str(), cfg.mqtt.port);

  bool ok = false;

  if (cfg.mqtt.username.length() > 0) {
    ok = mqtt.connect(
      cid.c_str(),
      cfg.mqtt.username.c_str(),
      cfg.mqtt.password.c_str(),
      availTopic.c_str(),
      0,
      true,
      "offline"
    );
  } else {
    ok = mqtt.connect(
      cid.c_str(),
      availTopic.c_str(),
      0,
      true,
      "offline"
    );
  }

  if (!ok) {
    LOGE("MQTT connect failed, rc=%d\n", mqtt.state());
    return false;
  }

  LOGI("MQTT connected\n");
  mqtt.publish(availTopic.c_str(), "online", true);
  mqttPublishHomeAssistantDiscovery(mqtt, cfg);
  return true;
}

void mqttApplyConfig(PubSubClient &mqtt, const AppConfig &cfg) {
  mqtt.setServer(cfg.mqtt.host.c_str(), cfg.mqtt.port);
}

void mqttPublishState(PubSubClient &mqtt, const AppConfig &cfg, const String &payload) {
  if (!mqttEnsureConnected(mqtt, cfg)) return;

  bool ok = mqtt.publish(cfg.mqtt.topic.c_str(), payload.c_str(), true);
  if (ok) {
    LOGD("MQTT publish ok: topic=%s\n", cfg.mqtt.topic.c_str());
  } else {
    LOGE("MQTT publish failed: topic=%s\n", cfg.mqtt.topic.c_str());
  }
}

void mqttPublishHealth(PubSubClient &mqtt, const AppConfig &cfg, const String &payload) {
  if (!mqttEnsureConnected(mqtt, cfg)) return;

  String topic = cfg.mqtt.topic + "/health";

  bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
  if (ok) {
    LOGD("MQTT health publish ok: topic=%s\n", topic.c_str());
  } else {
    LOGE("MQTT health publish failed: topic=%s\n", topic.c_str());
  }
}

void mqttPublishImage(PubSubClient &mqtt, const AppConfig &cfg, const uint8_t *payload, size_t len) {
  if (!cfg.mqtt.image_enabled) return;
  if (!payload || len == 0) return;
  if (!mqttEnsureConnected(mqtt, cfg)) return;

  String topic = mqttImageTopic(cfg);

  bool ok = mqtt.publish(topic.c_str(), payload, (unsigned int)len, true);
  if (ok) {
    LOGD("MQTT image publish ok: topic=%s len=%u\n", topic.c_str(), (unsigned)len);
  } else {
    LOGE("MQTT image publish failed: topic=%s len=%u (buffer too small?)\n",
         topic.c_str(), (unsigned)len);
  }
}

static PubSubClient* g_mqttClient = nullptr;

void mqttSetGlobalClient(PubSubClient *mqtt) {
  g_mqttClient = mqtt;
}

static String mqttPreCaptureTopic(const AppConfig &cfg) {
  return cfg.mqtt.topic + "/" + cfg.pre_capture.mqtt_topic;
}

void mqttPublishPreCapture(const AppConfig &cfg, bool state) {
  if (!cfg.mqtt.enabled || !cfg.pre_capture.enabled) {
    return;
  }

  if (!g_mqttClient) {
    LOGE("Pre-capture: MQTT client not initialized\n");
    return;
  }

  if (!g_mqttClient->connected()) {
    LOGD("Pre-capture: MQTT not connected, skipping publish\n");
    return;
  }

  String topic = mqttPreCaptureTopic(cfg);
  String payload = state ? "{\"state\":\"on\"}" : "{\"state\":\"off\"}";

  bool ok = g_mqttClient->publish(topic.c_str(), payload.c_str(), true);
  if (ok) {
    LOGI("Pre-capture MQTT publish ok: topic=%s state=%s\n", topic.c_str(), state ? "on" : "off");
  } else {
    LOGE("Pre-capture MQTT publish failed: topic=%s\n", topic.c_str());
  }
}
