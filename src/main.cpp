/*
 * CYD Wireless Macro Pad
 * Config-driven BLE keyboard with on-screen touch buttons.
 * Board: Freenove FNK0103 / FNK0114B 2.8" ST7789 (ESP32-WROOM-32E)
 *
 * CONFIG (SD card /config.json)
 *   {
 *     "ble_name":  "CYD Wireless Tool",
 *     "ble_manuf": "Freenove",
 *     "buttons": [
 *       { "label": "Build",        "keys": "ctrl+alt+b" },          // one chord
 *       { "label": "Sel + Copy",   "keys": ["ctrl+a", "ctrl+c"] }   // a sequence
 *     ]
 *   }
 *   - "keys" as a STRING = one chord  ('+' = keys held together)
 *   - "keys" as an ARRAY = a sequence of chords, run in order
 *   - To add a button: add one object to the array. A tile appears for it.
 *
 * CONTROLS
 *   - Tap an on-screen tile   -> runs that button's action.
 *   - TAP the BOOT button     -> runs the FIRST tile (fallback before touch is tuned).
 *   - HOLD the BOOT button 2s -> reboots into CONFIG MODE (WiFi portal).
 *
 * LIBRARIES (platformio.ini lib_deps) - touch is built in, no touch lib needed:
 *   t-vk/ESP32 BLE Keyboard @ ^0.3.2
 *   bodmer/TFT_eSPI @ ^2.5.43
 *   bblanchon/ArduinoJson @ ^7.0.0
 */

#include <Arduino.h>
#include <vector>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <BleKeyboard.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ---------- Pins ----------
#define BUTTON_PIN 0                 // BOOT button
#define SD_SCK 18                    // SD on VSPI
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
#define T_CS 33                      // XPT2046 touch (bit-banged, its own pins)
#define T_CLK 25
#define T_DIN 32
#define T_DOUT 39

// ---------- Touch calibration (tune if taps land on the wrong tile) ----------
int  X_MIN = 200, X_MAX = 3900;      // raw X range
int  Y_MIN = 200, Y_MAX = 3900;      // raw Y range
bool SWAP_XY = true;                 // landscape: raw axes are transposed
bool FLIP_X  = false;                // flip if taps are mirrored left/right
bool FLIP_Y  = false;                // flip if taps are mirrored up/down
const int RAWERR = 255;               // max jitter between two samples to count as a press

// ---------- Config portal AP ----------
const char* AP_SSID = "CYD-Config";
const char* AP_PASS = "";    // >= 8 chars
const char* CONFIG_PATH = "/config.json";
const unsigned long HOLD_MS = 2000;

// ---------- Globals ----------
TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(VSPI);
WebServer server(80);
Preferences prefs;
BleKeyboard* kb = nullptr;
bool sdReady = false;

struct Btn {
  String label;
  std::vector<String> steps;         // each entry is one chord, e.g. "ctrl+a"
};

String bleName  = "CYD Wireless Tool";
String bleManuf = "Freenove";
std::vector<Btn> buttons;

// ---------- Built-in XPT2046 touch reader (software SPI) ----------
// Reads one axis. yAxis=false -> X (cmd 0xD0), yAxis=true -> Y (cmd 0x90).
int readAxis(bool yAxis) {
  uint8_t cmd = yAxis ? 0x90 : 0xD0;
  digitalWrite(T_CS, LOW);
  for (int i = 8; i > 0; i--) {                       // 8-bit command, MSB first
    digitalWrite(T_DIN, (cmd >> (i - 1)) & 1);
    digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW);
  }
  digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW); // 1 busy clock
  int data = 0;
  for (int i = 12; i > 0; i--) {                       // read 12 bits (read, then clock)
    data += digitalRead(T_DOUT) << (i - 1);
    digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW);
  }
  digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW); // 3 flush clocks
  digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW);
  digitalWrite(T_CLK, HIGH); digitalWrite(T_CLK, LOW);
  digitalWrite(T_CS, HIGH);
  digitalWrite(T_DIN, LOW);
  return data;
}

// A touch is real only if two samples agree (stable) and are in range.
bool readTouch(int &sx, int &sy) {
  int xr = readAxis(false);
  int yr = readAxis(true);
  delay(1);
  if (abs(xr - readAxis(false)) > RAWERR) return false;
  if (abs(yr - readAxis(true))  > RAWERR) return false;
  if (!(xr > 0 && xr < 4095 && yr > 0 && yr < 4095)) return false;

  float fx = constrain((xr - X_MIN) / float(X_MAX - X_MIN), 0.0f, 1.0f);
  float fy = constrain((yr - Y_MIN) / float(Y_MAX - Y_MIN), 0.0f, 1.0f);
  if (FLIP_X) fx = 1.0f - fx;
  if (FLIP_Y) fy = 1.0f - fy;
  if (SWAP_XY) { sx = fy * 320; sy = fx * 240; }
  else         { sx = fx * 320; sy = fy * 240; }

  // Calibration helper: uncomment to print raw + mapped in the Serial Monitor
  // Serial.printf("raw=%d,%d -> screen=%d,%d\n", xr, yr, sx, sy);
  return true;
}

// ---------- Defaults ----------
void setDefaults() {
  bleName = "CYD Wireless Tool";
  bleManuf = "Freenove";
  buttons.clear();
  buttons.push_back({ "Build",        { "ctrl+alt+b" } });
  buttons.push_back({ "Build+Upload", { "ctrl+alt+u" } });
  buttons.push_back({ "Sel + Copy",   { "ctrl+a", "ctrl+c" } });
  buttons.push_back({ "Save",         { "ctrl+s" } });
}

// ---------- SD ----------
bool mountSD() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI);
  return sdReady;
}

String readConfigRaw() {
  if (!sdReady) return "";
  if (!SD.exists(CONFIG_PATH)) return "";         // avoids the noisy "does not exist" log
  File f = SD.open(CONFIG_PATH, FILE_READ);
  if (!f) return "";
  String s;
  while (f.available()) s += (char)f.read();
  f.close();
  return s;
}

void loadConfig() {
  setDefaults();
  String raw = readConfigRaw();
  if (raw.length() == 0) return;
  JsonDocument doc;
  if (deserializeJson(doc, raw)) return;          // bad JSON -> keep defaults

  const char* n = doc["ble_name"]  | "";  if (n[0]) bleName  = n;
  const char* m = doc["ble_manuf"] | "";  if (m[0]) bleManuf = m;

  if (doc["buttons"].is<JsonArray>()) {
    std::vector<Btn> tmp;
    for (JsonObject o : doc["buttons"].as<JsonArray>()) {
      Btn b;
      b.label = o["label"] | "Btn";
      JsonVariant k = o["keys"];
      if (k.is<JsonArray>()) {
        for (JsonVariant v : k.as<JsonArray>()) {
          const char* s = v | "";
          if (s[0]) b.steps.push_back(String(s));
        }
      } else {
        const char* s = k | "";
        if (s[0]) b.steps.push_back(String(s));
      }
      if (!b.steps.empty()) tmp.push_back(b);
    }
    if (!tmp.empty()) buttons = tmp;               // empty/invalid -> keep defaults
  }
}

bool writeConfigRaw(const String& body) {
  if (!sdReady) return false;
  File f = SD.open("/config.tmp", FILE_WRITE);
  if (!f) return false;
  f.print(body);
  f.close();
  SD.remove(CONFIG_PATH);
  return SD.rename("/config.tmp", CONFIG_PATH);
}

// ---------- Hotkey parsing & sending ----------
uint8_t namedKey(const String& k) {
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
  if (k.length() >= 2 && k[0] == 'f') {
    int n = k.substring(1).toInt();
    if (n >= 1 && n <= 12) return KEY_F1 + (n - 1);
  }
  return 0;
}

// Sends one chord, e.g. "ctrl+alt+b" (all keys held together, then released).
void sendChord(const String& chord) {
  if (!kb || !kb->isConnected()) return;
  String s = chord; s.toLowerCase();
  String finalKey = "";
  int start = 0;
  while (true) {
    int plus = s.indexOf('+', start);
    String tok = (plus == -1) ? s.substring(start) : s.substring(start, plus);
    tok.trim();
    if      (tok == "ctrl" || tok == "control")            kb->press(KEY_LEFT_CTRL);
    else if (tok == "alt")                                 kb->press(KEY_LEFT_ALT);
    else if (tok == "shift")                               kb->press(KEY_LEFT_SHIFT);
    else if (tok == "win" || tok == "gui" || tok == "cmd") kb->press(KEY_LEFT_GUI);
    else if (tok.length() > 0)                             finalKey = tok;
    if (plus == -1) break;
    start = plus + 1;
  }
  if (finalKey.length() == 1)      kb->press(finalKey[0]);
  else if (finalKey.length() > 1) { uint8_t nk = namedKey(finalKey); if (nk) kb->press(nk); }
  delay(80);
  kb->releaseAll();
}

// Runs a button: one chord, or a sequence of chords in order.
void runAction(const std::vector<String>& steps) {
  for (size_t i = 0; i < steps.size(); i++) {
    sendChord(steps[i]);
    if (i + 1 < steps.size()) delay(120);          // gap between sequence steps
  }
}

// ---------- Touch button grid ----------
const int TH = 34;                                  // header height
int gCols = 2, gRows = 1, gW = 160, gH = 206;

uint16_t tileColor(int i) {
  static const uint16_t c[] = { TFT_BLUE, TFT_DARKGREEN, TFT_MAROON,
                                TFT_PURPLE, TFT_OLIVE, TFT_NAVY };
  return c[i % 6];
}

void computeGrid() {
  int n = buttons.size(); if (n < 1) n = 1;
  gCols = (n <= 1) ? 1 : 2;
  gRows = (n + gCols - 1) / gCols;
  gW = 320 / gCols;
  gH = (240 - TH) / gRows;
}

void drawTile(int i, bool pressed) {
  int col = i % gCols, row = i / gCols;
  int x = col * gW, y = TH + row * gH;
  uint16_t c = pressed ? TFT_WHITE : tileColor(i);
  tft.fillRoundRect(x + 3, y + 3, gW - 6, gH - 6, 6, c);
  tft.drawRoundRect(x + 3, y + 3, gW - 6, gH - 6, 6, TFT_WHITE);
  tft.setTextColor(pressed ? TFT_BLACK : TFT_WHITE);
  tft.setTextSize(2);
  int tw = buttons[i].label.length() * 12;          // ~12 px/char at size 2
  int tx = x + (gW - tw) / 2; if (tx < x + 6) tx = x + 6;
  tft.setCursor(tx, y + gH / 2 - 8);
  tft.print(buttons[i].label);
}

void drawHeader(bool conn) {
  tft.fillRect(0, 0, 320, TH, conn ? TFT_DARKGREEN : TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 8);
  tft.print(conn ? "BLE connected" : "BLE waiting...");
}

void drawUI(bool conn) {
  tft.fillScreen(TFT_BLACK);
  drawHeader(conn);
  for (size_t i = 0; i < buttons.size(); i++) drawTile(i, false);
}

int hitTest(int px, int py) {
  if (py < TH) return -1;
  int col = px / gW, row = (py - TH) / gH;
  int idx = row * gCols + col;
  if (col < 0 || col >= gCols || idx < 0 || idx >= (int)buttons.size()) return -1;
  return idx;
}

// ---------- Config portal ----------
void handleRoot() {
  String c = readConfigRaw();
  if (c.length() == 0) {
    c = "{\n  \"ble_name\": \"CYD Wireless Tool\",\n"
        "  \"ble_manuf\": \"Freenove\",\n"
        "  \"buttons\": [\n"
        "    { \"label\": \"Build\",        \"keys\": \"ctrl+alt+b\" },\n"
        "    { \"label\": \"Build+Upload\", \"keys\": \"ctrl+alt+u\" },\n"
        "    { \"label\": \"Sel + Copy\",   \"keys\": [\"ctrl+a\", \"ctrl+c\"] },\n"
        "    { \"label\": \"Save\",         \"keys\": \"ctrl+s\" }\n"
        "  ]\n}\n";
  }
  c.replace("&", "&amp;");
  c.replace("<", "&lt;");
  String html =
    "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;margin:16px}"
    "textarea{width:100%;height:55vh;font-family:monospace;font-size:14px}"
    "button{padding:10px 16px;margin-top:8px;font-size:16px}code{background:#eee;padding:1px 4px}</style>"
    "<h2>CYD Macro Pad &mdash; config</h2>"
    "<p>Each button: <code>label</code> + <code>keys</code>. "
    "<code>keys</code> is one chord (<code>ctrl+alt+b</code>) or a sequence "
    "(<code>[\"ctrl+a\",\"ctrl+c\"]</code>).</p>"
    "<form method=POST action=/save>"
    "<textarea name=cfg>" + c + "</textarea><br>"
    "<button>Save &amp; reboot</button></form>";
  if (!sdReady)
    html += "<p style='color:#c00'>WARNING: SD card not detected &mdash; saving will fail.</p>";
  server.send(200, "text/html", html);
}

void handleSave() {
  String body = server.arg("cfg");
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "text/html", "Invalid JSON, not saved. <a href=/>back</a>");
    return;
  }
  if (writeConfigRaw(body)) {
    server.send(200, "text/html", "Saved. Rebooting...");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/html", "SD write failed. <a href=/>back</a>");
  }
}

void runConfigMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3); tft.setCursor(10, 20);  tft.print("CONFIG MODE");
  tft.setTextSize(2);
  tft.setCursor(10, 90);  tft.print(String("WiFi: ") + AP_SSID);
  tft.setCursor(10, 120); tft.print(String("pass: ") + AP_PASS);
  tft.setCursor(10, 160); tft.print(String("http://") + ip.toString());
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  for (;;) { server.handleClient(); delay(2); }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Touch pins (bit-banged software SPI)
  pinMode(T_CS, OUTPUT);  digitalWrite(T_CS, HIGH);
  pinMode(T_CLK, OUTPUT); digitalWrite(T_CLK, LOW);
  pinMode(T_DIN, OUTPUT); digitalWrite(T_DIN, LOW);
  pinMode(T_DOUT, INPUT);

  tft.init();                       // backlight (GPIO 21) on
  tft.setRotation(1);

  mountSD();

  prefs.begin("cyd", false);
  bool wantConfig = prefs.getBool("cfgmode", false);
  prefs.putBool("cfgmode", false);
  prefs.end();
  if (wantConfig) runConfigMode();  // never returns

  loadConfig();
  computeGrid();

  kb = new BleKeyboard(std::string(bleName.c_str()),
                       std::string(bleManuf.c_str()), 100);
  kb->begin();
  drawUI(false);
}

// ---------- Loop ----------
int lastButtonState = HIGH;
unsigned long pressStart = 0;
bool configArmed = false;
bool lastConnected = false;
bool touchLatched = false;

void loop() {
  bool conn = kb && kb->isConnected();
  if (conn != lastConnected) { drawHeader(conn); lastConnected = conn; }

  // ----- Touchscreen tiles (fire once per touch) -----
  int sx, sy;
  if (readTouch(sx, sy)) {
    if (!touchLatched) {
      int idx = hitTest(sx, sy);
      if (idx >= 0) {
        drawTile(idx, true);
        runAction(buttons[idx].steps);
        delay(120);
        drawTile(idx, false);
      }
      touchLatched = true;
    }
  } else {
    touchLatched = false;
  }

  // ----- BOOT button: tap = first tile, hold 2s = config mode -----
  int btn = digitalRead(BUTTON_PIN);
  if (btn == LOW && lastButtonState == HIGH) { pressStart = millis(); configArmed = false; }

  if (btn == LOW && !configArmed && millis() - pressStart > HOLD_MS) {
    configArmed = true;
    tft.fillScreen(TFT_NAVY);
    tft.setTextColor(TFT_WHITE); tft.setTextSize(3);
    tft.setCursor(10, 90); tft.print("Config...");
    prefs.begin("cyd", false); prefs.putBool("cfgmode", true); prefs.end();
    delay(400);
    ESP.restart();
  }

  if (btn == HIGH && lastButtonState == LOW) {
    unsigned long held = millis() - pressStart;
    if (held > 40 && held < HOLD_MS && !buttons.empty()) runAction(buttons[0].steps);
  }

  lastButtonState = btn;
  delay(10);
}