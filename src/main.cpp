#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include <NimBLEDevice.h>
#include <BleKeyboard.h>
#include "SPI.h"
#include "SD.h"

// Project libraries
#include "config.h"         // configuration settings and constants
#include "ui.h"             // UI drawing functions and utilities
#include "desktop.h"        // desktop app code 

// Prototypes to link files together cleanly
extern void checkForIncomingWindowsProfile();
extern void desktopHandleTouch(int x, int y);
extern void desktopDraw();

TFT_eSPI tft = TFT_eSPI();
TFT_Touch touch = TFT_Touch(T_DCS, T_DCLK, T_DIN, T_DOUT);
BleKeyboard* kb = nullptr;

// SD Card settings
SPIClass sdSPI(VSPI);  // SD card lives on its own VSPI bus
bool sdReady = false;  // did the SD card mount?

String bleName  = "Shortcut Keyboard"; 
String bleManuf = "Griegs";
 
void setup(){
  Serial.begin(115200);
  delay(300);

  // Set up the screen and touch controller
  tft.init();
  uiFillBackground(TFT_RED); // Fill the background with red to show it's working
  uiLoadRotation(); // Normal or flipped landscape, as last set by the FLIP button

  // --- ADDED TEXT DISPLAY CODE ---
  tft.setTextColor(TFT_WHITE);                 // Set font colour to white
  tft.setTextSize(2);                          // Set font size to 2
  tft.drawString("Waiting for Bluetooth", 10, 10); // Print message at X:10, Y:10
  // -------------------------------
  
  // Initialize the touch controller and define its screen boundary limits
  touch.setResolution(SW, SH); 
  touch.setRotation(1); // Match your TFT rotation (1)
  touch.setCal(300, 3900, 300, 3800, SW, SH, 1);  

  // Bring up the SD card on its own VSPI bus
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI, 20000000);

  // Give the board its own Bluetooth address so Windows sees it as a new device,
  // separate from any older pairing it cached for this board's factory address.
  // Change BLE_ADDRESS_ID in config.h to force a fresh identity again.
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  mac[0] = 0x02;                 // locally administered, unicast
  mac[5] ^= BLE_ADDRESS_ID;
  esp_base_mac_addr_set(mac);

  // Set up BLE keyboard
  NimBLEDevice::init(std::string(bleName.c_str()));
  NimBLEDevice::setSecurityAuth(true, true, true);            // bonding, MITM protection, secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);     // Forces the client PC to ask for a PIN

  // Custom 6-digit PIN code
  NimBLEDevice::setSecurityPasskey(260368);

  // Set up BLE keyboard (Uses the BLE initialization we just secured)
  kb = new BleKeyboard(std::string(bleName.c_str()),
                       std::string(bleManuf.c_str()), 100);
  kb->begin();
  kb->releaseAll();
  
  uiFillBackground(APP_BACKGROUND); // Fill the background with the app's background color
  desktopDraw(); // Draw the desktop UI
}

void loop() {
  checkForIncomingWindowsProfile();

  // Update the sidebar whenever the Bluetooth connection comes or goes
  static bool lastBleConnected = false;
  bool bleConnected = kb->isConnected();
  if (bleConnected != lastBleConnected) {
    Serial.println(bleConnected ? "BLE connected" : "BLE disconnected");
    uiDrawBleStatus(bleConnected);
    lastBleConnected = bleConnected;
  }

  // Track whether the screen was pressed in the previous frame
  static bool wasPressedLastFrame = false;

  if (touch.Pressed()) {
    // Only execute the touch action ONCE at the moment your finger first hits the glass
    if (!wasPressedLastFrame) {
      int x = touch.X();
      int y = touch.Y();
      uiMapTouch(x, y); // Follow the screen if it has been flipped

      // Fire the handler exactly once
      desktopHandleTouch(x, y);

      // Lock the system until the finger is lifted
      wasPressedLastFrame = true;
    }
  } 
  else {
    // Finger has completely left the screen, unlock the system for the next tap
    if (wasPressedLastFrame) {
      delay(50); // Small hardware debounce delay to let the electrical signals settle
      wasPressedLastFrame = false;
    }
  }
}
