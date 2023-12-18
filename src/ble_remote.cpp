#include "ble_remote.hpp"

#include <NimBLEAdvertising.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEService.h>

// Dump from F3 Air remote HID descriptor
static const uint8_t _hidReportDescriptor[] = {
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x01, 0x19, 0x00, 0x2a, 0x9c,
    0x02, 0x15, 0x00, 0x26, 0x9c, 0x02, 0x95, 0x01, 0x75, 0x10, 0x81, 0x00,
    0x09, 0x02, 0xa1, 0x02, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0a, 0x15, 0x01,
    0x25, 0x0a, 0x95, 0x01, 0x75, 0x08, 0x81, 0x40, 0xc0, 0xc0, 0x06, 0x01,
    0xff, 0x09, 0x01, 0xa1, 0x02, 0x85, 0x05, 0x09, 0x14, 0x75, 0x08, 0x95,
    0x14, 0x15, 0x80, 0x25, 0x7f, 0x81, 0x22, 0x85, 0x04, 0x09, 0x04, 0x75,
    0x08, 0x95, 0x01, 0x91, 0x02, 0xc0, 0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
    0x85, 0x0a, 0x75, 0x01, 0x95, 0x08, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
    0x15, 0x00, 0x25, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x05, 0x07, 0x19, 0x00, 0x29, 0xff, 0x81, 0x00, 0xc0,
};
char manufacturerData[] = {0x46, 0x00, 0x46, 0xFA, 0xC1, 0x69, 0x04,
                           0xC8, 0x38, 0xFF, 0xFF, 0xFF, 0xFF};

void BluetoothManager::init(const char *deviceName) {
    deviceName_ = deviceName;
    NimBLEDevice::init(deviceName_);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer_ = NimBLEDevice::createServer();

    pReportService_ = pServer_->createService("1812");

    auto pHidInfo =
        pReportService_->createCharacteristic("2A4A", NIMBLE_PROPERTY::READ);
    uint8_t info[] = {0x11, 0x1, 0x0, 0x1};
    pHidInfo->setValue(info, sizeof(info));

    auto pReportMap =
        pReportService_->createCharacteristic("2A4B", NIMBLE_PROPERTY::READ);
    pReportMap->setValue(_hidReportDescriptor, sizeof(_hidReportDescriptor));

    auto pHidControl = pReportService_->createCharacteristic(
        "2A4C", NIMBLE_PROPERTY::WRITE_NR);

    auto pProtocolMode = pReportService_->createCharacteristic(
        "2A4E", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
    const uint8_t pMode[] = {0x01};
    pProtocolMode->setValue(pMode, sizeof(pMode));

    pReport_ = pReportService_->createCharacteristic(
        "2A4D", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
    auto hidNotifyDescriptor = new NimBLEDescriptor(
        "2908", BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE, 2);
    hidNotifyDescriptor->setValue(std::vector<uint8_t>{0x0a, 0x01});
    pReport_->addDescriptor(hidNotifyDescriptor);

    pReportService_->start();

    // Battery service
    pBatteryService_ = pServer_->createService("180F");
    auto pBatteryLevel = pBatteryService_->createCharacteristic(
        "2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pBatteryLevel->setValue(std::vector<uint8_t>{0x64});

    auto battery2904 = new NimBLE2904();
    battery2904->setFormat(NimBLE2904::FORMAT_UINT8);
    battery2904->setNamespace(1);
    battery2904->setUnit(0x27AD);
    pBatteryLevel->addDescriptor(battery2904);

    pBatteryService_->start();

    auto pDeviceInfoService = pServer_->createService("180A");
    pDeviceInfoService->createCharacteristic("2A50", NIMBLE_PROPERTY::READ)
        ->setValue(
            std::vector<uint8_t>{0x02, 0x54, 0x2B, 0x00, 0x16, 0x00, 0x00});
    pDeviceInfoService->start();

    auto pAdvertising = pServer_->getAdvertising();
    pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_UND);
    pAdvertising->setMinInterval(0x0640 / 10);
    pAdvertising->setMaxInterval(0x0640 / 10);

    fillAdvData(false);

    pAdvertising->setScanFilter(false, false);
    pAdvertising->setScanResponse(false);

    BLEDevice::setSecurityAuth(true, false, false);
}

void BluetoothManager::fillAdvData(bool fillManufacturerData) {
    NimBLEAdvertisementData advData{};
    advData.setFlags(BLE_HS_ADV_F_BREDR_UNSUP | BLE_HS_ADV_F_DISC_LTD);
    advData.setAppearance(HID_KEYBOARD);
    advData.setPartialServices16(std::vector<NimBLEUUID>{
        pReportService_->getUUID(),
        pBatteryService_->getUUID(),
    });
    if (fillManufacturerData) {
        advData.setManufacturerData(
            std::string(manufacturerData, sizeof(manufacturerData)));
    } else {
        advData.setName(deviceName_);
    }
    pServer_->getAdvertising()->setAdvertisementData(advData);
}

void BluetoothManager::loop() {
    isConnected_ = (pServer_->getConnectedCount() > 0);
}

bool BluetoothManager::isConnected() { return isConnected_; }

void BluetoothManager::startAdvertising() {
    pServer_->getAdvertising()->start();
}

void BluetoothManager::stopAdvertising() { pServer_->getAdvertising()->stop(); }

void BluetoothManager::startWakingUp() {
    pServer_->getAdvertising()->stop();
    fillAdvData(true);
    pServer_->getAdvertising()->start();
}

void BluetoothManager::stopWakingUp() {
    pServer_->getAdvertising()->stop();
    fillAdvData(false);
    pServer_->getAdvertising()->start();
}

void BluetoothManager::sendKey(const std::vector<uint8_t> &key) {
    pReport_->setValue(key);
    pReport_->notify(true);
}

BluetoothManager &BluetoothManager::getInstance() {
    static BluetoothManager instance;
    return instance;
}
