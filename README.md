# BLE_Keyboard

This code is designed for the CYD 240x320 ESP32 Module.

### Known Issues
1. Pressing a button can send it more than once
3. Adding commands requires a code change

### Next steps (in order)
2. Will add a Web feature to be able to customise the button images, keys and pages and save to the SD card
3. Will track down and kill multiple press bug



1. Add the File Reader FunctionAdd this helper function to your SD card module or main.cpp. It opens a file called /config.json, reads it into a string, and handles errors gracefully if the card is missing or corrupted.cpp#include "FS.h"
#include "SD.h"

String readConfigRaw() {
  // Check if the SD card hardware is actually mounted
  if (!sdReady) {
    Serial.println("SD Card not ready. Skipping JSON read.");
    return "";
  }

  File file = SD.open("/config.json", FILE_READ);
  if (!file) {
    Serial.println("Failed to open /config.json! Using hardcoded defaults.");
    return "";
  }

  String contents = "";
  contents.reserve(file.size()); // Pre-allocate memory for speed
  while (file.available()) {
    contents += (char)file.read();
  }
  file.close();
  return contents;
}
Use code with caution.2. Update setup() to Load the FileInstead of calling setDefaults() inside setup(), call your macropadInit() function. This will automatically check the SD card first, and seamlessly fall back to your hardcoded buttons if the file isn't found.cppvoid setup() {
  // ... your screen and SD card initialization code ...
  
  // Bring up SD card interface tracks first
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI, 20000000);

  // --- LOAD DYNAMIC FILE HERE ---
  macropadInit(); // Reads SD card; falls back to defaults if missing!
  
  // ... your Bluetooth setup, latch checks, and releaseAll() code ...

  switchTo(APP_DESKTOP); // Boot into the home layout
}
Use code with caution.Now you can create a config.json file on your SD card, drop your .bmp images onto it, and your code never needs to be flashed again to add macros!