#include <esp_event_loop.h>
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>

#include "tasks.h"

void app_main(void) {
    nvs_flash_init();
    esp_event_loop_create_default();
    init_ble();
    init_wifi();
}