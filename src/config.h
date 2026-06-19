#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>

// Screen dimensions (landscape, rotation 1)
#define SW 320
#define SH 240

// Define project palettes
#define APP_BACKGROUND      TFT_BLACK

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
  String image;              // 48x48 BMP path on the SD card
  std::vector<String> steps; // each entry is one chord, e.g. "ctrl+a"
  String target; // NEW: Holds the profile name to load next (e.g., "VisualStudio")
};

// EXTERN tells other files this vector exists globally, without creating it yet
extern std::vector<Btn> buttons;

#endif