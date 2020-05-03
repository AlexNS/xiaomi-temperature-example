#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <sstream>

int scanTime = 25; // seconds
BLEScan* pBLEScan;

void printBuffer(uint8_t* buf, int len) {
    for(int i = 0; i < len; i++) {
        Serial.printf("%02x", buf[i]);
    }
    Serial.print("\n");
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    uint8_t* findServiceData(uint8_t* data, size_t length, uint8_t* foundBlockLength) {
        // пэйлоад у состоит из блоков [байт длины][тип блока][данные]
        // нам нужен блок с типом 0x16, следом за которым будет 0x95 0xfe
        // поэтому считываем длину, проверяем следующий байт и если что пропускаем
        // вернуть надо указатель на нужный блок и длину блока
        uint8_t* rightBorder = data + length;
        while (data < rightBorder) {
            uint8_t blockLength = *data;
            if (blockLength < 5) { // нам точно такие блоки не нужны
                data += (blockLength+1);
                continue;
            }
            uint8_t blockType = *(data+1);
            uint16_t serviceType = *(uint16_t*)(data + 2);
            if (blockType == 0x16 && serviceType == 0xfe95) { // мы нашли что искали
                *foundBlockLength = blockLength-3; // вычитаем длину типа сервиса
                return data+4; // пропускаем длину и тип сервиса
            }
            data += (blockLength+1);
        }   
        return nullptr;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (!advertisedDevice.haveName() || advertisedDevice.getName().compare("MJ_HT_V1"))
            return; // нас интересуют только устройства, которые транслируют нужное нам имя

        uint8_t* payload = advertisedDevice.getPayload();
        size_t payloadLength = advertisedDevice.getPayloadLength();
        Serial.printf("\n\nAdvertised Device: %s\n", advertisedDevice.toString().c_str());
        printBuffer(payload, payloadLength);
        uint8_t serviceDataLength=0;
        uint8_t* serviceData = findServiceData(payload, payloadLength, &serviceDataLength);

        if (serviceData == nullptr) {
            return; // нам этот пакет больше не интересен
        }

        Serial.printf("Found service data len: %d\n", serviceDataLength);
        printBuffer(serviceData, serviceDataLength);

        // 11й байт в пакете означает тип события
        // 0x0D - температура и влажность
        // 0x0A - батарейка
        // 0x06 - влажность
        // 0x04 - температура

        switch (serviceData[11])
        {
            case 0x0D:
            {
                float temp = *(uint16_t*)(serviceData + 11 + 3) / 10.0;
                float humidity = *(uint16_t*)(serviceData + 11 + 5) / 10.0;
                Serial.printf("Temp: %f Humidity: %f\n", temp, humidity);
            }
            break;
            case 0x04:
            {
                float temp = *(uint16_t*)(serviceData + 11 + 3) / 10.0;
                Serial.printf("Temp: %f\n", temp);
            }
            break;
            case 0x06:
            {
                float humidity = *(uint16_t*)(serviceData + 11 + 3) / 10.0;
                Serial.printf("Humidity: %f\n", humidity);
            }
            break;
            case 0x0A:
            {
                int battery = *(serviceData + 11 + 3);
                Serial.printf("Battery: %d\n", battery);
            }
            break;
        default:
            break;
        }
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Scanning...");
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); // создаем объект, который будте отвечать за сканирование BLE
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setActiveScan(true); 
}

void loop() {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.print("Devices found: ");
    Serial.println(foundDevices.getCount());
    Serial.println("Scan done!");
    pBLEScan->stop();
    pBLEScan->clearResults();
    delay(5000);
}