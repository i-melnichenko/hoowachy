#include "config.h"

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
    Serial.println("=== Configuration Settings ===");

    Serial.println("[WiFi]");
    Serial.printf("  SSID: %s\n", wifi.ssid.c_str());
    Serial.printf("  Password: %s\n", wifi.password.length() > 0 ? "****" : "Not set");

    Serial.println("[System]");
    Serial.printf("  Language: %s\n", system.language.c_str());
    Serial.printf("  Timezone: %s\n", system.timezone.c_str());
    Serial.printf("  NTP Server: %s\n", system.ntpServer.c_str());

    Serial.println("[Display]");
    Serial.printf("  Brightness: %d%%\n", display.brightness);

    Serial.println("[Buzzer]");
    Serial.printf("  Volume: %d%%\n", buzzer.volume);
    Serial.printf("  Enabled: %s\n", buzzer.enabled ? "Yes" : "No");
    Serial.printf("  Startup Sound: %s\n", buzzer.startupSound ? "Yes" : "No");

    Serial.println("[Modules]");
    Serial.println("  Module configurations are now managed by individual modules");

    Serial.println("===============================");
}

String Config::toString() const {
    String result = "Config{\n";
    result += "  WiFi: {ssid=" + wifi.ssid + ", password=" + wifi.password + "}\n";
    result += "  System: {language=" + system.language + ", timezone=" + system.timezone +
              ", ntpServer=" + system.ntpServer + "}\n";
    result += "  Display: {brightness=" + String(display.brightness) + "}\n";
    result += "  Buzzer: {volume=" + String(buzzer.volume) + ", enabled=" + String(buzzer.enabled) + "}\n";
    result += "  Modules: {configurations managed by individual modules}\n";
    result += "}";
    return result;
}

// Helper function to determine if it's daylight saving time for European timezones
bool isDaylightSavingTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;  // Default to standard time if we can't get time
    }

    int month = timeinfo.tm_mon + 1;  // tm_mon is 0-based
    int day = timeinfo.tm_mday;
    int weekday = timeinfo.tm_wday;  // 0=Sunday, 1=Monday, etc.

    // DST runs from last Sunday in March to last Sunday in October

    // Before March or after October - definitely standard time
    if (month < 3 || month > 10) {
        return false;
    }

    // April to September - definitely daylight saving time
    if (month > 3 && month < 10) {
        return true;
    }

    // March - check if we're past the last Sunday
    if (month == 3) {
        // Find last Sunday of March
        int lastSunday = 31;  // Start from last day of March
        while (lastSunday > 0) {
            // Calculate day of week for this date
            struct tm testDate = timeinfo;
            testDate.tm_mday = lastSunday;
            testDate.tm_mon = 2;  // March is month 2 (0-based)
            mktime(&testDate);    // Normalize the date

            if (testDate.tm_wday == 0) {  // Sunday
                break;
            }
            lastSunday--;
        }
        return day >= lastSunday;
    }

    // October - check if we're before the last Sunday
    if (month == 10) {
        // Find last Sunday of October
        int lastSunday = 31;  // Start from last day of October
        while (lastSunday > 0) {
            // Calculate day of week for this date
            struct tm testDate = timeinfo;
            testDate.tm_mday = lastSunday;
            testDate.tm_mon = 9;  // October is month 9 (0-based)
            mktime(&testDate);    // Normalize the date

            if (testDate.tm_wday == 0) {  // Sunday
                break;
            }
            lastSunday--;
        }
        return day < lastSunday;
    }

    return false;  // Default to standard time
}

// Convert timezone string to GMT offset in seconds
int Config::getTimezoneOffset(const String& timezone) {
    // Common timezone mappings (GMT offset in seconds)
    if (timezone == "UTC" || timezone == "GMT") return 0;

    // Europe - with DST support
    if (timezone == "CET" || timezone == "Europe/Berlin") {
        return isDaylightSavingTime() ? 2 * 3600 : 1 * 3600;  // CEST vs CET
    }
    if (timezone == "EET" || timezone == "Europe/Kiev") {
        return isDaylightSavingTime() ? 3 * 3600 : 2 * 3600;  // EEST vs EET
    }
    if (timezone == "BST" || timezone == "Europe/London") {
        return isDaylightSavingTime() ? 1 * 3600 : 0;  // BST vs GMT
    }

    // America
    if (timezone == "EST" || timezone == "America/New_York") return -5 * 3600;
    if (timezone == "CST" || timezone == "America/Chicago") return -6 * 3600;
    if (timezone == "MST" || timezone == "America/Denver") return -7 * 3600;
    if (timezone == "PST" || timezone == "America/Los_Angeles") return -8 * 3600;

    // Asia
    if (timezone == "JST" || timezone == "Asia/Tokyo") return 9 * 3600;
    if (timezone == "CST" || timezone == "Asia/Shanghai") return 8 * 3600;
    if (timezone == "IST" || timezone == "Asia/Kolkata") return 5 * 3600 + 30 * 60;

    // Try to parse as GMT+/-X format
    if (timezone.startsWith("GMT+") || timezone.startsWith("UTC+")) {
        int hours = timezone.substring(4).toInt();
        return hours * 3600;
    }
    if (timezone.startsWith("GMT-") || timezone.startsWith("UTC-")) {
        int hours = timezone.substring(4).toInt();
        return -hours * 3600;
    }

    // Default to UTC if timezone is not recognized
    Serial.printf("Warning: Unknown timezone '%s', defaulting to UTC\n", timezone.c_str());
    return 0;
}