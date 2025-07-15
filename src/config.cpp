#include "config.h"
#include "logger.h"
#include "timezone_utils.h"

// Global configuration instance
Config config;

Config::Config() {
    // Constructor - default values are set in struct definitions
    ready = false;
}

bool Config::isValid() const {
    return validateWiFiSettings() && validateSystemSettings() && validateDisplaySettings() && validateBuzzerSettings();
}

bool Config::isWiFiValid() const { return validateWiFiSettings(); }

bool Config::isSystemValid() const { return validateSystemSettings(); }

bool Config::validateWiFiSettings() const {
    // SSID can be empty (for AP mode or manual setup)

    return true;
}

bool Config::validateSystemSettings() const {
    if (system.language.length() == 0) return false;
    if (system.timezone.length() == 0) return false;
    if (system.ntpServer.length() == 0) return false;
    return true;
}

bool Config::validateDisplaySettings() const {
    if (display.brightness < 0 || display.brightness > 100) return false;
    return true;
}

bool Config::validateBuzzerSettings() const {
    if (buzzer.volume < 0 || buzzer.volume > 100) return false;
    return true;
}

bool Config::isReady() const { return this->ready; }

void Config::setReady(bool ready) { this->ready = ready; }

void Config::printConfig() const {
    LOG_INFO("=== Configuration Settings ===");

    LOG_INFO("[WiFi]");
    LOG_INFOF("  SSID: %s\n", wifi.ssid.c_str());
    LOG_INFOF("  Password: %s\n", wifi.password.length() > 0 ? "****" : "Not set");

    LOG_INFO("[System]");
    LOG_INFOF("  Language: %s\n", system.language.c_str());
    LOG_INFOF("  Timezone: %s\n", system.timezone.c_str());
    LOG_INFOF("  NTP Server: %s\n", system.ntpServer.c_str());

    LOG_INFO("[Display]");
    LOG_INFOF("  Brightness: %d%%\n", display.brightness);

    LOG_INFO("[Buzzer]");
    LOG_INFOF("  Volume: %d%%\n", buzzer.volume);
    LOG_INFOF("  Enabled: %s\n", buzzer.enabled ? "Yes" : "No");
    LOG_INFOF("  Startup Sound: %s\n", buzzer.startupSound ? "Yes" : "No");

    LOG_INFO("[Logger]");
    LOG_INFOF("  File Logging: %s\n", logger.fileLoggingEnabled ? "Yes" : "No");
    LOG_INFOF("  Log Level: %s\n", logger.logLevel.c_str());
    LOG_INFOF("  File Prefix: %s\n", logger.filePrefix.c_str());
    LOG_INFOF("  Include Date: %s\n", logger.includeDateInFilename ? "Yes" : "No");

    LOG_INFO("[Modules]");
    LOG_INFO("  Module configurations are now managed by individual modules");

    LOG_INFO("===============================");
}

String Config::toString() const {
    String result = "Config{\n";
    result += "  WiFi: {ssid=" + wifi.ssid + ", password=" + wifi.password + "}\n";
    result += "  System: {language=" + system.language + ", timezone=" + system.timezone +
              ", ntpServer=" + system.ntpServer + "}\n";
    result += "  Display: {brightness=" + String(display.brightness) + "}\n";
    result += "  Buzzer: {volume=" + String(buzzer.volume) + ", enabled=" + String(buzzer.enabled) + "}\n";
    result += "  Logger: {fileLogging=" + String(logger.fileLoggingEnabled) + ", level=" + logger.logLevel + ", prefix=" + logger.filePrefix + "}\n";
    result += "  Modules: {configurations managed by individual modules}\n";
    result += "}";
    return result;
}

// Convert timezone string to GMT offset in seconds
// Delegates to TimezoneUtils for proper UTC-based DST calculation
int Config::getTimezoneOffset(const String& timezone) {
    return TimezoneUtils::getTimezoneOffset(timezone);
}