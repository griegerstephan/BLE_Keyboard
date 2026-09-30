#ifndef UI_H
#define UI_H

#include "config.h"

void uiFillBackground(uint16_t backgroundColor);
void uiDrawScreen(const String& title, const std::vector<Btn>& buttons);
String uiPrettyName(const String& name);
void uiDrawBleStatus(bool connected);
bool uiIsHomePressed(int touchX, int touchY);
int uiGetPressedButtonIndex(int touchX, int touchY);

#endif
