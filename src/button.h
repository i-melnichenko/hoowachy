#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "pins.h"

// Forward declarations
class EventManager;
class Config;

class Button {
  public:
    static void Setup();
    static void Run();
};

#endif  // BUTTON_H