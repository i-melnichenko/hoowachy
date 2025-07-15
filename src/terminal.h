#ifndef TERMINAL_H
#define TERMINAL_H

#include <Arduino.h>
#include "logger.h"
#include <vector>

// Forward declarations
class TerminalEvent;

class Terminal {
  public:
    class ConsoleLine {
      public:
        int8_t id;
        String group;
        String description;
        String status;
        float offsetX = 0;
        ConsoleLine(int8_t id, const String& group, const String& description, const String& status)
            : id(id), group(group), description(description), status(status) {}
    };

    static void Setup();
    static void AddLine(int8_t id, const String& group, const String& description, const String& status);
    static const std::vector<ConsoleLine>& GetLines();
    static void UpdateLine(int8_t id, const String& group, const String& description, const String& status);

  private:
    static std::vector<ConsoleLine> consoleLines;

    // Single unified event handler
    static void onTerminalEvent(const TerminalEvent& event);
};

#endif