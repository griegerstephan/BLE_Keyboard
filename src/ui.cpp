#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include "SPI.h"
#include "SD.h"
#include "config.h"

extern TFT_eSPI tft;

void uiFillBackground(uint16_t backgroundColor) {
  tft.fillScreen(backgroundColor);
}   

static uint16_t read16(File &f) {
  uint16_t r;
  ((uint8_t *)&r)[0] = f.read();
  ((uint8_t *)&r)[1] = f.read();
  return r;
}

static uint32_t read32(File &f) {
  uint32_t r;
  ((uint8_t *)&r)[0] = f.read();
  ((uint8_t *)&r)[1] = f.read();
  ((uint8_t *)&r)[2] = f.read();
  ((uint8_t *)&r)[3] = f.read();
  return r;
}

int uiDrawBMP(const char* filename, int x, int y) {
  File bmp = SD.open(filename);
  if (!bmp) { Serial.printf("Cannot open %s\n", filename); return 0; }

  if (read16(bmp) != 0x4D42) { Serial.printf("%s: not a BMP\n", filename); bmp.close(); return 0; }

  read32(bmp); read32(bmp);
  uint32_t offset = read32(bmp);
  read32(bmp);
  int32_t  w = read32(bmp);
  int32_t  h = read32(bmp);
  uint16_t planes = read16(bmp);
  uint16_t depth  = read16(bmp);
  uint32_t compression = read32(bmp);

  if (planes != 1 || depth != 24 || compression != 0) {
    Serial.printf("%s: need 24-bit uncompressed BMP\n", filename);
    bmp.close();
    return 0;
  }

  uint32_t rowSize = (w * 3 + 3) & ~3;
  static uint16_t lineBuf[480];
  uint8_t rawBuf[480 * 3];

  tft.setSwapBytes(true);   // correct byte order (avoids green tint)
  bmp.seek(offset);
  tft.startWrite();
  for (int i = 0; i < h; i++) {
    bmp.read(rawBuf, rowSize);
    int screenY = y + (h - 1 - i);   // BMP is stored bottom-up
    for (int col = 0; col < w; col++) {
      uint8_t b = rawBuf[col * 3];
      uint8_t g = rawBuf[col * 3 + 1];
      uint8_t r = rawBuf[col * 3 + 2];
      lineBuf[col] = tft.color565(r, g, b);
    }
    tft.pushImage(x, screenY, w, 1, lineBuf);
  }
  tft.endWrite();
  bmp.close();
  return h;
}

void uiDrawButtons(const std::vector<Btn>& buttons) {
  const int btnW = 96;
  const int btnH = 96;
  
  const int startX = 8;   
  const int startY = 16;  
  const int gapX = 8;    
  const int gapY = 16;   
  const int cols = 3;     

  for (size_t i = 0; i < buttons.size(); i++) {
    int col = i % cols;
    int row = i / cols;

    int x = startX + col * (btnW + gapX);
    int y = startY + row * (btnH + gapY);

    uiDrawBMP(buttons[i].image.c_str(), x, y);
  }
}

int uiGetPressedButtonIndex(int touchX, int touchY) {
  const int btnW = 96;
  const int btnH = 96;
  
  // These MUST exactly match the layout numbers used in your drawing loop
  const int startX = 8;   // Left margin
  const int startY = 16;  // Top margin
  const int gapX = 8;     // Space between columns
  const int gapY = 16;    // Space between rows
  const int cols = 3;     // 3 buttons per row

  // Loop through whatever number of buttons are currently in the array
  for (size_t i = 0; i < buttons.size(); i++) {
    int col = i % cols;
    int row = i / cols;

    // Calculate the top-left corner of this specific button
    int x1 = startX + col * (btnW + gapX);
    int y1 = startY + row * (btnH + gapY);

    // Calculate the bottom-right corner of this specific button
    int x2 = x1 + btnW;
    int y2 = y1 + btnH;

    // Check if the touch coordinates fall entirely inside this bounding box
    if (touchX >= x1 && touchX <= x2 && touchY >= y1 && touchY <= y2) {
      return i; // Found it! Return the index (0, 1, 2...)
    }
  }

  return -1; // No button was pressed (touched empty background space)
}
