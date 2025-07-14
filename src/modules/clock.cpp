#include "clock.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <time.h>
#include "../config_manager.h"
#include "../timezone_utils.h"
#include "../wifi_manager.h"
#include "module_registry.h"

// Auto-register this module
REGISTER_MODULE("Clock", "clock", 2, 4096, modules::Clock)

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

namespace modules {

void Clock::Configure(const ModuleConfig& config) {
    // Cast to specific config type and store
    const ClockConfig& clockConfig = static_cast<const ClockConfig&>(config);
    moduleConfig = clockConfig;

    Serial.println("Clock module configured");
    Serial.printf("  Format: %s\n", moduleConfig.format.c_str());
    Serial.printf("  Show seconds: %s\n", moduleConfig.showSeconds ? "YES" : "NO");
    Serial.printf("  Timezone: %s\n", moduleConfig.timezone.isEmpty() ? "DEFAULT" : moduleConfig.timezone.c_str());
    Serial.printf("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    Serial.printf("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    Serial.printf("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");
}

bool Clock::ConfigureFromSection(const ConfigSection& section) {
    // Parse configuration from INI section
    moduleConfig.format = section.getValue("format", "24h");
    moduleConfig.showSeconds = section.getBoolValue("showSeconds", true);
    moduleConfig.syncInterval = section.getIntValue("syncInterval", 3600);
    moduleConfig.timezone = section.getValue("timezone", "");
    moduleConfig.systemTimezone = section.getValue("systemTimezone", "UTC");
    moduleConfig.positionX = section.getIntValue("position_x", 0);
    moduleConfig.positionY = section.getIntValue("position_y", 0);
    moduleConfig.width = section.getIntValue("width", 128);
    moduleConfig.height = section.getIntValue("height", 64);
    moduleConfig.enable = section.getBoolValue("enable", false);

    // Validation
    if (moduleConfig.format != "12h" && moduleConfig.format != "24h") {
        Serial.println("Clock: Invalid format, using 24h");
        moduleConfig.format = "24h";
    }

    if (moduleConfig.syncInterval < 60) {
        Serial.println("Clock: Sync interval too low, using 3600 seconds");
        moduleConfig.syncInterval = 3600;
    }

    Serial.println("Clock configured from INI section");
    Serial.printf("  Format: %s\n", moduleConfig.format.c_str());
    Serial.printf("  Show seconds: %s\n", moduleConfig.showSeconds ? "YES" : "NO");
    Serial.printf("  Sync interval: %d seconds\n", moduleConfig.syncInterval);
    Serial.printf("  Timezone: %s\n", moduleConfig.timezone.isEmpty() ? "DEFAULT" : moduleConfig.timezone.c_str());
    Serial.printf("  System Timezone: %s\n", moduleConfig.systemTimezone.c_str());
    Serial.printf("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    Serial.printf("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    Serial.printf("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");

    return true;
}

void Clock::Setup() { Serial.println("Clock Setup"); }

void Clock::Run(void* parameter) {
    Serial.println("Clock Run");

    // Wait for configuration to be ready
    ConfigManager* configManager = ConfigManager::getInstance();
    while (!configManager->IsReady()) {
        Serial.println("Waiting for config to be ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Re-configure from INI section now that config is ready
    ConfigSection moduleSection = configManager->getConfigSection("clock");
    // Add system timezone to the section
    moduleSection.keyValuePairs["systemTimezone"] = configManager->getSystemTimezone();
    
    if (!ConfigureFromSection(moduleSection)) {
        Serial.println("Failed to re-configure Clock module after config ready");
        vTaskDelete(NULL);
        return;
    }

    // Wait for WiFi connection
    while (!WiFiManager::IsConnected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!moduleConfig.enable) {
        vTaskDelete(NULL);
        return;
    }

    ready = true;
    configTime(0, 0, "pool.ntp.org");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
    return;
}

void Clock::Draw() {
    // Get current time and display it
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Apply timezone offset if configured
        String timezone = moduleConfig.timezone;
        if (timezone == "") {
            timezone = moduleConfig.systemTimezone;
        }

        if (timezone != "") {
            // Get current time as timestamp
            time_t currentTime = mktime(&timeinfo);

            // Apply timezone offset
            int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
            currentTime += timezoneOffset;

            // Convert back to tm structure
            struct tm* adjustedTime = gmtime(&currentTime);
            if (adjustedTime != nullptr) {
                timeinfo = *adjustedTime;
            }
        }

        char timeString[64];

        if (moduleConfig.format == "12h") {
            if (moduleConfig.showSeconds) {
                strftime(timeString, sizeof(timeString), "%I:%M:%S %p", &timeinfo);
            } else {
                strftime(timeString, sizeof(timeString), "%I:%M %p", &timeinfo);
            }
        } else {
            if (moduleConfig.showSeconds) {
                strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
            } else {
                strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo);
            }
        }
        if (moduleConfig.width > 100 && moduleConfig.height > 50) {
            u8g2.setFont(u8g2_font_logisoso24_tn);
        } else if (moduleConfig.width > 60 && moduleConfig.height > 30) {
            u8g2.setFont(u8g2_font_9x6LED_tr);
        } else {
            u8g2.setFont(u8g2_font_6x10_tr);
        }

        // Calculate text dimensions
        int textWidth = u8g2.getStrWidth(timeString);
        int textHeight = u8g2.getMaxCharHeight();

        // Center the text within the specified area
        int centerX = moduleConfig.positionX + (moduleConfig.width - textWidth) / 2;
        int centerY = moduleConfig.positionY + (moduleConfig.height - textHeight) / 2 + u8g2.getAscent();

        u8g2.drawStr(centerX, centerY, timeString);
    } else {
        Serial.println("Time not available");
    }
}

bool Clock::IsReady() { return ready; }

}  // namespace modules
