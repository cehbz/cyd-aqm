#include "ha_discovery.hpp"
#include "mqtt.hpp"

#include <cstdio>
#include "cJSON.h"
#include "esp_log.h"

namespace {

const char *TAG = "ha_disc";

struct SensorDef {
    const char *entity;        // suffix for topic + unique_id
    const char *name;          // human-readable
    const char *device_class;  // HA device_class (nullptr if none)
    const char *unit;          // unit_of_measurement (nullptr if none)
};

constexpr SensorDef kSen55Sensors[] = {
    {"pm1_0",    "PM1.0",       "pm1",         "µg/m³"},
    {"pm2_5",    "PM2.5",       "pm25",        "µg/m³"},
    {"pm4_0",    "PM4.0",       nullptr,       "µg/m³"},
    {"pm10",     "PM10",        "pm10",        "µg/m³"},
    {"temp",     "Temperature", "temperature", "°C"},
    {"humidity", "Humidity",    "humidity",    "%"},
    {"voc",      "VOC Index",   "volatile_organic_compounds_part", nullptr},
    {"nox",      "NOx Index",   nullptr,       nullptr},
};

cJSON *make_device_block(const char *device_id)
{
    cJSON *dev = cJSON_CreateObject();
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString(device_id));
    cJSON_AddItemToObject(dev, "identifiers", ids);

    char name[32];
    std::snprintf(name, sizeof(name), "SEN55 %s", device_id);
    cJSON_AddStringToObject(dev, "name", name);
    cJSON_AddStringToObject(dev, "model", "SEN55");
    cJSON_AddStringToObject(dev, "manufacturer", "haynes");
    cJSON_AddStringToObject(dev, "sw_version", "1.0.0");

    return dev;
}

void publish_sensor_config(const char *device_id, const SensorDef &s)
{
    // Discovery topic: homeassistant/sensor/<device_id>/<entity>/config
    char topic[128];
    std::snprintf(topic, sizeof(topic),
                  "homeassistant/sensor/%s/%s/config", device_id, s.entity);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", s.name);

    char unique_id[64];
    std::snprintf(unique_id, sizeof(unique_id),
                  "sensor_%s_%s", device_id, s.entity);
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddStringToObject(root, "object_id", unique_id);

    char state_topic[64];
    std::snprintf(state_topic, sizeof(state_topic),
                  "aqm/%s/sensor/%s", device_id, s.entity);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    if (s.device_class) {
        cJSON_AddStringToObject(root, "device_class", s.device_class);
    }
    if (s.unit) {
        cJSON_AddStringToObject(root, "unit_of_measurement", s.unit);
    }

    cJSON_AddStringToObject(root, "state_class", "measurement");

    char avail_topic[64];
    std::snprintf(avail_topic, sizeof(avail_topic),
                  "aqm/%s/availability", device_id);
    cJSON_AddStringToObject(root, "availability_topic", avail_topic);

    cJSON_AddItemToObject(root, "device", make_device_block(device_id));

    char *json = cJSON_PrintUnformatted(root);
    mqtt_publish(topic, json, 1, true);
    ESP_LOGI(TAG, "Published: %s", topic);

    cJSON_free(json);
    cJSON_Delete(root);
}

} // namespace

void ha_discovery_publish_sen55(const char *device_id)
{
    for (const auto &s : kSen55Sensors) {
        publish_sensor_config(device_id, s);
    }
}
