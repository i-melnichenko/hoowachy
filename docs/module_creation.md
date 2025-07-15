# Module Creation Guide

This guide explains how to create new modules for the Hoowachy system.

## Overview

The Hoowachy system uses an explicit module registration approach. All modules must be manually registered in `main.cpp` before being started.

## Module Registration System

### How It Works

1. **Module Creation**: Each module implements the `IModule` interface
2. **Explicit Registration**: Modules are registered manually in `main.cpp` using `ModuleRegistry::RegisterModule()`
3. **Module Startup**: The `ModuleManager::StartAllModules()` function starts all registered modules

### Registration Process

Modules are registered in the `registerModules()` function in `main.cpp`:

```cpp
void registerModules() {
    LOG_INFO("Registering modules explicitly...");

    // Register Clock module
    modules::ModuleRegistry::RegisterModule("Clock", "clock", 2, 4096,
                                           []() -> modules::IModule* { return new modules::Clock(); });

    // Register AccuWeather module
    modules::ModuleRegistry::RegisterModule("AccuWeather", "accuweather", 5, 12 * 1024,
                                           []() -> modules::IModule* { return new modules::AccuWeather(); });
                                           
    // Register Overlay module
    modules::ModuleRegistry::RegisterModule("Overlay", "overlay", 3, 4096,
                                           []() -> modules::IModule* { return new modules::Overlay(); });

    LOG_INFO("All modules registered explicitly");
}
```

### Registration Parameters

- **Name**: Unique module name for identification
- **Config Section**: INI file section name for configuration
- **Priority**: Task priority (1-5, higher = more priority)
- **Stack Size**: Memory allocated for module task (in bytes)
- **Factory Function**: Lambda that creates module instance

## Creating a New Module

### 1. Create Module Header File

Create `src/modules/yourmodule.h`:

```cpp
#ifndef YOURMODULE_H
#define YOURMODULE_H

#include <Arduino.h>
#include "module.h"

namespace modules {

// Module-specific configuration
struct YourModuleConfig : public ModuleConfig {
    String customSetting = "";
    int customValue = 0;
};

class YourModule : public IModule {
  public:
    void Setup() override;
    void Run(void* parameter) override;
    void Draw() override;
    bool IsReady() override;
    void Configure(const ModuleConfig& config) override;
    bool ConfigureFromSection(const ConfigSection& section) override;

  private:
    YourModuleConfig moduleConfig;
    bool ready = false;
};

}  // namespace modules

#endif  // YOURMODULE_H
```

### 2. Create Module Implementation File

Create `src/modules/yourmodule.cpp`:

```cpp
#include "yourmodule.h"
#include "logger.h"
#include "../config_manager.h"

namespace modules {

void YourModule::Setup() {
    LOG_INFO("YourModule setup");
    // Initialize your module here
}

void YourModule::Run(void* parameter) {
    LOG_INFO("YourModule Run");

    // Wait for configuration to be ready
    ConfigManager* configManager = ConfigManager::getInstance();
    while (!configManager->IsReady()) {
        LOG_INFO("Waiting for config to be ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Configure from INI section
    ConfigSection moduleSection = configManager->getConfigSection("yourmodule");

    if (!ConfigureFromSection(moduleSection)) {
        LOG_INFO("Failed to configure YourModule");
        vTaskDelete(NULL);
        return;
    }

    if (!moduleConfig.enable) {
        vTaskDelete(NULL);
        return;
    }

    ready = true;
    LOG_INFO("YourModule is now READY!");

    while (true) {
        // Your module logic here
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void YourModule::Draw() {
    if (!ready) return;

    // Draw your module content here
    // Use u8g2 library functions
}

bool YourModule::IsReady() {
    return ready;
}

void YourModule::Configure(const ModuleConfig& config) {
    const YourModuleConfig& yourConfig = static_cast<const YourModuleConfig&>(config);
    moduleConfig = yourConfig;

    LOG_INFO("YourModule configured manually");
}

bool YourModule::ConfigureFromSection(const ConfigSection& section) {
    // Parse configuration from INI section
    moduleConfig.customSetting = section.getValue("custom_setting", "default");
    moduleConfig.customValue = section.getIntValue("custom_value", 0);
    moduleConfig.enable = section.getBoolValue("enable", false);

    LOG_INFO("YourModule configured from INI section");
    return true;
}

}  // namespace modules
```

### 3. Add Module to Main Registration

In `src/main.cpp`, add your module to the `registerModules()` function:

```cpp
#include "modules/yourmodule.h"  // Add include

void registerModules() {
    LOG_INFO("Registering modules explicitly...");

    // ... existing registrations ...

    // Register Your Module
    modules::ModuleRegistry::RegisterModule("YourModule", "yourmodule", 2, 8192,
                                           []() -> modules::IModule* { return new modules::YourModule(); });

    LOG_INFO("All modules registered explicitly");
}
```

### 4. Add Configuration Section

Add configuration section to `hoowachy_config.ini`:

```ini
[yourmodule]
enable=true
custom_setting="example"
custom_value=42
position_x=0
position_y=0
width=128
height=64
```

## Module Lifecycle

1. **Registration**: Module registered in `registerModules()`
2. **Startup**: `ModuleManager::StartAllModules()` creates task for each module
3. **Setup**: `Setup()` method called for initialization
4. **Run**: `Run()` method executed in module's task
5. **Configuration**: Module configures itself from INI file
6. **Ready State**: Module sets `ready = true` when operational
7. **Draw**: `Draw()` method called by display system for rendering

## Module Types

### Regular Modules

- Standard modules that draw content on designated screen areas
- Examples: Clock, AccuWeather

### Overlay Modules

- Special modules that draw on top of other modules
- Override `IsOverlay()` to return `true`
- Example: Overlay (system info display)

```cpp
bool IsOverlay() const override { return true; }
```

## Best Practices

1. **Always check `ready` state** before drawing or performing operations
2. **Handle configuration errors gracefully** - return `false` from `ConfigureFromSection()`
3. **Use appropriate stack sizes** - larger for HTTP/JSON operations, smaller for simple modules
4. **Set proper task priorities** - higher for time-sensitive modules
5. **Include error handling** in `Run()` method
6. **Log important events** for debugging
7. **Clean up resources** when module exits

## Module Priority Guidelines

- **Priority 1**: Low priority, background tasks
- **Priority 2**: Normal priority, most modules
- **Priority 3**: Medium priority, interactive modules
- **Priority 4**: High priority, time-sensitive modules
- **Priority 5**: Highest priority, critical system modules

## Stack Size Guidelines

- **4KB**: Simple modules with minimal memory usage
- **8KB**: Standard modules with basic operations
- **12KB**: Modules with moderate memory needs
- **64KB**: Modules with HTTP requests, JSON parsing, large data structures

## Troubleshooting

### Module Not Starting

- Check registration in `registerModules()` function
- Verify module is included in `main.cpp`
- Check task creation logs
- Ensure adequate stack size

### Module Not Drawing

- Verify `IsReady()` returns `true`
- Check if display is in correct state (DASHBOARD vs TERMINAL)
- Ensure `Draw()` method is implemented
- For overlay modules, check `IsOverlay()` returns `true`

### Configuration Issues

- Verify INI section exists and is correctly named
- Check configuration parsing in `ConfigureFromSection()`
- Ensure `enable=true` in configuration
- Review configuration logs

## Current Modules

1. **Clock**: Displays current time with timezone support
2. **AccuWeather**: Weather information and forecasts
4. **Overlay**: System information overlay (FPS, memory, WiFi)

Each module serves as an example of different implementation patterns and can be used as reference for new modules.
