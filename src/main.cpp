#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include <Update.h>

#include <ArduinoJson.h>

#include <LittleFS.h>

#include <OTA_Update_.h>


// Callback for handling data received on the RX characteristic
int SEGMENT = 0;
int FULL_PACKAGE = 0;
int SIZE_OTA = 0;
bool isUpdate = false;
String msg_status = "";
bool status_update = false;

// Define the BLE Service and Characteristics
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicRX = NULL;
BLECharacteristic* pCharacteristicTX = NULL;

// Characteristic UUIDs
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID_RX("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID charUUID_TX("e32d6400-0a1c-43af-a591-8634cc4b7af4");

bool deviceConnected = false;
bool oldDeviceConnected = false;

// Callback for receiving data from the central device
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        isUpdate = false;
        ESP.restart();
    }
};

const String generateJsonString() {
    DynamicJsonDocument jsonDoc(256);
    jsonDoc["Segment"] = SEGMENT;
    jsonDoc["ota_size"] = SIZE_OTA;
    jsonDoc["msg_status"] = msg_status;
    jsonDoc["Total_byte"] = LittleFS.totalBytes();
    jsonDoc["Use_byte"] = LittleFS.usedBytes();

    String jsonString;
    serializeJson(jsonDoc, jsonString);
    return jsonString;
}

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    msg_status = "Written : " + String(written) + "/" + String(updateSize) + " [" + String((written / updateSize) * 100) + "%] \n";
    if (Update.end()) {
      Serial.println("OTA done!");
      msg_status = "OTA Done: ";
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting...");
        status_update = true;
        msg_status = "Success!\n";
      }
      else {
        Serial.println("Update not finished? Something went wrong!");
        msg_status = "Failed!\n";
      }

    }
    else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      msg_status = "Error #: " + String(Update.getError());
    }
  }
  else
  {
    Serial.println("Not enough space to begin OTA");
    msg_status = "Not enough space for OTA";
  }
}

void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/firmware.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Serial.println("Trying to start update");
      performUpdate(updateBin, updateSize);
    }
    else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    // when finished remove the binary from spiffs to indicate end of the process
    Serial.println("Removing update file");
    fs.remove("/firmware.bin");
  }
  else {
    Serial.println("Could not load update.bin from spiffs root");
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    int len = rxValue.length();
    uint8_t* data = pCharacteristic->getData();

    if (deviceConnected) {
      if (SEGMENT > 0) {
        File file = LittleFS.open("/firmware.bin", "ab");  // Open file in append mode
        if (!file) {
          Serial.println("Failed to open file for writing");
          return;
        }
        //
        isUpdate = true;

        size_t bytesWritten = file.write(data, len);  // Use write method to write binary data
        if (bytesWritten == len) {
          // Print a message indicating successful data write
          msg_status = "Data written successfully";
          Serial.print("Data written successfully: ");
          for (int i = 0; i < len; i++) {
            Serial.print(data[i], HEX);
            Serial.print(' '); // Add a space between hex values for better readability
          }
          Serial.println();

        } else {
          Serial.println("Write failed");
          msg_status = "Data written successfully";
        }

        file.close();

        SIZE_OTA += len;  // Use 'len' instead of 'rxValue.length()'

        if (FULL_PACKAGE == SIZE_OTA) {
         // Print OTA success message
          Serial.println("OTA SUCCESS");

          updateFromFS(LittleFS);

          // Open the file for reading to get the file size
          File readfile = LittleFS.open("/firmware.bin", "rb");
          if (readfile) {
            Serial.print("File Size: ");
            Serial.println(readfile.size());
            readfile.close();
          } else {
            Serial.println("Failed to open file for reading");
          }

        }
        Serial.println(SIZE_OTA);
      } else {
        Serial.println(rxValue.c_str());
        FULL_PACKAGE = std::stoi(rxValue.c_str());
      }

      SEGMENT += 1;
    }
  }
};

const int ledPin = 2;

void setup() {

    Serial.begin(115200);

    pinMode(ledPin, OUTPUT);

   // Initialize FFat
    if (!LittleFS.begin(true)) {
      Serial.println("Failed to initialize FFat filesystem");
      // Handle initialization failure
    }

     if (LittleFS.remove("/firmware.bin")) {
          Serial.println("Deleted existing firmware.bin file");
      }

    LittleFS.format();
    // Rest of your setup code
    
    //fix low energy reset
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Create the BLE Server
    BLEDevice::init("ESP32 dev");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(serviceUUID);

    // Create the RX Characteristic
    pCharacteristicRX = pService->createCharacteristic(
        charUUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharacteristicRX->setCallbacks(new MyCallbacks());

    // Create the TX Characteristic
    pCharacteristicTX = pService->createCharacteristic(
        charUUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // Descriptor for the TX Characteristic
    pCharacteristicTX->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(serviceUUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Waiting for a connection...");
}

void Blinking(){
  digitalWrite(ledPin, HIGH);
  delay(500);
  digitalWrite(ledPin, LOW);
  delay(500);
}

bool dataSent = false;

void loop() {

    if(isUpdate){
      pCharacteristicTX->setValue(generateJsonString().c_str());
      pCharacteristicTX->notify();
      delay(500);
      dataSent = true;
    } else {
      // Blinking();
    }
    if(status_update){
      //delay(5000);
      //ESP.restart();
    }
}