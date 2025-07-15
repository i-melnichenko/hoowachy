#include "clock.h"
#include "logger.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <time.h>
#include "../config_manager.h"
#include "../timezone_utils.h"
#include "../wifi_manager.h"
#include "module_registry.h"

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

namespace modules {

void Clock::Configure(const ModuleConfig& config) {
    // Cast to specific config type and store
    const ClockConfig& clockConfig = static_cast<const ClockConfig&>(config);
    moduleConfig = clockConfig;

    LOG_INFO("Clock module configured");
    LOG_INFOF("  Format: %s\n", moduleConfig.format.c_str());
    LOG_INFOF("  Show seconds: %s\n", moduleConfig.showSeconds ? "YES" : "NO");
    LOG_INFOF("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    LOG_INFOF("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    LOG_INFOF("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");
}

bool Clock::ConfigureFromSection(const ConfigSection& section) {
    // Parse configuration from INI section
    moduleConfig.format = section.getValue("format", "24h");
    moduleConfig.showSeconds = section.getBoolValue("showSeconds", true);
    moduleConfig.syncInterval = section.getIntValue("syncInterval", 3600);
    moduleConfig.positionX = section.getIntValue("position_x", 0);
    moduleConfig.positionY = section.getIntValue("position_y", 0);
    moduleConfig.width = section.getIntValue("width", 128);
    moduleConfig.height = section.getIntValue("height", 64);
    moduleConfig.enable = section.getBoolValue("enable", false);

    // Validation
    if (moduleConfig.format != "12h" && moduleConfig.format != "24h") {
        LOG_INFO("Clock: Invalid format, using 24h");
        moduleConfig.format = "24h";
    }

    if (moduleConfig.syncInterval < 60) {
        LOG_INFO("Clock: Sync interval too low, using 3600 seconds");
        moduleConfig.syncInterval = 3600;
    }

    LOG_INFO("Clock configured from INI section");
    LOG_INFOF("  Format: %s\n", moduleConfig.format.c_str());
    LOG_INFOF("  Show seconds: %s\n", moduleConfig.showSeconds ? "YES" : "NO");
    LOG_INFOF("  Sync interval: %d seconds\n", moduleConfig.syncInterval);
    LOG_INFOF("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    LOG_INFOF("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    LOG_INFOF("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");

    return true;
}

void Clock::Setup() { LOG_INFO("Clock Setup"); }

void Clock::Run(void* parameter) {
    LOG_INFO("Clock Run");

    // Wait for configuration to be ready
    ConfigManager* configManager = ConfigManager::getInstance();
    while (!configManager->IsReady()) {
        LOG_INFO("Waiting for config to be ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Re-configure from INI section now that config is ready
    ConfigSection moduleSection = configManager->getConfigSection("clock");
    
    if (!ConfigureFromSection(moduleSection)) {
        LOG_INFO("Failed to re-configure Clock module after config ready");
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
    LOG_INFO("Clock module ready - using system time sync");

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
        // Always use system timezone from ConfigManager
        ConfigManager* configManager = ConfigManager::getInstance();
        String timezone = configManager->getSystemTimezone();

        if (timezone != "") {
            // getLocalTime() returns UTC time since configTime was called with (0,0)
            // Convert tm to timestamp treating it as UTC time
            // Using portable implementation instead of timegm()
            time_t utcTime = mktime(&timeinfo) - timezone_offset_from_mktime_to_utc();
            
            // Apply timezone offset to get local time
            int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
            time_t localTime = utcTime + timezoneOffset;

            // Convert back to tm structure
            struct tm* adjustedTime = gmtime(&localTime);
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
        LOG_INFO("Time not available");
    }
}

// Helper function to get timezone offset that mktime would apply
// This helps us convert mktime result back to UTC
time_t Clock::timezone_offset_from_mktime_to_utc() {
    // Since we called configTime(0, 0, ...), mktime should work as if we're in UTC
    // But to be safe, we calculate the offset
    time_t now = time(nullptr);
    struct tm* utc_tm = gmtime(&now);
    if (!utc_tm) return 0;
    
    time_t utc_time = mktime(utc_tm);
    return utc_time - now;
}

bool Clock::IsReady() { return ready; }

}  // namespace modules
