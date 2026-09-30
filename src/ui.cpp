#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include "SPI.h"
#include <Preferences.h>
#include "config.h"
#include "ui.h"

extern TFT_eSPI tft;

// Screen orientation: rotation 1 (normal landscape) or 3 (landscape, upside down)
static bool screenFlipped = false;

// --- LCARS frame geometry (320x240, rotation 1) ---
const int SIDEBAR_W    = 58;   // width of the left sidebar
const int TOPBAR_H     = 16;   // height of the top bar
const int BOTBAR_Y     = 224;  // top edge of the bottom bar
const int OUTER_R_TOP  = 24;   // outer radius of the top elbow
const int OUTER_R_BOT  = 16;   // outer radius of the bottom elbow
const int INNER_R      = 13;   // inner (concave) radius of both elbows

// Sidebar blocks
const int FLIP_Y = 98;
const int FLIP_H = 50;
const int HOME_Y = 151;
const int HOME_H = 44;

// --- Button area geometry ---
// <= 3 buttons: one column of wide pills. Otherwise: two columns of smaller pills.
const int AREA_X      = 68;
const int WIDE_Y      = 30;
const int WIDE_W      = 240;
const int WIDE_H      = 48;
const int WIDE_STEP_Y = 62;
const int GRID_Y      = 28;
const int GRID_W      = 118;
const int GRID_H      = 38;
const int GRID_STEP_X = 126;
const int GRID_STEP_Y = 48;
const int GRID_COLS   = 2;
const int GRID_MAX    = 8;     // 4 rows x 2 columns fit between the bars

// Colours cycled through for buttons that don't set their own
static const uint16_t BUTTON_PALETTE[] = {
  LCARS_ORANGE, LCARS_LAVENDER, LCARS_BLUE, LCARS_PEACH,
  LCARS_RED, LCARS_TAN, LCARS_SKY, LCARS_APRICOT
};
static const int PALETTE_SIZE = sizeof(BUTTON_PALETTE) / sizeof(BUTTON_PALETTE[0]);

void uiFillBackground(uint16_t backgroundColor) {
  tft.fillScreen(backgroundColor);
}

// Restores the orientation saved by the FLIP button
void uiLoadRotation() {
  Preferences prefs;
  prefs.begin("ui", true);
  screenFlipped = prefs.getBool("flipped", false);
  prefs.end();
  tft.setRotation(screenFlipped ? 3 : 1);
}

void uiToggleRotation() {
  screenFlipped = !screenFlipped;
  tft.setRotation(screenFlipped ? 3 : 1);

  Preferences prefs;
  prefs.begin("ui", false);
  prefs.putBool("flipped", screenFlipped);
  prefs.end();
}

// Converts raw touch coordinates (always in rotation 1) to screen coordinates
void uiMapTouch(int& x, int& y) {
  if (screenFlipped) {
    x = SW - 1 - x;
    y = SH - 1 - y;
  }
}

// Turns "BuildUpload" into "BUILD UPLOAD" and "claude.exe" into "CLAUDE"
String uiPrettyName(const String& name) {
  String src = name;
  if (src.endsWith(".exe")) src = src.substring(0, src.length() - 4);

  String out;
  for (size_t i = 0; i < src.length(); i++) {
    char c = src[i];
    if (i > 0 && isupper(c) && islower(src[i - 1])) out += ' ';
    out += (char)toupper(c);
  }
  return out;
}

static bool bleConnected = false;

// Lavender sidebar block showing whether the PC is connected over Bluetooth
void uiDrawBleStatus(bool connected) {
  bleConnected = connected;
  tft.fillRect(0, 61, SIDEBAR_W, 34, connected ? LCARS_LAVENDER : LCARS_RED);
  tft.setFreeFont(nullptr);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(connected ? "BLE LINK" : "NO LINK", SIDEBAR_W - 5, 92);
  tft.setTextDatum(TL_DATUM);
}

static bool useWideLayout(size_t count) {
  return count <= 3;
}

// Screen rectangle of button i. Shared by drawing and touch detection so they can't drift apart.
static void buttonRect(size_t i, size_t count, int& x, int& y, int& w, int& h) {
  if (useWideLayout(count)) {
    x = AREA_X;
    y = WIDE_Y + i * WIDE_STEP_Y;
    w = WIDE_W;
    h = WIDE_H;
  } else {
    x = AREA_X + (i % GRID_COLS) * GRID_STEP_X;
    y = GRID_Y + (i / GRID_COLS) * GRID_STEP_Y;
    w = GRID_W;
    h = GRID_H;
  }
}

static size_t visibleButtonCount(size_t count) {
  if (useWideLayout(count)) return count;
  return count > GRID_MAX ? GRID_MAX : count;
}

// Right-aligned black label in a small free font
static void drawBlockLabel(const char* text, int rightX, int bottomY) {
  tft.setFreeFont(nullptr);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(text, rightX, bottomY);
}

static void drawFrame(const String& title) {
  // Smooth circles are drawn first and then covered by rectangles, so only the
  // outward-facing quarter of each anti-aliased edge stays visible.

  // --- Top elbow ---
  // Outer rounded corner
  tft.fillSmoothCircle(OUTER_R_TOP, OUTER_R_TOP, OUTER_R_TOP, LCARS_ORANGE, APP_BACKGROUND);
  tft.fillRect(OUTER_R_TOP, 0, SIDEBAR_W - OUTER_R_TOP, 58, LCARS_ORANGE);
  tft.fillRect(0, OUTER_R_TOP, SIDEBAR_W, 58 - OUTER_R_TOP, LCARS_ORANGE);
  // Inner concave corner
  tft.fillRect(SIDEBAR_W, TOPBAR_H, INNER_R + 1, INNER_R + 1, LCARS_ORANGE);
  tft.fillCircle(SIDEBAR_W + INNER_R + 1, TOPBAR_H + INNER_R + 1, INNER_R, APP_BACKGROUND);

  // --- Title, right-aligned, with the top bar running up to it ---
  String titleText = uiPrettyName(title);
  if (title == "Defaults") titleText = "MAIN MENU";

  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(LCARS_ORANGE);
  tft.setTextDatum(TR_DATUM);
  const int titleRight = 292;
  int titleLeft = titleRight - tft.textWidth(titleText);
  tft.fillRect(OUTER_R_TOP, 0, titleLeft - 6 - OUTER_R_TOP, TOPBAR_H, LCARS_ORANGE);
  tft.drawString(titleText, titleRight, 1);
  // Rounded end cap
  tft.fillSmoothCircle(311, TOPBAR_H / 2, TOPBAR_H / 2, LCARS_ORANGE, APP_BACKGROUND);
  tft.fillRect(298, 0, 13, TOPBAR_H, LCARS_ORANGE);

  // --- Sidebar blocks ---
  drawBlockLabel("PRF", SIDEBAR_W - 5, 55);
  uiDrawBleStatus(bleConnected);
  tft.fillRect(0, FLIP_Y, SIDEBAR_W, FLIP_H, LCARS_BLUE);
  tft.fillRect(0, HOME_Y, SIDEBAR_W, HOME_H, LCARS_RED);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString("FLIP", SIDEBAR_W - 5, FLIP_Y + FLIP_H - 4);
  tft.drawString("HOME", SIDEBAR_W - 5, HOME_Y + HOME_H - 4);

  // --- Bottom elbow ---
  // Outer rounded corner
  tft.fillSmoothCircle(OUTER_R_BOT, SH - OUTER_R_BOT - 1, OUTER_R_BOT, LCARS_PEACH, APP_BACKGROUND);
  tft.fillRect(0, 198, SIDEBAR_W, BOTBAR_Y - 198, LCARS_PEACH);
  tft.fillRect(OUTER_R_BOT, BOTBAR_Y, 180 - OUTER_R_BOT, SH - BOTBAR_Y, LCARS_PEACH);
  // Inner concave corner
  tft.fillRect(SIDEBAR_W, BOTBAR_Y - INNER_R - 1, INNER_R + 1, INNER_R + 1, LCARS_PEACH);
  tft.fillCircle(SIDEBAR_W + INNER_R + 1, BOTBAR_Y - INNER_R - 1, INNER_R, APP_BACKGROUND);
  drawBlockLabel("SHORTCUT KEYBOARD", 174, SH - 4);

  // Bottom bar segments and end cap
  tft.fillRect(183, BOTBAR_Y, 60, SH - BOTBAR_Y, LCARS_ORANGE);
  tft.fillRect(246, BOTBAR_Y, 20, SH - BOTBAR_Y, LCARS_LAVENDER);
  tft.fillSmoothCircle(311, BOTBAR_Y + 8, 8, LCARS_BLUE, APP_BACKGROUND);
  tft.fillRect(269, BOTBAR_Y, 42, SH - BOTBAR_Y, LCARS_BLUE);
}

static void drawButton(const Btn& btn, size_t i, size_t count) {
  int x, y, w, h;
  buttonRect(i, count, x, y, w, h);

  uint16_t color = btn.color != 0 ? btn.color : BUTTON_PALETTE[i % PALETTE_SIZE];
  tft.fillSmoothRoundRect(x, y, w, h, h / 2, color, APP_BACKGROUND);

  // LCARS labels are right-aligned; drop to a smaller (non-bold) font if the label doesn't fit
  const int padRight = 12;
  const int padLeft  = 8;
  String label = uiPrettyName(btn.label);
  int maxTextW = w - padRight - padLeft;
  tft.setFreeFont(useWideLayout(count) ? &FreeSansBold12pt7b : &FreeSansBold9pt7b);
  if (tft.textWidth(label) > maxTextW) {
    Serial.printf("Label \"%s\" is %dpx, only %dpx fits in bold - shorten it in config.json\n",
                  label.c_str(), tft.textWidth(label), maxTextW);
    tft.setFreeFont(nullptr);
    tft.setTextFont(2);
  }
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(label, x + w - padRight, y + h / 2);
}

void uiDrawScreen(const String& title, const std::vector<Btn>& buttons) {
  tft.fillScreen(APP_BACKGROUND);
  drawFrame(title);

  size_t count = visibleButtonCount(buttons.size());
  if (count < buttons.size()) {
    Serial.printf("Profile %s has %u buttons, only %u fit on screen\n",
                  title.c_str(), (unsigned)buttons.size(), (unsigned)count);
  }
  for (size_t i = 0; i < count; i++) {
    drawButton(buttons[i], i, buttons.size());
  }

  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(nullptr);
}

bool uiIsFlipPressed(int touchX, int touchY) {
  return touchX <= SIDEBAR_W + 6 && touchY >= FLIP_Y && touchY < HOME_Y;
}

bool uiIsHomePressed(int touchX, int touchY) {
  // The HOME block plus the bottom elbow beneath it, to be forgiving of touch calibration
  return touchX <= SIDEBAR_W + 6 && touchY >= HOME_Y;
}

int uiGetPressedButtonIndex(int touchX, int touchY) {
  size_t count = visibleButtonCount(buttons.size());
  for (size_t i = 0; i < count; i++) {
    int x, y, w, h;
    buttonRect(i, buttons.size(), x, y, w, h);
    if (touchX >= x && touchX <= x + w && touchY >= y && touchY <= y + h) {
      return i;
    }
  }

  return -1; // No button was pressed (touched empty background space)
}
