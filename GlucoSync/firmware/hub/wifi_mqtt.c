/**
 * Wi-Fi + MQTT cloud connectivity for ESP32-S3 hub.
 * Production: uses ESP-IDF esp_wifi + esp_mqtt.
 * License: MIT
 */

#include "wifi_mqtt.h"
#include <string.h>

static mqtt_cmd_cb g_cmd_cb = NULL;
static bool g_connected = false;

void wifi_mqtt_init(void)
{
    /* Production: esp_wifi_init(), set STA mode, connect to SSID from NVS.
     * esp_mqtt_client_init() with broker URL from NVS.
     * Register event handler for MQTT_DATA events → g_cmd_cb */
}

void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb callback)
{
    g_cmd_cb = callback;
}

void wifi_mqtt_connect(const char *client_id)
{
    (void)client_id;
    /* Production: esp_mqtt_client_start() */
}

bool wifi_mqtt_is_connected(void)
{
    return g_connected;
}

void mqtt_publish(const char *topic, const char *data, int len)
{
    (void)topic; (void)data; (void)len;
    /* Production: esp_mqtt_client_publish(topic, data, len, 1, 0) */
}