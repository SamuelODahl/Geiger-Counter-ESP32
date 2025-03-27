//https://elkim.no/wp-content/uploads/2021/06/esp-wroom-32_datasheet_en-1223836.pdf
//https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
//https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

#include <SPI.h>
#include "SPIFFS.h"
#include "esp_system.h"

const int GEIGER_PIN = 4; 
const int ledPin = 2;

volatile int pulseCount;

void IRAM_ATTR countPulse() {
  pulseCount++;
}

void readDataFile() {
  File file = SPIFFS.open("/data.csv", FILE_READ);
  if (!file) {
    Serial.println("Failed to read");
    return;
  }
  Serial.println("Reading from: /data.csv:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }
  file.close();
}

void maybeResetDataFile() {
  esp_reset_reason_t reason = esp_reset_reason();

  if (reason == ESP_RST_POWERON) {
    Serial.println("Cold boot detected, clearing log file.");

    File file = SPIFFS.open("/data.csv", FILE_WRITE); //Truncates (fjerner) det som ligger i filen
    if (file) {
      file.close(); 
      Serial.println("File cleared.");
    }
  } else {
    Serial.print("Boot reason: ");
    Serial.println(reason);
    Serial.println("Log file kept (not a cold boot).");
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(GEIGER_PIN, INPUT);
  pinMode(ledPin, OUTPUT);
  attachInterrupt(GEIGER_PIN, countPulse, RISING);


  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS ready");
    maybeResetDataFile();
  }
}

void loop() {
  static unsigned long lastTime = millis();
  unsigned long currentTime = millis();

  if (currentTime - lastTime >= 6000) {
    int cpm = pulseCount / 2;  //nøyaktig dobbel signalregistrering (why?)
    pulseCount = 0;
    lastTime = currentTime;

    String dataLine = "Seconds in flight: " + String(currentTime/1000) + ", Counts per 6 seconds: " + String(cpm);

    File spiffsFile = SPIFFS.open("/data.csv", FILE_APPEND);
    if (spiffsFile) {
      spiffsFile.println(dataLine);
      spiffsFile.close();
      Serial.println("Saved to SPIFFS");
    } else {
      Serial.println("SPIFFS write failed");
    }

    digitalWrite(ledPin, HIGH);
    readDataFile();
    delay(100);
    digitalWrite(ledPin, LOW);
    
  }
}
