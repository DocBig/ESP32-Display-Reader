#include "device_identity.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <esp_random.h>

static const char *UID_FILE = "/device_uid.txt";
static String g_cachedUid;

static void readMacBytes(uint8_t macBytes[6]) {
  uint64_t mac = ESP.getEfuseMac();

  for (int i = 0; i < 6; i++) {
    macBytes[i] = (mac >> (8 * (5 - i))) & 0xFF;
  }
}

String deviceMacAddress() {
  uint8_t mac[6];
  readMacBytes(mac);

  char buf[18];
  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(buf);
}

String deviceMacSuffix() {
  uint8_t mac[6];
  readMacBytes(mac);

  char buf[7];
  snprintf(buf, sizeof(buf),
           "%02x%02x%02x",
           mac[3], mac[4], mac[5]);

  return String(buf);
}

String defaultDeviceId() {
  return "display_reader_" + devicePersistentUid().substring(0, 6);
}

String defaultDeviceName() {
  return "Display Reader " + devicePersistentUid().substring(0, 6);
}

String defaultHostname() {
  return "display-reader-" + devicePersistentUid().substring(0, 6);
}

String defaultMqttTopicBase() {
  return "heatpump/" + defaultDeviceId();
}

String devicePersistentUid() {
  if (g_cachedUid.length() >= 12) return g_cachedUid;

  File f = LittleFS.open(UID_FILE, "r");
  if (f) {
    String stored = f.readString();
    f.close();
    stored.trim();
    if (stored.length() >= 12) {
      g_cachedUid = stored;
      return g_cachedUid;
    }
  }

  // Generate 6 random bytes → 12 hex chars
  char buf[13];
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  snprintf(buf, sizeof(buf), "%06lx%06lx",
           (unsigned long)(r1 & 0xFFFFFF),
           (unsigned long)(r2 & 0xFFFFFF));
  g_cachedUid = String(buf);

  File fw = LittleFS.open(UID_FILE, "w");
  if (fw) {
    fw.print(g_cachedUid);
    fw.close();
  }

  return g_cachedUid;
}