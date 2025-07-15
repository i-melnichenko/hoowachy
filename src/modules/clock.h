#ifndef CLOCK_H
#define CLOCK_H

#include "module.h"
#include "logger.h"

// Forward declaration
class WiFiManager;

namespace modules {

// Clock specific configuration
struct ClockConfig : public ModuleConfig {
    String format = "24h";  // "12h" or "24h"
    bool showSeconds = true;
    int syncInterval = 3600;        // Time sync interval in seconds
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
    
    // Helper function to calculate timezone offset for mktime conversion
    time_t timezone_offset_from_mktime_to_utc();
};

}  // namespace modules

#endif