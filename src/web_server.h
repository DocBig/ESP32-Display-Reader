#pragma once

#include <WebServer.h>
#include <PubSubClient.h>
#include "app_config.h"

void setupWebServer(WebServer &server, PubSubClient &mqtt, AppConfig &cfg, String &lastPayload);

extern bool g_liveStreamTriggerActive;