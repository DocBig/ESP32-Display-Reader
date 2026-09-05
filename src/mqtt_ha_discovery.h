#pragma once

#include <PubSubClient.h>
#include "app_config.h"

void mqttPublishHomeAssistantDiscovery(PubSubClient &mqtt, const AppConfig &cfg);