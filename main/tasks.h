#include <stdbool.h>

// BLE
void init_ble(void);
bool is_ble_connected(void);
void send_power_key(bool is_down);
void adv_with_mfg_data(bool on);

// WiFi
void init_wifi(void);

// MQTT
void init_mqtt(void);
void set_connected_state(bool is_connected);