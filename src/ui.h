#ifndef UI_H
#define UI_H

void uiFillBackground(uint16_t backgroundColor);
void uiDrawButtons(const std::vector<Btn>& buttons);
int uiGetPressedButtonIndex(int touchX, int touchY);

#endif