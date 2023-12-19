#include <NimBLECharacteristic.h>
#include <NimBLEServer.h>

#include <string>

class BluetoothManager {
   public:
    void init(const char *deviceName);
    void loop();
    bool isConnected();

    void startAdvertising();
    void stopAdvertising();

    void startWakingUp();
    void stopWakingUp();

    void sendKey(const std::vector<uint8_t> &key);

    static BluetoothManager &getInstance();

   private:
    std::string deviceName_;
    bool isConnected_ = false;
    NimBLEServer *pServer_;
    NimBLEService *pReportService_;
    NimBLEService *pBatteryService_;
    NimBLECharacteristic *pReport_;

    void fillAdvData(bool fillManufacturerData);
};
