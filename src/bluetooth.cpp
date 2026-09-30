#include <BleKeyboard.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"

extern BleKeyboard* kb;

// -------------------------------------------------------
// Hotkey parsing & sending
// -------------------------------------------------------
static uint8_t namedKey(const String& k) {
  if (k == "enter" || k == "return") return KEY_RETURN;
  if (k == "tab")                    return KEY_TAB;
  if (k == "esc")                    return KEY_ESC;
  if (k == "space")                  return ' ';
  if (k == "backspace")              return KEY_BACKSPACE;
  if (k == "delete" || k == "del")   return KEY_DELETE;
  if (k == "up")                     return KEY_UP_ARROW;
  if (k == "down")                   return KEY_DOWN_ARROW;
  if (k == "left")                   return KEY_LEFT_ARROW;
  if (k == "right")                  return KEY_RIGHT_ARROW;
  if (k == "home")                   return KEY_HOME;
  if (k == "end")                    return KEY_END;
  
  // --- ADDED WINDOWS / GUI KEY SUPPORT ---
  if (k == "win" || k == "windows" || k == "gui") return KEY_LEFT_GUI;

  if (k.length() >= 2 && k[0] == 'f') {
    int n = k.substring(1).toInt();

    if (n >= 1 && n <= 12) {
      return KEY_F1 + (n - 1);
    }
  }
  return 0;
}

// Sends the given chord string (e.g. "ctrl+alt+del") as a single keypress event, then releases all keys after a short delay.
static void sendChord(const String& chord) {
  if (!kb || !kb->isConnected()) return;
  
  // 1. Force explicit local instantiation to isolate fresh RAM tracks
  String s = chord; 
  s.toLowerCase();
  String finalKey = "";
  finalKey.reserve(16); // Pre-allocate heap space to prevent pointer shifting
  
  int start = 0;
  while (true) {
    int plus = s.indexOf('+', start);
    String tok = (plus == -1) ? s.substring(start) : s.substring(start, plus);
    tok.trim();
    
    if      (tok == "ctrl" || tok == "control")            kb->press(KEY_LEFT_CTRL);
    else if (tok == "alt")                                 kb->press(KEY_LEFT_ALT);
    else if (tok == "shift")                               kb->press(KEY_LEFT_SHIFT);
    else if (tok == "win" || tok == "windows" || tok == "gui" || tok == "cmd") kb->press(KEY_LEFT_GUI);
    else if (tok.length() > 0) {
      finalKey = tok; // Safely lock the character token
    }
    
    if (plus == -1) break;
    start = plus + 1;
  }
  
  // 2. Strict execution block with forced type verification
  if (finalKey.length() == 1) {
    char cleanChar = finalKey.charAt(0);
    kb->press((uint8_t)cleanChar);
  }
  else if (finalKey.length() > 1) { 
    uint8_t nk = namedKey(finalKey); 
    if (nk != 0) {
      kb->press(nk); 
    }
  }
  
  delay(80);
  kb->releaseAll();
  delay(10); // Small cooldown safety pause before the next chord step tracks
}

// Executes a sequence of chord steps with a short delay between each step (e.g. for "ctrl+alt+del, win+r, notepad+enter")
void bleRunAction(std::vector<String> steps) {
  if (!kb || !kb->isConnected()) {
    Serial.println("BLE not connected - shortcut not sent");
    return;
  }

  for (size_t i = 0; i < steps.size(); i++) {
    Serial.println("Sending: " + steps[i]);
    sendChord(steps[i]);
    if (i + 1 < steps.size()) delay(120);
  }
}

