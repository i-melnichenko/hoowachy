#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "logger.h"

class Config {
  public:
    // WiFi Settings
    struct WiFiSettings {
        String ssid = "";
        String password = "";
    } wifi;

    // System Settings
    struct SystemSettings {
        String language = "en";
        String timezone = "UTC";
        String ntpServer = "pool.ntp.org";
    } system;

    // Display Settings
    struct DisplaySettings {
        int brightness = 80;
    } display;

    // Buzzer Settings
    struct BuzzerSettings {
        int volume = 50;
        bool enabled = true;
        bool startupSound = true;
    } buzzer;

    // Logger Settings
    struct LoggerSettings {
        bool fileLoggingEnabled = false;
        String logLevel = "INFO";  // DEBUG, INFO, WARNING, ERROR
        String filePrefix = "hoowachy";
        bool includeDateInFilename = true;
    } logger;

    // Constructor
    Config();

    // Validation methods
    bool isValid() const;
    bool isWiFiValid() const;
    bool isSystemValid() const;

    // Utility methods
    void printConfig() const;
    String toString() const;
    bool isReady() const;
    void setReady(bool ready);

    // Timezone utilities
    static int getTimezoneOffset(const String& timezone);

  private:
    // Internal validation helpers
    bool validateWiFiSettings() const;
    bool validateSystemSettings() const;
    bool validateDisplaySettings() const;
    bool validateBuzzerSettings() const;
    bool ready;
};

// Global configuration instance
extern Config config;

extern SemaphoreHandle_t spiMutex;

#define CONFIG_DEBUG 1
#define EEPROM_SIZE 1024
#define BUTTON_LONG_PRESS_TIME 300

#endif  // CONFIG_H