#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include "SPI.h"
#include "config.h"
#include "ui.h"
#include "bluetooth.h"

// Use the tft object to draw to the screen
extern TFT_eSPI tft;

std::vector<Btn> buttons; // the buttons to show on the desktop, loaded from config.json

// Set the startup app to show
AppId currentApp = APP_DESKTOP;

static void setDefaults() {
  buttons.clear();
  buttons.push_back({ "VS",           "/VSC_Icon.bmp",            { "ctrl" } });
  buttons.push_back({ "Arduino",        "/Arduino_Icon.bmp",      { "ctrl" } });
} 
 
static void setVisualStudioButtons(){
  buttons.clear();
  buttons.push_back({ "Copy",           "/copy.bmp",            { "ctrl+a", "ctrl+c" } });
  buttons.push_back({ "SaveAll",        "/save.bmp",            { "ctrl+k", "s" } });
  buttons.push_back({ "AI",             "/vsai.bmp",            { "ctrl+alt+i" } });
  buttons.push_back({ "Build",          "/vsbuild.bmp",         { "ctrl+alt+b" } });
  buttons.push_back({ "BuildUpload",    "/vsbuildupload.bmp",   { "ctrl+alt+u" } });
  buttons.push_back({ "Serial",         "/vsserial.bmp",        { "ctrl+alt+s" } });
}

static void setArduinoButtons(){
  buttons.clear();
  buttons.push_back({ "Copy",           "/arduinocopy.bmp",             { "ctrl+a", "ctrl+c" } });
  buttons.push_back({ "SaveAll",        "/arduinosave.bmp",             { "ctrl+s" } });
  buttons.push_back({ "Build",          "/arduinobuild.bmp",            { "ctrl+r" } });
  buttons.push_back({ "BuildUpload",    "/arduinoupload.bmp",           { "ctrl+u" } });
  buttons.push_back({ "Serial",         "/arduinoserial.bmp",           { "ctrl+shift+m" } });
  
}

void desktopDraw() {
  if (buttons.size() == 0) {
    setDefaults();
  }

  // Draw the buttons on the screen
  uiDrawButtons(buttons);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("--- TAP HERE FOR MAIN MENU ---", SW / 2, 228, 1);
}


void desktopHandleTouch(int x, int y) {
  // --- INVISIBLE GLOBAL BOTTOM HOME BUTTON ---
  Serial.printf("Touch at X:%d Y:%d\n", x, y); // Debug: Print touch coordinates to Serial Monitor
  if (y >= 200) {
    setDefaults(); // Reset to default desktop buttons
    tft.fillScreen(APP_BACKGROUND); // Clear the screen to the app background color 
    desktopDraw(); // Redraw the desktop with default buttons
    return; 
  }

  // Your original working button processing logic remains completely untouched below
  int pressedIndex = uiGetPressedButtonIndex(x, y);
  
  if (pressedIndex != -1) {
    if (buttons[pressedIndex].label == "VS") {
      setVisualStudioButtons();
      tft.fillScreen(APP_BACKGROUND);
      desktopDraw();
    } else if (buttons[pressedIndex].label == "Arduino") {
      setArduinoButtons();
      tft.fillScreen(ARDUINO); // Change background color to match Arduino palette
      desktopDraw();
    } else {
      bleRunAction(buttons[pressedIndex].steps);
    }
  }
}

