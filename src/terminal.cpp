#include "terminal.h"
#include "event_manager.h"

std::vector<Terminal::ConsoleLine> Terminal::consoleLines;

void Terminal::Setup() {
    // Subscribe to unified terminal event
    EventManager::Subscribe<TerminalEvent>(onTerminalEvent);

    // Add welcome line
    consoleLines.push_back(ConsoleLine(0, "SYS", "Welcome Hoowachy 1.0", "OK"));
}

void Terminal::AddLine(int8_t id, const String& group, const String& description, const String& status) {
    consoleLines.push_back(ConsoleLine(id, group, description, status));
}

const std::vector<Terminal::ConsoleLine>& Terminal::GetLines() { return consoleLines; }

void Terminal::UpdateLine(int8_t id, const String& group, const String& description, const String& status) {
    for (int i = 0; i < consoleLines.size(); i++) {
        if (consoleLines[i].id == id && consoleLines[i].group == group) {
            consoleLines[i].description = description;
            consoleLines[i].status = status;
            consoleLines[i].offsetX = 0;
            return;
        }
    }
    // If not found, add new line
    consoleLines.push_back(ConsoleLine(id, group, description, status));
}

void Terminal::onTerminalEvent(const TerminalEvent& event) {
    Serial.printf("Terminal event: [%s] %s\n", event.group.c_str(), event.info.c_str());

    String status = "";
    if (event.state == TerminalEvent::State::SUCCESS) {
        status = "OK";
    } else if (event.state == TerminalEvent::State::FAILURE) {
        status = "FAIL";
    }

    // Combine info with extra data if available
    String fullInfo = event.info;
    if (!event.extra.isEmpty()) {
        fullInfo += " " + event.extra;
    }

    UpdateLine(event.step, event.group, fullInfo, status);
}