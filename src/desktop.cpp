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
  Serial.println("Loading default desktop buttons");
  buttons.clear();
  buttons.push_back({ "VS",           "/VSC_Icon.bmp",            {  } });
  buttons.push_back({ "Arduino",        "/Arduino_Icon.bmp",            {  } });
} 

static void setVisualStudioButtons(){
  Serial.println("Loading Visual Studio desktop buttons");
  buttons.clear();
  buttons.push_back({ "Copy",           "/copy.bmp",            { "ctrl+a", "ctrl+c" } });
  buttons.push_back({ "SaveAll",        "/save.bmp",            { "ctrl+k", "s" } });
  buttons.push_back({ "AI",             "/vsai.bmp",            { "ctrl+alt+i" } });
  buttons.push_back({ "Build",          "/vsbuild.bmp",         { "ctrl+alt+b" } });
  buttons.push_back({ "BuildUpload",    "/vsbuildupload.bmp",   { "ctrl+alt+u" } });
  buttons.push_back({ "Serial",         "/vsserial.bmp",        { "ctrl+alt+s" } });
}

void desktopDraw() {
  // Clear the screen and draw the desktop background
  tft.fillScreen(APP_BACKGROUND); 

  if (buttons.size() == 0) {
    setDefaults();
  }

  // Draw the buttons on the screen
  uiDrawButtons(buttons);
}

void desktopHandleTouch(int x, int y) {
  int pressedIndex = uiGetPressedButtonIndex(x, y);
  int numSteps = buttons[pressedIndex].steps.size();
  
  if (pressedIndex != -1) {
    if (buttons[pressedIndex].label == "VS") {
      setVisualStudioButtons();
      desktopDraw();
    } else if (buttons[pressedIndex].label == "Arduino") {
      //switchTo(APP_ARDUINO);
    } else {
      bleRunAction(buttons[pressedIndex].steps);
    }
  }
}
