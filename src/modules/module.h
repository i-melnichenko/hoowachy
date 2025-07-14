#ifndef MODULE_H
#define MODULE_H

#include <Arduino.h>
#include <map>
#include <vector>

namespace modules {

// Structure to hold a configuration section from INI file
struct ConfigSection {
    std::map<String, String> keyValuePairs;

    // Helper method to get value with default
    String getValue(const String& key, const String& defaultValue = "") const {
        auto it = keyValuePairs.find(key);
        return (it != keyValuePairs.end()) ? it->second : defaultValue;
    }

    // Helper method to get boolean value
    bool getBoolValue(const String& key, bool defaultValue = false) const {
        String value = getValue(key, defaultValue ? "true" : "false");
        value.toLowerCase();
        return value == "true" || value == "1" || value == "yes";
    }

    // Helper method to get integer value
    int getIntValue(const String& key, int defaultValue = 0) const {
        String value = getValue(key, String(defaultValue));
        return value.toInt();
    }
};

// Base configuration structure for all modules
struct ModuleConfig {
    int positionX = 0;
    int positionY = 0;
    int width = 128;
    int height = 64;
    bool enable = false;

    virtual ~ModuleConfig() = default;
};

class IModule {
  public:
    virtual ~IModule() = default;
    virtual void Setup() = 0;
    virtual void Run(void* parameter) = 0;
    virtual void Draw() = 0;
    virtual bool IsReady() = 0;

    // Method to inject configuration (legacy)
    virtual void Configure(const ModuleConfig& config) = 0;

    // New method to configure from INI section
    virtual bool ConfigureFromSection(const ConfigSection& section) = 0;
};

}  // namespace modules

extern std::vector<modules::IModule*> active_modules;

#endif  // MODULE_H