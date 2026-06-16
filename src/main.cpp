#include <Arduino.h>
#include <BleKeyboard.h>

// Name that will show up in your Windows Bluetooth settings
BleKeyboard bleKeyboard("CYD Wireless Tool", "Freenove", 100);

// CYD onboard BOOT button is connected to GPIO 0
const int BUTTON_PIN = 0; 
int lastButtonState = HIGH;

void setup() {
  // Start the BLE keyboard service
  bleKeyboard.begin();

  // Configure the built-in boot button with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Read the current state of the button
  int buttonState = digitalRead(BUTTON_PIN);

  // Check if the board is paired and connected to Windows
  if(bleKeyboard.isConnected()) {
    
    // Check if the button was pressed (LOW means pressed)
    if (buttonState == LOW && lastButtonState == HIGH) {
      delay(50); // Simple debounce delay to prevent double-triggering

      // Press and hold CTRL, then press 'a'
      bleKeyboard.press(KEY_LEFT_CTRL);
      bleKeyboard.press('a');
      delay(100); // Small pause to let Windows register the combination
      
      // Release all keys immediately
      bleKeyboard.releaseAll();
    }
  }
  
  // Save the current state for the next loop iteration
  lastButtonState = buttonState;
}
