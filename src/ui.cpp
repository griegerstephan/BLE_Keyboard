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

// Sidebar blocks (FLIP lives in the top elbow, y 0-57)
const int ELBOW_H = 58;
const int SNIP_Y  = 61;
const int SNIP_H  = 34;
const int DESK_Y  = 98;
const int DESK_H  = 50;
const int HOME_Y  = 151;
const int HOME_H  = 44;

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

// Right-aligned black label in a small font
static void drawBlockLabel(const char* text, int rightX, int bottomY) {
  tft.setFreeFont(nullptr);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(text, rightX, bottomY);
}

// Right-aligned black label in the bold button font
static void drawBoldLabel(const char* text, int rightX, int bottomY) {
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(text, rightX, bottomY);
}

// Draws one sidebar button in the given colour (used for normal drawing and the tap flash)
static void drawSidebarBlock(SidebarAction action, uint16_t color) {
  switch (action) {
    case SIDEBAR_FLIP:
      // Top elbow: smooth outer corner first, then rectangles cover all but its outer quarter
      tft.fillSmoothCircle(OUTER_R_TOP, OUTER_R_TOP, OUTER_R_TOP, color, APP_BACKGROUND);
      tft.fillRect(OUTER_R_TOP, 0, SIDEBAR_W - OUTER_R_TOP, ELBOW_H, color);
      tft.fillRect(0, OUTER_R_TOP, SIDEBAR_W, ELBOW_H - OUTER_R_TOP, color);
      drawBoldLabel("FLIP", SIDEBAR_W - 5, ELBOW_H - 4);
      break;
    case SIDEBAR_SNIP:
      tft.fillRect(0, SNIP_Y, SIDEBAR_W, SNIP_H, color);
      drawBoldLabel("SNIP", SIDEBAR_W - 5, SNIP_Y + SNIP_H - 4);
      break;
    case SIDEBAR_DESK:
      tft.fillRect(0, DESK_Y, SIDEBAR_W, DESK_H, color);
      drawBoldLabel("DESK", SIDEBAR_W - 5, DESK_Y + DESK_H - 4);
      break;
    case SIDEBAR_HOME:
      tft.fillRect(0, HOME_Y, SIDEBAR_W, HOME_H, color);
      drawBoldLabel("HOME", SIDEBAR_W - 5, HOME_Y + HOME_H - 4);
      break;
    default:
      break;
  }
}

static uint16_t sidebarColor(SidebarAction action) {
  switch (action) {
    case SIDEBAR_FLIP: return LCARS_ORANGE;
    case SIDEBAR_SNIP: return LCARS_LAVENDER;
    case SIDEBAR_DESK: return LCARS_BLUE;
    case SIDEBAR_HOME: return LCARS_RED;
    default:           return APP_BACKGROUND;
  }
}

// Lights a sidebar button white while it is held, and restores its colour on release
void uiDrawSidebarState(SidebarAction action, bool pressed) {
  drawSidebarBlock(action, pressed ? TFT_WHITE : sidebarColor(action));
  tft.setTextDatum(TL_DATUM);
}

// Bottom elbow: peach with "BLE LINK" when connected, red with "NO LINK" when not
void uiDrawBleStatus(bool connected) {
  bleConnected = connected;
  uint16_t color = connected ? LCARS_PEACH : LCARS_RED;

  // Outer rounded corner
  tft.fillSmoothCircle(OUTER_R_BOT, SH - OUTER_R_BOT - 1, OUTER_R_BOT, color, APP_BACKGROUND);
  tft.fillRect(0, 198, SIDEBAR_W, BOTBAR_Y - 198, color);
  tft.fillRect(OUTER_R_BOT, BOTBAR_Y, 180 - OUTER_R_BOT, SH - BOTBAR_Y, color);
  // Inner concave corner
  tft.fillRect(SIDEBAR_W, BOTBAR_Y - INNER_R - 1, INNER_R + 1, INNER_R + 1, color);
  tft.fillCircle(SIDEBAR_W + INNER_R + 1, BOTBAR_Y - INNER_R - 1, INNER_R, APP_BACKGROUND);
  drawBlockLabel(connected ? "BLE LINK" : "NO LINK", 174, SH - 4);

  // First bar segment follows the link state too
  tft.fillRect(183, BOTBAR_Y, 60, SH - BOTBAR_Y, connected ? LCARS_ORANGE : LCARS_RED);
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

static void drawFrame(const String& title) {
  // Smooth circles are drawn first and then covered by rectangles, so only the
  // outward-facing quarter of each anti-aliased edge stays visible.

  // --- Top elbow (doubles as the FLIP button) ---
  drawSidebarBlock(SIDEBAR_FLIP, LCARS_ORANGE);
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

  // --- Sidebar buttons ---
  drawSidebarBlock(SIDEBAR_SNIP, LCARS_LAVENDER);
  drawSidebarBlock(SIDEBAR_DESK, LCARS_BLUE);
  drawSidebarBlock(SIDEBAR_HOME, LCARS_RED);

  // --- Bottom elbow with Bluetooth status, then the remaining bar segments and end cap ---
  uiDrawBleStatus(bleConnected);
  tft.fillRect(246, BOTBAR_Y, 20, SH - BOTBAR_Y, LCARS_LAVENDER);
  tft.fillSmoothCircle(311, BOTBAR_Y + 8, 8, LCARS_BLUE, APP_BACKGROUND);
  tft.fillRect(269, BOTBAR_Y, 42, SH - BOTBAR_Y, LCARS_BLUE);
}

static void drawButton(const Btn& btn, size_t i, size_t count, bool pressed = false) {
  int x, y, w, h;
  buttonRect(i, count, x, y, w, h);

  uint16_t color = btn.color != 0 ? btn.color : BUTTON_PALETTE[i % PALETTE_SIZE];
  if (pressed) color = TFT_WHITE;
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

// Redraws one profile button white while held, or in its normal colour on release
void uiDrawButtonState(size_t index, bool pressed) {
  if (index >= visibleButtonCount(buttons.size())) return;
  drawButton(buttons[index], index, buttons.size(), pressed);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(nullptr);
}

SidebarAction uiGetSidebarAction(int touchX, int touchY) {
  if (touchX > SIDEBAR_W + 6) return SIDEBAR_NONE;
  // Boundaries sit in the gaps between blocks. HOME also takes the bottom elbow beneath
  // it, to be forgiving of touch calibration.
  if (touchY < SNIP_Y - 1) return SIDEBAR_FLIP;
  if (touchY < DESK_Y - 1) return SIDEBAR_SNIP;
  if (touchY < HOME_Y - 1) return SIDEBAR_DESK;
  return SIDEBAR_HOME;
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
