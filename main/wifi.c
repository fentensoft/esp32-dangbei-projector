#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <string.h>

#include "tasks.h"

#define LED2 13
#define WIFI_SSID "lym"
#define WIFI_PASSWORD "lym941028"

static const char *TAG = "wifi_task";
static const int CONNECTED_BIT = BIT0;
static EventGroupHandle_t s_wifi_event_group;

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                   void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, reason: %d",
                 ((wifi_event_sta_disconnected_t *)event_data)->reason);
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        gpio_set_level(LED2, 1);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Connected");
        init_mqtt();
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        gpio_set_level(LED2, 0);
    }
}

void init_wifi(void) {
    gpio_set_direction(LED2, GPIO_MODE_OUTPUT);
    gpio_set_level(LED2, 1);
    esp_netif_init();
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler,
                               NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler,
                               NULL);
    wifi_config_t wifi_config = {.sta = {
                                     .ssid = WIFI_SSID,
                                     .password = WIFI_PASSWORD,
                                 }};
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}
