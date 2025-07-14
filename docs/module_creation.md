# Module Registry System

This system allows for automatic registration and management of modules in the project. Modules can be easily added or removed without modifying the main application code.

## How It Works

1. **Module Registry**: All modules are automatically registered using the `REGISTER_MODULE` macro
2. **Module Manager**: Handles the lifecycle of all registered modules
3. **Auto-discovery**: Modules are discovered at compile time and started automatically

## Adding a New Module

To add a new module, you only need to create 2 files in the `modules/` directory:

### 1. Create Module Header (`your_module.h`)

```cpp
#ifndef YOUR_MODULE_H
#define YOUR_MODULE_H

#include "module.h"

namespace modules {

// Your module specific configuration
struct YourModuleConfig : public ModuleConfig {
    String someParameter = "default_value";
    int someNumber = 42;
};

class YourModule : public IModule {
public:
    void Setup() override;
    void Run(void *parameter) override;
    void Draw() override;
    bool IsReady() override;
    void Configure(const ModuleConfig& config) override;
    bool ConfigureFromSection(const ConfigSection& section) override;

private:
    YourModuleConfig moduleConfig;
    bool ready = false;
};

}

#endif // YOUR_MODULE_H
```

### 2. Create Module Implementation (`your_module.cpp`)

```cpp
#include "your_module.h"
#include "module_registry.h"

// Auto-register this module
REGISTER_MODULE("YourModule", "your_module", 2, 4096, modules::YourModule)

namespace modules {

void YourModule::Setup() {
    // Basic module initialization
    Serial.println("YourModule: Setup called");
    // ready status will be determined in Run()
}

void YourModule::Run(void *parameter) {
    // Your module's main loop
    while (true) {
        // Check if module is enabled in configuration
        if (!moduleConfig.enable) {
            Serial.println("YourModule: Module disabled in configuration");
            ready = false;
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before checking again
            continue;
        }

        // Check if WiFi is connected (if needed)
        if (!WiFi.isConnected()) {
            Serial.println("YourModule: WiFi not connected, waiting...");
            ready = false;
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before retrying
            continue;
        }

        // All checks passed, module is ready
        if (!ready) {
            Serial.println("YourModule: All checks passed, module is ready");
            ready = true;
        }

        // Do your work here
        // Example: make API calls, process data, etc.

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void YourModule::Draw() {
    // Drawing logic for display
}

bool YourModule::IsReady() {
    return ready;
}

void YourModule::Configure(const ModuleConfig& config) {
    // Handle legacy configuration
}

bool YourModule::ConfigureFromSection(const ConfigSection& section) {
    // Parse configuration from INI file
    moduleConfig.someParameter = section.getValue("someParameter", "default_value");
    moduleConfig.someNumber = section.getIntValue("someNumber", 42);
    moduleConfig.enable = section.getBoolValue("enable", false);

    return true;
}

}
```

### 3. Add Module Include to main.cpp

Add an include for your new module to `main.cpp`:

```cpp
#include "modules/your_module.h"
```

### 4. Add Configuration Section

Add a configuration section to `hoowachy_config.ini`:

```ini
[your_module]
enable=true
someParameter=hello_world
someNumber=123
```

## Removing a Module

To remove a module:

1. Delete the 2 module files (`your_module.h` and `your_module.cpp`)
2. Remove the include from `main.cpp`
3. Remove the configuration section from `hoowachy_config.ini`

That's it! The module will be completely removed from the system.

## REGISTER_MODULE Parameters

```cpp
REGISTER_MODULE(name, configSection, priority, stackSize, moduleClass)
```

- `name`: Display name for the module
- `configSection`: INI file section name for configuration
- `priority`: FreeRTOS task priority (1-10, higher = more priority)
- `stackSize`: Stack size in bytes for the module task
- `moduleClass`: Your module class name

## Example Modules

- **Clock**: `clock.h` / `clock.cpp` - Displays current time
- **AccuWeather**: `accuweather.h` / `accuweather.cpp` - Weather information

## Best Practices

### WiFi-Dependent Modules

For modules that require internet connectivity (like weather, API calls, etc.):

- Put all WiFi connectivity checks in `Run()` method
- Continuously monitor connection status during operation
- Handle connection loss gracefully by waiting and retrying

### Configuration Validation

- Put all configuration checks in `Run()` method
- Continuously check if the module is enabled in configuration
- Validate required configuration parameters in the main loop
- Set `ready = false` if any validation fails

### Error Handling

- Use Serial.println() for debugging and status messages
- Set appropriate delays when waiting for resources
- Implement retry logic for recoverable errors

### Dependencies

Common includes you might need:

```cpp
#include <WiFi.h>          // For WiFi connectivity checks
#include <HTTPClient.h>    // For HTTP requests
#include <ArduinoJson.h>   // For JSON parsing
```

## Benefits

1. **Easy Module Management**: Add/remove modules by simply adding/removing files
2. **No Main Code Changes**: No need to modify `main.cpp` when adding modules
3. **Automatic Registration**: Modules register themselves at compile time
4. **Consistent Interface**: All modules follow the same pattern
5. **Configuration Support**: Each module can have its own INI configuration section
6. **Robust Error Handling**: Built-in checks for connectivity and configuration
