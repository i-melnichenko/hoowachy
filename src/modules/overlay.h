#ifndef OVERLAY_H
#define OVERLAY_H

#include <Arduino.h>
#include "logger.h"
#include "module.h"
#include "../event_manager.h"

namespace modules {

// Overlay specific configuration
struct OverlayConfig : public ModuleConfig {
    bool showFps = true;
    bool showMemory = true;
    bool showWifi = true;
    bool showCpu = true;
    bool showUptime = true;
    int fontSize = 1;           // 1=small, 2=medium, 3=large
    int corner = 1;             // 1=top-left, 2=top-right, 3=bottom-left, 4=bottom-right
    int spacing = 8;            // spacing between lines
    bool transparent = false;   // background transparency
};

class Overlay : public IModule {
  public:
    void Setup() override;
    void Run(void* parameter) override;
    void Draw() override;
    bool IsReady() override;
    void Configure(const ModuleConfig& config) override;
    bool ConfigureFromSection(const ConfigSection& section) override;
    bool IsOverlay() const override { return true; }

  private:
    // Module configuration
    OverlayConfig moduleConfig;

    // Module state
    bool ready = false;
    bool isVisible = false;  // Controls overlay visibility
    
    // FPS tracking
    unsigned long lastFrameTime = 0;
    unsigned long frameCount = 0;
    float currentFps = 0.0f;
    unsigned long lastFpsUpdate = 0;
    
    // Memory tracking
    size_t currentFreeHeap = 0;
    unsigned long lastMemoryUpdate = 0;
    
    // WiFi tracking  
    int currentRssi = 0;
    unsigned long lastWifiUpdate = 0;
    
    // CPU tracking
    float currentCpuUsage = 0.0f;
    unsigned long lastCpuUpdate = 0;
    
    // Uptime tracking
    unsigned long currentUptime = 0;
    unsigned long lastUptimeUpdate = 0;
    
    // Button event handlers
    static void onButtonLongPress(const ButtonLongPressEvent& event);
    static void onButtonShortPress(const ButtonShortPressEvent& event);
    static Overlay* instance;  // Static instance for event callbacks
    
    // Helper methods
    void updateFps();
    void updateMemory();
    void updateWifi();
    void updateCpu();
    void updateUptime();
    void drawOverlayInfo();
    String formatMemory(size_t bytes);
    String formatWifiSignal(int rssi);
    String formatCpuUsage(float cpuUsage);
    String formatUptime(unsigned long uptimeMs);
    void getPositionForCorner(int& x, int& y);
};

}  // namespace modules

#endif  // OVERLAY_H 