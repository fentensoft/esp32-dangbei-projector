#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "ble_remote.hpp"

enum class State {
    kStandby = 0,
    kWakingUp,
    kConnected,
    kShuttingDown,
};

static State currState = State::kStandby;
static bool toShutDown = false;
static bool toWakeUp = false;
static uint8_t counter = 0;
static uint16_t forceSyncCounter = 0;

static const char* wifi_ssid = "homewifi";
static const char* wifi_password = "home123456";
static const char* bluetooth_name = "fentensoft";
static const char* mqtt_server = "192.168.10.10";
static uint16_t mqtt_port = 1883;
static const char* mqtt_client_id = "";
static const char* mqtt_topic = "esp32projector006";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (!strncmp((const char*)payload, "ON", length) &&
        currState == State::kStandby) {
        Serial.println("Waking up");
        forceSyncCounter = 0;
        toWakeUp = true;
    } else if (!strncmp((const char*)payload, "OFF", length) &&
               currState == State::kConnected) {
        Serial.println("Shutting down");
        forceSyncCounter = 0;
        toShutDown = true;
    }
}

void setup() {
    Serial.begin(9600);

    Serial.println("Initializing bluetooth server");
    BluetoothManager::getInstance().init(bluetooth_name);

    Serial.println("Connecting WiFi");
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Connecting to MQTT broker");
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(onMqttMessage);

    BluetoothManager::getInstance().startAdvertising();
}

void syncToMqtt(bool force = false) {
    static std::string lastStateStr = "";
    std::string currStateStr;
    if (currState == State::kConnected || currState == State::kShuttingDown) {
        currStateStr = "ON";
    } else {
        currStateStr = "OFF";
    }
    if (currStateStr != lastStateStr || force) {
        if (mqttClient.publish(mqtt_topic, currStateStr.c_str())) {
            lastStateStr = currStateStr;
        } else {
            lastStateStr = "";
        }
    }
}

void loop() {
    if (mqttClient.connected()) {
        mqttClient.loop();
    } else {
        if (!mqttClient.connect(mqtt_client_id)) {
            Serial.println("MQTT connection failed!");
        } else {
            Serial.println("MQTT connected");
            mqttClient.subscribe(mqtt_topic);
        }
    }

    BluetoothManager::getInstance().loop();

    switch (currState) {
        case State::kStandby:
            if (BluetoothManager::getInstance().isConnected()) {
                Serial.println("Bluetooth connected");
                BluetoothManager::getInstance().stopAdvertising();
                currState = State::kConnected;
            }
            if (toWakeUp) {
                toWakeUp = false;
                currState = State::kWakingUp;
                counter = 0;
            }
            break;
        case State::kWakingUp:
            if (counter == 0) {
                BluetoothManager::getInstance().startWakingUp();
                ++counter;
            } else if (counter >= 100) {
                // Send wake up message for 5s
                BluetoothManager::getInstance().stopWakingUp();
                counter = 0;
                currState = State::kStandby;
            } else {
                ++counter;
            }
            break;
        case State::kConnected:
            if (!BluetoothManager::getInstance().isConnected()) {
                Serial.println("Bluetooth disconnected");
                BluetoothManager::getInstance().startAdvertising();
                currState = State::kStandby;
            }
            if (toShutDown) {
                toShutDown = false;
                counter = 0;
                currState = State::kShuttingDown;
            }
            break;
        case State::kShuttingDown:
            if (counter == 0) {
                BluetoothManager::getInstance().sendKey(std::vector<uint8_t>{
                    0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00});
                ++counter;
            } else if (counter >= 15) {
                // Long press power button for 0.75s
                BluetoothManager::getInstance().sendKey(std::vector<uint8_t>{
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
                counter = 0;
                currState = State::kConnected;
            } else {
                ++counter;
            }
            break;
    }

    if (++forceSyncCounter >= 600) {
        // Force sync every 30 seconds to prevent the projector from being stuck
        // in a wrong state
        forceSyncCounter = 0;
        syncToMqtt(true);
    } else {
        syncToMqtt(false);
    }

    delay(50);
}
