#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "logger.h"
#include <vector>
#include "pins.h"
#include "terminal.h"

// Forward declarations
class U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI;

namespace modules {
class IModule;
}

// External reference to active modules
extern std::vector<modules::IModule*> active_modules;

class Display {
  public:
    enum class State { TERMINAL, DASHBOARD };

    static void Setup();
    static void Run();
    static void SetState(State state);

  private:
    static State currentState;
    static void on_button_press();
    static void drawTerminal();
    static void drawDashboard();
    static void drawRightAlignedText(const char* text, int y);

    // Animation variables
    static int loadingAngle;
    static int loadingDots;
    static int animationCounter;
    static int bootStep;
    static int stepProgress;
    static bool stepCompleted;
};

#endif
