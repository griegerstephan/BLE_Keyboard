#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>

// Changing this gives the board a new Bluetooth address (Windows treats it as a new device)
#define BLE_ADDRESS_ID 0x05

// Screen dimensions (landscape, rotation 1)
#define SW 320
#define SH 240
 
// Pack 8-bit RGB into the display's 16-bit RGB565 format
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Define project palettes
#define APP_BACKGROUND      TFT_BLACK

// LCARS palette
#define LCARS_ORANGE        rgb565(0xFF, 0x99, 0x00)
#define LCARS_PEACH         rgb565(0xFF, 0xCC, 0x99)
#define LCARS_LAVENDER      rgb565(0xCC, 0x99, 0xCC)
#define LCARS_BLUE          rgb565(0x99, 0x99, 0xFF)
#define LCARS_RED           rgb565(0xCC, 0x66, 0x66)
#define LCARS_TAN           rgb565(0xFF, 0xAA, 0x90)
#define LCARS_SKY           rgb565(0x99, 0xCC, 0xFF)
#define LCARS_APRICOT       rgb565(0xFF, 0x99, 0x66)

// SD card pins (VSPI - separate bus from the TFT/touch on HSPI)
#define SD_CS   5
#define SD_SCK  18
#define SD_MISO 19 
#define SD_MOSI 23
#define T_DOUT 39  // MISO / Data out
#define T_DIN  32  // MOSI / Data in
#define T_DCS  33  // Chip Select (TOUCH_CS)
#define T_DCLK 25  // Clock pin

// Define the button structure so every file knows what a 'Btn' is
struct Btn {
  String label;
  uint16_t color = 0;        // RGB565 button colour; 0 = pick from the LCARS palette
  std::vector<String> steps; // each entry is one chord, e.g. "ctrl+a"
  String target; // NEW: Holds the profile name to load next (e.g., "VisualStudio")
};

// EXTERN tells other files this vector exists globally, without creating it yet
extern std::vector<Btn> buttons;

#endif