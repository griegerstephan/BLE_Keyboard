#ifndef BLE_H
#define BLE_H

#include <Arduino.h>
#include <vector>    // <-- ADD THIS: Defines what a 'std::vector' is

void bleRunAction(std::vector<String> steps);

#endif  