#pragma once

#include <Arduino.h>

String deviceMacAddress();
String deviceMacSuffix();
String defaultDeviceId();
String defaultDeviceName();
String defaultHostname();
String defaultMqttTopicBase();

// Returns a persistent hardware-unique ID stored in LittleFS.
// Generated once from esp_random() on first boot, survives reboots and
// config import/export. Must be called after storageBegin().
String devicePersistentUid();