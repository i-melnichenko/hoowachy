#include "terminal.h"
#include "logger.h"
#include "memory_manager.h"
#include "event_manager.h"

std::vector<Terminal::ConsoleLine> Terminal::consoleLines;

void Terminal::Setup() {
    // Subscribe to unified terminal event
    EventManager::Subscribe<TerminalEvent>(onTerminalEvent);

    // Add welcome line
    consoleLines.push_back(ConsoleLine(0, "SYS", "Welcome Hoowachy 1.0", "OK"));
}

void Terminal::AddLine(int8_t id, const String& group, const String& description, const String& status) {
    // Check memory before adding new console lines
    if (MEMORY_CHECK_CRITICAL()) {
        LOG_WARNING("Terminal: Skipping line addition due to critical memory");
        return;
    }
    
    // Limit console buffer size to prevent memory issues
    if (consoleLines.size() >= 20) {
        consoleLines.erase(consoleLines.begin()); // Remove oldest line
    }
    
    consoleLines.push_back(ConsoleLine(id, group, description, status));
}

const std::vector<Terminal::ConsoleLine>& Terminal::GetLines() { return consoleLines; }

void Terminal::UpdateLine(int8_t id, const String& group, const String& description, const String& status) {
    // Check memory before updating console lines
    if (MEMORY_CHECK_CRITICAL()) {
        LOG_WARNING("Terminal: Skipping line update due to critical memory");
        return;
    }
    
    for (int i = 0; i < consoleLines.size(); i++) {
        if (consoleLines[i].id == id && consoleLines[i].group == group) {
            consoleLines[i].description = description;
            consoleLines[i].status = status;
            consoleLines[i].offsetX = 0;
            return;
        }
    }
    
    // If not found, add new line (with memory check)
    if (consoleLines.size() < 20) { // Limit buffer size
        consoleLines.push_back(ConsoleLine(id, group, description, status));
    }
}

void Terminal::onTerminalEvent(const TerminalEvent& event) {
    LOG_INFOF("Terminal event: [%s] %s\n", event.group.c_str(), event.info.c_str());

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