#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include "app_config.h"

void mqttApplyConfig(PubSubClient &mqtt, const AppConfig &cfg);
void mqttPublishState(PubSubClient &mqtt, const AppConfig &cfg, const String &payload);
void mqttPublishImage(PubSubClient &mqtt, const AppConfig &cfg, const uint8_t *payload, size_t len);
void mqttPublishPreCapture(const AppConfig &cfg, bool state);
void mqttSetGlobalClient(PubSubClient *mqtt);

// neu
void mqttPublishHealth(PubSubClient &mqtt, const AppConfig &cfg, const String &payload);
bool mqttEnsureConnected(PubSubClient &mqtt, const AppConfig &cfg);
