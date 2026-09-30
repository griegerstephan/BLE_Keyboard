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
static String currentProfile = "Defaults";
std::vector<Btn> buttons;

// Parses "#RRGGBB" into RGB565; returns 0 (use the palette) if missing or invalid
static uint16_t parseColor(const String& hex) {
    if (hex.length() != 7 || hex[0] != '#') return 0;
    uint32_t rgb = strtoul(hex.c_str() + 1, nullptr, 16);
    return rgb565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void loadProfile(const String& profileName) {
    buttons.clear();
    currentProfile = profileName;

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

    // Apps without a profile in config.json fall back to the main menu instead of a blank screen
    if (profileButtons.isNull() && profileName != "Defaults") {
      Serial.println("No profile for " + profileName + ", showing Defaults");
      currentProfile = "Defaults";
      profileButtons = doc["Defaults"];
    }

    for (JsonObject btn : profileButtons) {
        Btn newButton;
        newButton.label = btn["label"].as<String>(); 
        newButton.color = parseColor(btn["color"] | "");
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

  uiDrawScreen(currentProfile, buttons);
}

void desktopHandleTouch(int x, int y) {
  // --- LCARS HOME BUTTON (bottom of the sidebar) ---
  Serial.printf("Touch at X:%d Y:%d\n", x, y);

  // --- LCARS FLIP BUTTON (turns the screen upside down) ---
  if (uiIsFlipPressed(x, y)) {
    uiToggleRotation();
    desktopDraw();
    return;
  }

  if (uiIsHomePressed(x, y)) {
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
        desktopDraw();
      }
    }
  }
}

