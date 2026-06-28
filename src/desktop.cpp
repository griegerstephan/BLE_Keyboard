#include <TFT_eSPI.h>  
#include <TFT_Touch.h>
#include <FS.h>
#include <SD.h>
#include <ArduinoJson.h>

#include "config.h"
#include "ui.h"
#include "bluetooth.h"

#define SD_CS_PIN 5 

extern TFT_eSPI tft;
static bool firstBootCompleted = false;
std::vector<Btn> buttons;

void loadProfile(const String& profileName) { 
    buttons.clear();

    File configFile = SD.open("/config.json", FILE_READ); 
    if (!configFile) {
      Serial.println("Error: Could not open config.json from SD");
      return;
    }

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    if (error){ 
      Serial.println("Error: JSON parsing failed");
      return;
    }

    JsonArray profileButtons = doc[profileName];

    for (JsonObject btn : profileButtons) {
        Btn newButton;
        newButton.label = btn["label"].as<String>(); 
        newButton.image = btn["image"].as<String>();
        newButton.target = btn["target"].as<String>(); 

        JsonArray shortcutsArr = btn["steps"];
        for (JsonVariant v : shortcutsArr) {
            newButton.steps.push_back(v.as<String>());
        }

        buttons.push_back(newButton);
    }
}

void desktopDraw() {
  if (!firstBootCompleted && buttons.size() == 0) {
    loadProfile("Defaults");
    firstBootCompleted = true; 
  }

  tft.fillScreen(APP_BACKGROUND); 
  uiDrawButtons(buttons);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("--- TAP HERE FOR MAIN MENU ---", SW / 2, 228, 1);
}

void desktopHandleTouch(int x, int y) {
  // --- INVISIBLE GLOBAL BOTTOM HOME BUTTON ---
  Serial.printf("Touch at X:%d Y:%d\n", x, y); 
  if (y >= 200) {
    loadProfile("Defaults");
    desktopDraw(); 
    return; 
  }

  int pressedIndex = uiGetPressedButtonIndex(x, y);
  
  if (pressedIndex != -1) {
    const auto clickedButton = buttons[pressedIndex];
    
    Serial.println("Button pressed target profile: " + clickedButton.target); 
    
     if (clickedButton.target != "" && clickedButton.target != "null") {
      
      loadProfile(clickedButton.target);
      
      desktopDraw();
      
    } else {
      bleRunAction(clickedButton.steps);
    }
  } 
}

void checkForIncomingWindowsProfile() {
  // Check if Windows dropped any characters down the USB pipeline
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n'); 
    input.trim(); // Wipe out carriage returns \r or stray whitespace

    // Capture our unique protocol token string
    if (input.startsWith("PROFILE:")) {
      String targetProfile = input.substring(8); 
      targetProfile.trim();

      if (targetProfile != "") {
        
        loadProfile(targetProfile);
        tft.fillScreen(APP_BACKGROUND); 
        desktopDraw(); 
      }
    }
  }
}

