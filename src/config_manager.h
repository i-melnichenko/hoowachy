#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "logger.h"
#include "config.h"
#include "modules/module.h"

// Forward declarations
namespace fs {
class File;
}

class WiFiClass;
class SDClass;
class SPIClass;

class ConfigManager {
  private:
    static ConfigManager* instance;
    bool sdInitialized;
    const char* configFileName;

    // SPI management for avoiding conflicts with display
    void restoreSPISettings();
    bool tryAlternativeSDInit();
    void configureSPIForSD();

    // Private constructor for singleton pattern
    ConfigManager();

    // Helper methods
    void trim(std::string& str);
    bool parseINIFile(const String& filePath);
    String readFile(const String& filePath);

    // Configuration parsing helpers
    void parseWiFiSection(const String& key, const String& value);
    void parseSystemSection(const String& key, const String& value);
    void parseDisplaySection(const String& key, const String& value);
    void parseBuzzerSection(const String& key, const String& value);
    void parseLoggerSection(const String& key, const String& value);

  public:
    // Singleton pattern
    static ConfigManager* getInstance();

    // SD card operations
    bool initializeSD();
    bool isSDReady() const;

    // Configuration readiness check
    bool IsReady() const;

    // System configuration access
    String getSystemTimezone() const;

    // Configuration file operations
    bool loadConfig(const char* fileName = "hoowachy_config.ini");
    // bool saveConfig(const char* fileName = "hoowachy_config.ini");

    // Configuration validation
    bool validateConfig();

    // Configuration section operations
    modules::ConfigSection getConfigSection(const String& sectionName, const String& filePath = "hoowachy_config.ini");

    // Utility methods
    void printConfig();
    bool configExists();

    // File operations
    bool fileExists(const String& filePath);
    // bool createDefaultConfig();
};

#endif  // CONFIG_MANAGER_H