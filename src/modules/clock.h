#ifndef CLOCK_H
#define CLOCK_H

#include "module.h"

// Forward declaration
class WiFiManager;

namespace modules {

// Clock specific configuration
struct ClockConfig : public ModuleConfig {
    String format = "24h";  // "12h" or "24h"
    bool showSeconds = true;
    int syncInterval = 3600;        // Time sync interval in seconds
    String timezone = "";           // Module-specific timezone
    String systemTimezone = "UTC";  // Fallback system timezone
};

class Clock : public IModule {
  public:
    void Setup() override;
    void Run(void* parameter) override;
    void Draw() override;
    bool IsReady() override;
    void Configure(const ModuleConfig& config) override;
    bool ConfigureFromSection(const ConfigSection& section) override;

  private:
    // Module configuration (injected)
    ClockConfig moduleConfig;

    // Module state
    bool ready = false;
};

}  // namespace modules

#endif