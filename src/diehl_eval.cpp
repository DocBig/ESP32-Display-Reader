#include "diehl_reader.h"
#include <ArduinoJson.h>
#include <Arduino.h>

String evaluateDiehlState(AppConfig &cfg) {
  DiehlReader reader(cfg);
  DiehlResult result;

  JsonDocument doc;
  doc["valid"] = false;

  if (!reader.run(result)) {
    Serial.println("Diehl reader failed");
    String out;
    serializeJson(doc, out);
    return out;
  }

  doc["valid"] = true;

  // Seiten 01–04
  for (auto &p : result.pages) {
    char key[12];
    snprintf(key, sizeof(key), "page_%02d", p.first);
    doc[key] = p.second;
  }

  // Fehlerseite
  JsonArray errors = doc["errors"].to<JsonArray>();

  for (auto &e : result.errors) {
    JsonObject obj = errors.add<JsonObject>();
    obj["code"] = e.first;

    String types;
    for (char t : e.second) {
      types += t;
    }
    obj["types"] = types;
  }

  String out;
  serializeJson(doc, out);
  return out;
}