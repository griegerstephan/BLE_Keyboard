# BLE_Keyboard

A Bluetooth shortcut keyboard with an LCARS-style touch screen, for the CYD 240x320 ESP32 module.

The screen shows buttons for the app you're using. Tapping one sends its keyboard shortcut to the PC over Bluetooth. The App Detector on the PC tells the device which app is in front, so the buttons follow you as you switch apps.

### Screen layout

- **Header:** the current profile name.
- **Sidebar**, the same on every screen:
  - **FLIP** (top elbow): turns the screen upside down. The choice survives a reboot.
  - **SNIP:** Windows screenshot (`win+shift+s`).
  - **DESK:** show desktop (`win+d`).
  - **HOME:** back to the main menu.
- **Footer:** `BLE LINK` when the PC is connected, or red `NO LINK` when it isn't.
- **Buttons:** up to 3 buttons show as wide pills. Up to 8 show in a 2-column grid.

### Configuring buttons

Buttons live in `config.json` on the SD card; a copy is in `src/assets/config.json`. Each profile is a list of buttons:

```json
"VisualStudio": [
  { "label": "Build", "steps": ["ctrl+alt+b"] },
  { "label": "Copy", "steps": ["ctrl+a", "ctrl+c"], "color": "#CC6666" }
]
```

- **`label`** is shown in uppercase, and CamelCase is split into words, so `SaveAll` shows as `SAVE ALL`. In the 2-column grid, labels over 98px fall back to a smaller, non-bold font, and a warning is printed to Serial.
- **`steps`** are sent in order. Each step is one chord, such as `ctrl+shift+m`, `win+d`, `enter` or `f5`.
- **`target`** opens another profile instead of sending keys. The main menu uses this.
- **`color`** is optional. Without it, a colour is picked from the LCARS palette.

`Defaults` is the main menu. Apps that the App Detector reports without a matching profile also show the main menu.

### App Detector

`src/assets/App Detector/app_detector.ahk` (compiled as `BLE-Keyboard.exe`) watches the foreground app. It sends `PROFILE:<name>` to the device over USB serial. Close it before uploading firmware, because it holds the COM port, and restart it afterwards.

### Troubleshooting

**The screen says BLE LINK but no keys arrive:** the ESP32 is probably holding stale pairing keys. Erase the flash, re-upload, then remove and re-pair the keyboard in Windows (PIN 260368):

```
platformio run -t erase
```

**Windows keeps rejecting pairing:** change `BLE_ADDRESS_ID` in `src/config.h`. This gives the board a new Bluetooth address, so Windows treats it as a brand-new device.

### Known issues
1. Pressing a button can send it more than once

### Next steps
1. Edit `config.json` on the PC and sync it to the device over USB, so the SD card doesn't need removing
2. Track down and fix the multiple press bug
