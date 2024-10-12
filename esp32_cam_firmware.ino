#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"

#define SD_CS_PIN 13  // ESP32-CAM SD card chip select pin

void setup() {
  // Start serial communication for debugging
  Serial.begin(115200);
  delay(1000); // Optional delay for stability

  // Initialize SD card
  if (!SD_MMC.begin(SD_CS_PIN, SPI, 20000000)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Open the config.txt file
  File file = SD.open("/config.txt");
  if (!file) {
    Serial.println("Failed to open config.txt");
    return;
  }

  // Read 4 lines from the file
  int lineNumber = 0;
  String lineData;
  
  while (file.available()) {
    lineData = file.readStringUntil('\n');  // Read until the end of the line
    lineData.trim();  // Remove any extra spaces or newline characters

    lineNumber++;
    Serial.print("Line ");
    Serial.print(lineNumber);
    Serial.print(": ");
    Serial.println(lineData);

    // Break the loop once 4 lines are read
    if (lineNumber >= 4) {
      break;
    }
  }

  // Close the file
  file.close();
}

void loop() {
  // Nothing to do here
}
