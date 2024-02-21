#include <driver/gpio.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <string.h>

#include "tasks.h"

#define LED1 12

static const char* TAG = "mqtt_task";
static char* topic = "esp32projector006";
static const char* server = "mqtt://192.168.10.10:1883";
static const char* MSG_ON = "ON";
static const char* MSG_OFF = "OFF";
static const char* MSG_TOGGLE = "TOGGLE";
static bool initiated = false;
static const int WORKING = BIT0;
static EventGroupHandle_t s_projector_event_group;
static esp_mqtt_client_handle_t client = NULL;

void toggle_task(void* param) {
    xEventGroupSetBits(s_projector_event_group, WORKING);
    if (param == NULL && is_ble_connected()) {
        // Turn off
        send_power_key(true);
        vTaskDelay(700 / portTICK_PERIOD_MS);
        send_power_key(false);

        int counter = 0;
        // Wait 10 seconds for BLE to disconnect
        while (is_ble_connected() && counter < 10) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ++counter;
        }
        if (!is_ble_connected()) {
            ESP_LOGI(TAG, "Successfully turned off the projector");
        } else {
            ESP_LOGE(TAG, "Failed to turn off the projector");
            esp_mqtt_client_publish(client, topic, MSG_ON, 0, 0, 0);
        }
    } else if (param != NULL && !is_ble_connected()) {
        // Turn on
        adv_with_mfg_data(true);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        adv_with_mfg_data(false);
        int counter = 0;
        // Wait 30 seconds for BLE to connect
        while (!is_ble_connected() && counter < 30) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ++counter;
        }
        if (is_ble_connected()) {
            ESP_LOGI(TAG, "Successfully turned on the projector");
        } else {
            ESP_LOGE(TAG, "Failed to turn on the projector");
            esp_mqtt_client_publish(client, topic, MSG_OFF, 0, 0, 0);
        }
    }
    xEventGroupClearBits(s_projector_event_group, WORKING);
    vTaskDelete(NULL);
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    ESP_LOGI(TAG,
             "Event dispatched from event loop base=%s, event_id=%" PRIi32 "",
             base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT broker connected");
        esp_mqtt_client_subscribe(client, topic, 0);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "MQTT broker disconnected");
    } else if (event_id == MQTT_EVENT_SUBSCRIBED) {
        ESP_LOGI(TAG, "MQTT topic subscribed");
        set_connected_state(is_ble_connected());
    } else if (event_id == MQTT_EVENT_DATA) {
        ESP_LOGI(TAG, "MQTT data received");
        strupr(event->data);
        if ((!strncmp(event->data, MSG_ON, event->data_len) ||
             !strncmp(event->data, MSG_TOGGLE,
                      event->data_len)) &&  // ON or TOGGLE
            !is_ble_connected() &&          // BLE is not connected
            !(xEventGroupGetBits(s_projector_event_group) &
              WORKING)  // Task is not working
        ) {
            ESP_LOGI(TAG, "Turning on the projector");
            xTaskCreate(toggle_task, "toggle_task", 4096, &initiated, 5, NULL);
        } else if ((!strncmp(event->data, MSG_OFF, event->data_len) ||
                    !strncmp(event->data, MSG_TOGGLE,
                             event->data_len)) &&  // OFF or TOGGLE
                   is_ble_connected() &&           // BLE is connected
                   !(xEventGroupGetBits(s_projector_event_group) &
                     WORKING)  // Task is not working
        ) {
            ESP_LOGI(TAG, "Turning off the projector");
            xTaskCreate(toggle_task, "toggle_task", 4096, NULL, 5, NULL);
        }
    }
}

void init_mqtt(void) {
    if (!initiated) {
        gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
        gpio_set_level(LED1, 0);
        esp_mqtt_client_config_t config = {.broker.address.uri = server};
        client = esp_mqtt_client_init(&config);
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                       mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);
        s_projector_event_group = xEventGroupCreate();
        initiated = true;
    }
}

void set_connected_state(bool is_connected) {
    if (initiated) {
        if (is_connected) {
            gpio_set_level(LED1, 1);
            esp_mqtt_client_publish(client, topic, MSG_ON, 0, 0, 0);
        } else {
            gpio_set_level(LED1, 0);
            esp_mqtt_client_publish(client, topic, MSG_OFF, 0, 0, 0);
        }
    }
}