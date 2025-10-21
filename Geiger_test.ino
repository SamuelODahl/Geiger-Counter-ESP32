#include <SPI.h>
#include "SPIFFS.h"
#include "esp_system.h"
#include <RadioLib.h>  // Installed from Library Manager

// ─────── Pins ───────
const int GEIGER_PIN = 4; 
const int ledPin = 2;
#define LORA_NSS   5
#define LORA_BUSY  25
#define LORA_RESET 26

// ─────── SPI & LoRa Setup (but simulated for now) ───────
SPIClass spiLoRa(VSPI);
Module radioModule(LORA_NSS, -1, LORA_RESET, LORA_BUSY, spiLoRa);
SX1280 radio(&radioModule);

// ─────── Geiger Interrupt ───────
volatile int pulseCount = 0;

void IRAM_ATTR countPulse() {
  pulseCount++;
}

// ─────── Struct for readings ───────
struct GeigerReading {
  uint32_t timestamp;  // in seconds
  uint16_t cpm;
};

// ─────── Buffer ───────
const int BUFFER_SIZE = 10;
GeigerReading buffer[BUFFER_SIZE];
int writeIndex = 0;
int itemCount = 0;

void addToBuffer(uint32_t seconds, uint16_t cpm) {
  buffer[writeIndex] = { seconds, cpm };
  writeIndex = (writeIndex + 1) % BUFFER_SIZE;
  if (itemCount < BUFFER_SIZE) itemCount++;
}

// ─────── Transmit Buffer (Simulated) ───────
void transmitBuffer() {
  Serial.println("Simulating LoRa transmission...");

  for (int i = 0; i < itemCount; i++) {
    int index = (writeIndex - itemCount + i + BUFFER_SIZE) % BUFFER_SIZE;
    GeigerReading reading = buffer[index];

    // Convert struct to byte array (if needed for real LoRa)
    uint8_t packet[sizeof(GeigerReading)];
    memcpy(packet, &reading, sizeof(GeigerReading));

    // SIMULATION: Print as if sending
    Serial.print("→ Sending reading: ");
    Serial.print(reading.timestamp);
    Serial.print("s, ");
    Serial.print(reading.cpm);
    Serial.println(" CPM");

    // To send via LoRa (when connected):
    // radio.transmit(packet, sizeof(packet));
  }

  Serial.println("Buffer sent and cleared.\n");
  itemCount = 0; // Clear buffer
}

// ─────── SPIFFS Helpers ───────
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
    File file = SPIFFS.open("/data.csv", FILE_WRITE);
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

// ─────── Setup ───────
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

  // Setup LoRa (for later)
  spiLoRa.begin(18, 19, 23, LORA_NSS);
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa radio initialized (waiting for hardware)...");
  } else {
    Serial.print("LoRa init failed, code: ");
    Serial.println(state);
  }
}

// ─────── Loop ───────
void loop() {
  static unsigned long lastTime = millis();
  unsigned long currentTime = millis();

  if (currentTime - lastTime >= 6000) {
    int cpm = pulseCount / 2;  // fix double pulse issue
    pulseCount = 0;
    lastTime = currentTime;

    uint32_t timestamp = currentTime / 1000;
    addToBuffer(timestamp, cpm);

    Serial.print("Buffered reading: ");
    Serial.print(timestamp);
    Serial.print("s, ");
    Serial.print(cpm);
    Serial.println(" CPM");

    // Log to SPIFFS
    File spiffsFile = SPIFFS.open("/data.csv", FILE_APPEND);
    if (spiffsFile) {
      spiffsFile.printf("%lu,%u\n", timestamp, cpm);
      spiffsFile.close();
      Serial.println("Saved to SPIFFS");
    } else {
      Serial.println("SPIFFS write failed");
    }

    if (itemCount >= BUFFER_SIZE) {
      transmitBuffer();  // Simulated for now
    }

    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
  }
}
