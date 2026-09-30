
#ifndef DESKTOP_H
#define DESKTOP_H

#include <Arduino.h>

void desktopDraw();
void desktopHandleTouch(int x, int y);
void desktopHandleRelease();
void checkForIncomingWindowsProfile();
  
#endif