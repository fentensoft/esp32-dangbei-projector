#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>

#include "tasks.h"

void app_main(void) {
    nvs_flash_init();
    init_ble();
    init_wifi();
}