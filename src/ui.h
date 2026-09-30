#ifndef UI_H
#define UI_H

#include "config.h"

void uiFillBackground(uint16_t backgroundColor);
void uiLoadRotation();
void uiToggleRotation();
void uiMapTouch(int& x, int& y);

// Fixed buttons in the LCARS sidebar, from top to bottom
enum SidebarAction { SIDEBAR_NONE, SIDEBAR_FLIP, SIDEBAR_SNIP, SIDEBAR_DESK, SIDEBAR_HOME };
SidebarAction uiGetSidebarAction(int touchX, int touchY);
void uiFlashSidebar(SidebarAction action);

void uiDrawScreen(const String& title, const std::vector<Btn>& buttons);
String uiPrettyName(const String& name);
void uiDrawBleStatus(bool connected);
int uiGetPressedButtonIndex(int touchX, int touchY);

#endif
