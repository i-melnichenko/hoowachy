#include "accuweather.h"
#include "logger.h"
#include "memory_manager.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "../event_manager.h"
#include "../wifi_manager.h"
#include "../config_manager.h"
#include "../timezone_utils.h"
#include "module_registry.h"
#include "weather_icons.h"

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

namespace modules {

// Global cleanup function for AccuWeather memory management  
static void accuWeatherCleanupCallback() {
    LOG_INFO("AccuWeather: Memory cleanup callback triggered");
    
    // Trigger garbage collection for weather data
    ESP.getFreeHeap(); // Trigger garbage collection
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP.getFreeHeap(); // Second trigger
    
    LOG_INFO("AccuWeather: Cleanup callback completed");
}

void AccuWeather::Configure(const ModuleConfig& config) {
    // Cast to specific config type and store
    const AccuWeatherConfig& weatherConfig = static_cast<const AccuWeatherConfig&>(config);
    moduleConfig = weatherConfig;

    LOG_INFO("AccuWeather module configured");
    LOG_INFOF("  API Key: %s\n", moduleConfig.apiKey.isEmpty() ? "NOT SET" : "SET");
    LOG_INFOF("  City: %s\n", moduleConfig.city.c_str());
    LOG_INFOF("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    LOG_INFOF("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    LOG_INFOF("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");
}

bool AccuWeather::ConfigureFromSection(const ConfigSection& section) {
    LOG_INFO("AccuWeather configured from INI section");
    
    // Debug: Print all key-value pairs in the section
    LOG_INFO("Debug: All config section key-value pairs:");
    for (const auto& pair : section.keyValuePairs) {
        LOG_INFOF("  '%s' = '%s'\n", pair.first.c_str(), pair.second.c_str());
    }
    
    // Parse configuration from INI section
    moduleConfig.apiKey = section.getValue("api_key", "");
    moduleConfig.city = section.getValue("city", "");
    moduleConfig.timezone = section.getValue("timezone", "");
    moduleConfig.systemTimezone = section.getValue("systemTimezone", "UTC");
    moduleConfig.positionX = section.getIntValue("position_x", 0);
    moduleConfig.positionY = section.getIntValue("position_y", 0);
    moduleConfig.width = section.getIntValue("width", 128);
    moduleConfig.height = section.getIntValue("height", 64);
    moduleConfig.enable = section.getBoolValue("enable", false);

    // Debug: Print what we actually got
    LOG_INFOF("Debug: api_key value = '%s' (length: %d)\n", moduleConfig.apiKey.c_str(), moduleConfig.apiKey.length());
    LOG_INFOF("Debug: city value = '%s' (length: %d)\n", moduleConfig.city.c_str(), moduleConfig.city.length());

    // Validation
    if (moduleConfig.apiKey.isEmpty()) {
        LOG_INFO("AccuWeather: API key is required");
        return false;
    }

    if (moduleConfig.city.isEmpty()) {
        LOG_INFO("AccuWeather: City is required");
        return false;
    }

    LOG_INFO("AccuWeather configured from INI section");
    LOG_INFOF("  API Key: %s\n", moduleConfig.apiKey.isEmpty() ? "NOT SET" : "SET");
    LOG_INFOF("  City: %s\n", moduleConfig.city.c_str());
    LOG_INFOF("  Timezone: %s\n", moduleConfig.timezone.c_str());
    LOG_INFOF("  System Timezone: %s\n", moduleConfig.systemTimezone.c_str());
    LOG_INFOF("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    LOG_INFOF("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    LOG_INFOF("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");

    return true;
}

void AccuWeather::Setup() {
    // Weather setup initialization
    LOG_INFO("AccuWeather module setup");
    loadFromEEPROM();
    LOG_INFO("Forecasts loaded from EEPROM");

    // Register cleanup callback with MemoryManager
    MemoryManager::getInstance()->registerCleanupCallback("AccuWeather", accuWeatherCleanupCallback);
}

void AccuWeather::saveToEEPROM() {
    LOG_INFO("Saving forecasts to EEPROM...");

    // Write magic number to indicate valid data
    uint32_t magic = 0xABCD1234;
    EEPROM.put(EEPROM_FORECAST_START, magic);

    // Write Unix timestamp when data was saved
    lastDataSaveTime = time(nullptr);
    LOG_INFOF("Saving lastDataSaveTime to EEPROM: %ld (Unix timestamp)\n", lastDataSaveTime);
    
    // Convert to human readable time for debugging
    struct tm* timeinfo = localtime(&lastDataSaveTime);
    if (timeinfo) {
        char timeString[64];
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
        LOG_INFOF("Data saved at: %s\n", timeString);
    }
    
    EEPROM.put(EEPROM_FORECAST_START + sizeof(magic), lastDataSaveTime);

    // Write forecasts data
    int address = EEPROM_FORECAST_START + sizeof(magic) + sizeof(lastDataSaveTime);
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        EEPROM.put(address, forecasts[i]);
        address += sizeof(Forecast);
    }

    EEPROM.commit();
    LOG_INFO("Forecasts saved to EEPROM successfully");
}

void AccuWeather::loadFromEEPROM() {
    LOG_INFO("Loading forecasts from EEPROM...");

    // Read magic number to check if data is valid
    uint32_t magic;
    EEPROM.get(EEPROM_FORECAST_START, magic);

    if (magic != 0xABCD1234) {
        LOG_INFO("No valid forecast data found in EEPROM, initializing empty forecasts");

        // Initialize with empty forecasts
        for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
            forecasts[i] = Forecast();
        }
        lastDataSaveTime = 0;  // Mark as no data
        return;
    }

    // Read Unix timestamp when data was saved
    EEPROM.get(EEPROM_FORECAST_START + sizeof(magic), lastDataSaveTime);
    LOG_INFOF("Loaded lastDataSaveTime from EEPROM: %ld (Unix timestamp)\n", lastDataSaveTime);
    
    // Convert to human readable time for debugging
    if (lastDataSaveTime > 0) {
        struct tm* timeinfo = localtime(&lastDataSaveTime);
        if (timeinfo) {
            char timeString[64];
            strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
            LOG_INFOF("Data was saved at: %s\n", timeString);
        }
    }
    
    // Show current time for comparison
    time_t currentTime = time(nullptr);
    if (currentTime > 0) {
        struct tm* currentTimeinfo = localtime(&currentTime);
        if (currentTimeinfo) {
            char currentTimeString[64];
            strftime(currentTimeString, sizeof(currentTimeString), "%Y-%m-%d %H:%M:%S", currentTimeinfo);
            LOG_INFOF("Current time: %s\n", currentTimeString);
        }
    }

    // Read forecasts data
    int address = EEPROM_FORECAST_START + sizeof(magic) + sizeof(lastDataSaveTime);
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        EEPROM.get(address, forecasts[i]);
        address += sizeof(Forecast);
    }

    LOG_INFOF("Successfully loaded %d forecasts from EEPROM\n", 6);
    LOG_INFOF("Data saved at: %lu, current time: %lu\n", lastDataSaveTime, millis());

    // Check data freshness immediately after loading
    LOG_INFOF("Is data fresh after loading? %s\n", isDataFresh() ? "YES" : "NO");

    // Print loaded data for verification
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        if (forecasts[i].time != 0) {
            LOG_INFOF("Forecast %d: temp=%d, humidity=%d, icon=%d, phrase=%.20s\n", i, forecasts[i].temperature,
                          forecasts[i].humidity, forecasts[i].icon, forecasts[i].phrase);
        } else {
            LOG_INFOF("Forecast %d: EMPTY (time=0)\n", i);
        }
    }
    
    // Debug: Print all forecasts regardless of time value
    LOG_INFO("[AccuWeather loadFromEEPROM] All forecast data loaded:");
    for (int i = 0; i < 6; i++) {
        LOG_INFOF("  Forecast %d: time=%ld, temp=%d, humidity=%d, icon=%d, phrase=%.20s\n", 
                     i, forecasts[i].time, forecasts[i].temperature, 
                     forecasts[i].humidity, forecasts[i].icon, forecasts[i].phrase);
    }
}
    
void AccuWeather::Run(void* parameter) {
    LOG_INFO("Weather Run");

     ConfigManager* configManager = ConfigManager::getInstance();
    
    // Re-configure from INI section now that config is ready
    ConfigSection moduleSection = configManager->getConfigSection("accuweather");
    // Add system timezone to the section - FORCE it to override any existing value
    moduleSection.keyValuePairs["systemTimezone"] = configManager->getSystemTimezone();
    
    if (!ConfigureFromSection(moduleSection)) {
        LOG_INFO("Failed to re-configure AccuWeather module after config ready");
        vTaskDelete(NULL);
        return;
    }
    
    // Double-check: Force system timezone from config manager (bypass section parsing)
    moduleConfig.systemTimezone = configManager->getSystemTimezone();
    LOG_INFOF("AccuWeather: Forced systemTimezone to '%s'\n", moduleConfig.systemTimezone.c_str());

    // Wait for WiFi connection
    while (!WiFiManager::IsConnected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!moduleConfig.enable) {
        vTaskDelete(NULL);
        return;
    }

    TerminalEvent event(0, "AW", "Load data from EEPROM", TerminalEvent::State::SUCCESS);
    EventManager::Emit(event);
    ready = true;

    // Check if EEPROM data is fresh (less than 2 hours old), if not - fetch immediately
    if (!isDataFresh()) {
        LOG_INFO("EEPROM data is older than 2 hours, fetching fresh weather data on startup...");
        if (!fetchWeatherData()) {
            LOG_INFO("Failed to fetch fresh data on startup, using stale EEPROM data");
            TerminalEvent startupEvent(0, "AW", "Using stale cached data", TerminalEvent::State::PROCESSING);
            EventManager::Emit(startupEvent);
        }
    } else {
        LOG_INFO("EEPROM data is fresh (less than 2 hours old), skipping immediate fetch");
        TerminalEvent freshEvent(0, "AW", "Using fresh cached data", TerminalEvent::State::SUCCESS);
        EventManager::Emit(freshEvent);
    }

    while (true) {
        // Always wait 30 minutes between API requests
        const unsigned long waitTimeMinutes = 30;
        unsigned long waitTime = waitTimeMinutes * 60 * 1000; // Convert to milliseconds
        
        LOG_INFOF("Waiting %lu minutes until next update...\n", waitTimeMinutes);
        vTaskDelay(pdMS_TO_TICKS(waitTime));
        
        LOG_INFO("Scheduled weather data update...");
        if (!fetchWeatherData()) {
            LOG_INFO("Scheduled update failed, retrying in 30 seconds");
            vTaskDelay(pdMS_TO_TICKS(30 * 1000));
            ready = false;
            continue;
        }
        ready = true;
    }
    vTaskDelete(NULL);
    return;
}

void AccuWeather::Draw() {
    int xPos = moduleConfig.positionX;
    int yPos = moduleConfig.positionY;
    int width = moduleConfig.width;
    int height = moduleConfig.height;

    // Debug: check if we have valid data
    if (!ready || forecasts[0].time == 0) {
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(xPos, yPos + 8, "No Weather");
        u8g2.drawStr(xPos, yPos + 16, "Data");
        return;
    }

    // Filter forecasts to show only current and future hours
    int validForecasts[6];
    int validCount = 0;
    
    // Get current time for filtering (same logic as in parsing)
    time_t currentTime = time(nullptr);
    ConfigManager* configManager = ConfigManager::getInstance();
    String timezone = configManager->getSystemTimezone();
    int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
    time_t localCurrentTime = currentTime + timezoneOffset;
    time_t currentHourTimeUTC = (localCurrentTime / 3600) * 3600 - timezoneOffset;
    
    // Find valid forecasts (from next hour onwards, not current hour)
    time_t nextHourTimeUTC = currentHourTimeUTC + 3600; // Add 1 hour
    for (int i = 0; i < 6; i++) {
        if (forecasts[i].time != 0 && forecasts[i].time >= nextHourTimeUTC) {
            validForecasts[validCount] = i;
            validCount++;
            if (validCount >= 2) break; // We only show 2 forecasts
        }
    }
    

    
    if (validCount == 0) {
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(xPos, yPos + 8, "No Current");
        u8g2.drawStr(xPos, yPos + 16, "Weather");
        return;
    }

    u8g2.setFont(u8g2_font_4x6_tr);
    for (int i = 0; i < 2 && i < validCount; i++) {
        int forecastIndex = validForecasts[i];
        const uint8_t* forecastIcon = weatherIcon(forecasts[forecastIndex].icon);
        int currentYPos = yPos + (i * 16);  // Fixed positioning calculation

        if (i == 0) {
            unsigned long animationTime = millis();
            float scale = 1.0f + 0.1f * sin(animationTime / 1000.0f);

            int iconSize = 19;

            for (int y = 0; y < iconSize; y++) {
                for (int x = 0; x < iconSize; x++) {
                    int srcX = ((x * 18) / iconSize) / scale;
                    int srcY = ((y * 18) / iconSize) / scale;
                    if (srcX < 16 && srcY < 16) {
                        int byteIndex = srcY * 2 + srcX / 8;
                        int bitIndex = srcX % 8;
                        if (forecastIcon[byteIndex] & (1 << bitIndex)) {
                            u8g2.drawPixel(xPos + x, currentYPos + y);
                        }
                    }
                }
            }

        } else {
            u8g2.drawXBMP(xPos, currentYPos, 16, 16, forecastIcon);
        }

        u8g2.setFont(u8g2_font_4x6_tr);
        char time_buffer[8];

        // Convert Unix timestamp to HH:MM format with timezone
        if (forecasts[forecastIndex].time != 0) {
            time_t timestamp = forecasts[forecastIndex].time;

            // Always use system timezone from ConfigManager
            ConfigManager* configManager = ConfigManager::getInstance();
            String timezone = configManager->getSystemTimezone();

            // Apply timezone offset
            int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
            timestamp += timezoneOffset;

            struct tm* timeinfo = gmtime(&timestamp);
            snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
        } else {
            snprintf(time_buffer, sizeof(time_buffer), "--:--");
        }

        u8g2.drawStr(xPos + 21, currentYPos + 6, time_buffer);
        char forecast_temp_buffer[8];
        snprintf(forecast_temp_buffer, sizeof(forecast_temp_buffer), "%d°C", forecasts[forecastIndex].temperature);
        u8g2.drawStr(xPos + 21, currentYPos + 14, forecast_temp_buffer);
        char forecast_humidity_buffer[8];
        snprintf(forecast_humidity_buffer, sizeof(forecast_humidity_buffer), "%d%%", forecasts[forecastIndex].humidity);
        u8g2.drawStr(xPos + 40, currentYPos + 14, forecast_humidity_buffer);
    }
}

// Forecast management methods
void AccuWeather::updateForecast(int index, const long time, int temperature, int humidity, const char* phrase,
                                 int icon) {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        LOG_INFOF("Invalid forecast index: %d\n", index);
        return;
    }

    LOG_INFOF("[AccuWeather updateForecast] Updating index %d with: time=%ld, temp=%d, humidity=%d, icon=%d\n", 
                 index, time, temperature, humidity, icon);

    forecasts[index].time = time;
    forecasts[index].temperature = temperature;
    forecasts[index].humidity = humidity;
    forecasts[index].icon = icon;

    // Safely copy phrase with null termination
    if (phrase != nullptr) {
        strncpy(forecasts[index].phrase, phrase, sizeof(forecasts[index].phrase) - 1);
        forecasts[index].phrase[sizeof(forecasts[index].phrase) - 1] = '\0';
    } else {
        forecasts[index].phrase[0] = '\0';
    }

    LOG_INFOF("Updated forecast %d: temp=%d, humidity=%d, icon=%d\n", index, temperature, humidity, icon);
    LOG_INFOF("[AccuWeather updateForecast] Forecast array after update: time=%ld, temp=%d, humidity=%d\n", 
                 forecasts[index].time, forecasts[index].temperature, forecasts[index].humidity);

    // Temporarily disable automatic EEPROM save to avoid issues with partial data
    // saveToEEPROM();
}

void AccuWeather::updateForecast(int index, const Forecast& forecast) {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        LOG_INFOF("Invalid forecast index: %d\n", index);
        return;
    }

    forecasts[index] = forecast;

    LOG_INFOF("Updated forecast %d from Forecast object\n", index);

    // Automatically save to EEPROM after update
    saveToEEPROM();
}

AccuWeather::Forecast AccuWeather::getForecast(int index) const {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        LOG_INFOF("Invalid forecast index: %d\n", index);
        return Forecast();  // Return empty forecast
    }

    return forecasts[index];
}

void AccuWeather::clearForecasts() {
    LOG_INFO("Clearing all forecasts...");

    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        forecasts[i] = Forecast();
    }

    // Save cleared state to EEPROM
    saveToEEPROM();
}

bool AccuWeather::hasForecastData() const {
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        if (forecasts[i].time != 0) {
            return true;
        }
    }
    return false;
}

int AccuWeather::getValidForecastCount() const {
    int count = 0;
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        if (forecasts[i].time != 0) {
            count++;
        }
    }
    return count;
}

const uint8_t* AccuWeather::weatherIcon(int p) {
    switch (p) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            return Sunny_01_16;
            break;
        case 6:
            return Mostly_Cloudy_06_16;
            break;
        case 7:
        case 8:
            return Cloudy_07_16;
            break;
        case 11:
            return Fog_11_16;
            break;
        case 12:
            return Showers_12_16;
            break;
        case 13:
            return Mostly_Cloudy_with_Showers_13_16;
            break;
        case 14:
            return ePartly_Sunny_with_Showers_14_16;
            break;
        case 15:
            return Thunderstorms_15_16;
            break;
        case 16:
            return Mostly_Cloudy_with_Thundershowers_16_16;
            break;
        case 17:
            return Partly_Cloudy_with_Thundershowers_17_16;
            break;
        case 18:
            return Rain_18_16;
            break;
        case 19:
            return Flurries_19_16;
            break;
        case 20:
            return Mostly_Cloudy_w_Flurries_20_16;
            break;
        case 21:
            return Sunny_w_Flurries_21_16;
            break;
        case 22:
            return Snow_22_16;
            break;
        case 23:
            return Mostly_Cloudy_with_Snow_23_16;
            break;
        case 24:
        case 25:
        case 26:
            return Sleet_24_26_16;
            break;
        case 29:
            return Rain___Snow_Mix_29_16;
            break;
        case 30:
            return Hot_30_16;
            break;
        case 31:
            return Cold_31_16;
            break;
        case 32:
            return Windy_32_16;
            break;
        case 33:
            return Clear_Moon_33_16;
            break;
        case 34:
            return Mostly_Clear_Night_34_16;
            break;
        case 35:
            return Partly_Cloudy_Night_35_16;
            break;
        case 36:
            return Intermittent_Clouds_Night_36_16;
            break;
        case 37:
            return Hazy_Night_37_16;
            break;
        case 38:
            return Mostly_Cloudy_Night_38_16;
            break;
        case 39:
            return Partly_Cloudy_with_Showers_Night_39_16;
            break;
        case 40:
            return Mostly_Cloudy_w_Showers_Night_40_16;
            break;
        case 41:
            return Partly_Cloudy_with_Thundershowers_Night_41_16;
            break;
        case 42:
            return Mostly_Cloudy_with_Thundershowers_Night_42_16;
            break;
        case 43:
            return Mostly_Cloudy_with_Flurries_Night_43_16;
            break;
        case 44:
            return Mostly_Cloudy_with_Snow_Night_44_16;
            break;
    }
    return Sunny_01_16;
}

bool AccuWeather::IsReady() { return ready; }

bool AccuWeather::isDataFresh() const {
    // Check if we have no data saved
    if (lastDataSaveTime == 0) {
        LOG_INFO("No data saved, data is not fresh");
        return false;
    }

    // Get current Unix timestamp
    time_t currentTime = time(nullptr);
    
    // Check if system time is synchronized (should be > year 2020)
    if (currentTime < 1577836800) {  // Jan 1, 2020 00:00:00 UTC
        LOG_INFO("System time not synchronized, treating data as stale");
        return false;
    }

    // Calculate age in seconds
    time_t dataAge = currentTime - lastDataSaveTime;
    const time_t TWO_HOURS = 2 * 60 * 60;  // 2 hours in seconds

    // Convert to human readable for debugging
    struct tm* savedTimeinfo = localtime(&lastDataSaveTime);
    struct tm* currentTimeinfo = localtime(&currentTime);
    
    if (savedTimeinfo && currentTimeinfo) {
        char savedTimeStr[64], currentTimeStr[64];
        strftime(savedTimeStr, sizeof(savedTimeStr), "%H:%M:%S", savedTimeinfo);
        strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M:%S", currentTimeinfo);
        LOG_INFOF("Data saved at: %s, current time: %s, age: %ld seconds\n", 
                     savedTimeStr, currentTimeStr, dataAge);
    }

    if (dataAge < TWO_HOURS && dataAge >= 0) {
        LOG_INFOF("Data is fresh: age=%ld seconds (<%ld seconds)\n", dataAge, TWO_HOURS);
        return true;
    } else {
        LOG_INFOF("Data is stale: age=%ld seconds (>=%ld seconds)\n", dataAge, TWO_HOURS);
        return false;
    }
}

bool AccuWeather::fetchWeatherData() {
    // Request memory for weather data fetch operation
    if (!MEMORY_REQUEST(MemoryManager::Operation::HTTP_REQUEST, 
                       MemoryManager::Priority::NORMAL, 
                       8192, "AccuWeather-Fetch")) {
        LOG_INFO("AccuWeather: Cannot get memory for weather fetch, skipping");
        TerminalEvent event(0, "AW", "Memory unavailable", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    // Check if API key and city are configured
    if (moduleConfig.apiKey.isEmpty() || moduleConfig.city.isEmpty()) {
        LOG_INFO("AccuWeather API key or city not configured");
        LOG_INFOF("API key: %s\n", moduleConfig.apiKey.isEmpty() ? "EMPTY" : "SET");
        LOG_INFOF("City: %s\n", moduleConfig.city.isEmpty() ? "EMPTY" : moduleConfig.city.c_str());
        TerminalEvent event(0, "AW", "API key or city not configured", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        
        MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
        return false;
    }

    // Verify WiFi connection before making request
    if (!WiFiManager::IsConnected()) {
        LOG_INFO("[AccuWeather] WiFi not connected, cannot fetch weather data");
        TerminalEvent event(0, "AW", "WiFi not connected", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        
        MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
        return false;
    }
    
    LOG_INFOF("[AccuWeather] WiFi status: %d, RSSI: %d dBm\n", WiFi.status(), WiFi.RSSI());

    // Create HTTP client
    HTTPClient http;

    // Build URL for 12-hour forecast with details to ensure all fields are present
    String url = "http://dataservice.accuweather.com/forecasts/v1/hourly/12hour/" + moduleConfig.city +
                 "?apikey=" + moduleConfig.apiKey + "&language=en-us&details=true&metric=true";

    LOG_INFO("Fetching weather data from AccuWeather API");
    LOG_INFOF("City ID: %s\n", moduleConfig.city.c_str());
    LOG_INFOF("API Key: %s\n", moduleConfig.apiKey.c_str());
    LOG_INFOF("Full URL: %s\n", url.c_str());
    LOG_INFOF("URL length: %d\n", url.length());

    // Configure HTTP client
    http.begin(url);
    http.setTimeout(10000);  // 10 seconds timeout
    
    // Add headers for better compatibility - use standard browser User-Agent
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity"); // Disable compression to avoid issues
    http.addHeader("Connection", "close");

    // Send GET request
    int httpCode = http.GET();

    LOG_INFOF("HTTP response code: %d\n", httpCode);
    
    // Debug: Print response headers
    String contentLength = http.header("Content-Length");
    String contentType = http.header("Content-Type");
    String server = http.header("Server");
    LOG_INFOF("Content-Length: %s\n", contentLength.c_str());
    LOG_INFOF("Content-Type: %s\n", contentType.c_str());
    LOG_INFOF("Server: %s\n", server.c_str());
    LOG_INFOF("Response size from getSize(): %d\n", http.getSize());

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = http.getSize();
            LOG_INFOF("[AccuWeather] Expected content length: %d bytes\n", contentLength);
            
            String payload = "";
            
            if (contentLength > 0) {
                // Method 1: Read in small chunks to avoid memory issues
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    LOG_INFOF("[AccuWeather] Free heap before reading: %d bytes\n", ESP.getFreeHeap());
                    
                    const int CHUNK_SIZE = 1024; // Read 1KB at a time
                    char chunk[CHUNK_SIZE + 1];
                    int totalBytesRead = 0;
                    
                    unsigned long startTime = millis();
                    const unsigned long timeout = 10000; // 10 seconds
                    
                    LOG_INFO("[AccuWeather] Starting chunked read...");
                    payload.reserve(contentLength > 8192 ? 8192 : contentLength); // Reserve reasonable amount
                    
                    while (totalBytesRead < contentLength && (millis() - startTime < timeout)) {
                        if (stream->available()) {
                            int available = stream->available();
                            int toRead = min(available, min(CHUNK_SIZE, contentLength - totalBytesRead));
                            int actualRead = stream->readBytes(chunk, toRead);
                            
                            if (actualRead > 0) {
                                // Create String from buffer with explicit length to avoid null-termination issues
                                String chunkStr = "";
                                chunkStr.reserve(actualRead + 10);
                                for (int i = 0; i < actualRead; i++) {
                                    chunkStr += (char)chunk[i];
                                }
                                payload += chunkStr;
                                totalBytesRead += actualRead;
                                
                                LOG_INFOF("[AccuWeather] Read chunk %d bytes, total: %d/%d, heap: %d\n", 
                                             actualRead, totalBytesRead, contentLength, ESP.getFreeHeap());
                            }
                        } else {
                            delay(10);
                        }
                    }
                    
                    LOG_INFOF("[AccuWeather] Chunked read completed: %d bytes, String length: %d\n", 
                                 totalBytesRead, payload.length());
                }
                LOG_INFOF("[AccuWeather] Stream method: payload size: %d bytes\n", payload.length());
            }
            
            // Fallback to getString() if stream method failed or no content-length
            if (payload.length() == 0) {
                LOG_INFO("[AccuWeather] Trying getString() method...");
                payload = http.getString();
                LOG_INFOF("[AccuWeather] getString() method: payload size: %d bytes\n", payload.length());
            }
            
            LOG_INFOF("[AccuWeather] Final payload size: %d bytes\n", payload.length());

            // Check if response is empty
            if (payload.length() == 0) {
                LOG_INFO("[AccuWeather] Empty response from API");
                TerminalEvent event(0, "AW", "Empty API response", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
                http.end();
                MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
                return false;
            }

            // Check if response looks like JSON
            payload.trim(); // Remove any leading/trailing whitespace
            if (!payload.startsWith("{") && !payload.startsWith("[")) {
                LOG_INFO("[AccuWeather] Response is not JSON format");
                LOG_INFOF("[AccuWeather] Response content: %s\n", payload.c_str());
                TerminalEvent event(0, "AW", "Invalid response format", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
                http.end();
                MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
                return false;
            }

            // Parse JSON response
            LOG_INFO("[AccuWeather] Starting JSON parsing...");
            if (!parseWeatherData(payload)) {
                LOG_INFO("[AccuWeather] JSON parsing failed!");
                http.end();
                MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
                return false;
            }
            LOG_INFO("[AccuWeather] JSON parsing completed successfully!");

            TerminalEvent event(0, "AW", "Weather data updated", TerminalEvent::State::SUCCESS);
            EventManager::Emit(event);
            http.end();
            MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
            return true;
        } else {
            // Get error response for debugging
            String errorResponse = http.getString();
            LOG_INFOF("HTTP error: %d\n", httpCode);
            LOG_INFOF("Error response: %s\n", errorResponse.c_str());
            
            // Handle specific AccuWeather API error codes
            if (httpCode == 401) {
                LOG_INFO("Invalid API key");
                TerminalEvent event(0, "AW", "Invalid API key", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else if (httpCode == 400) {
                LOG_INFO("Bad request - check city ID");
                TerminalEvent event(0, "AW", "Bad request", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else if (httpCode == 403) {
                LOG_INFO("API key exceeded quota");
                TerminalEvent event(0, "AW", "API quota exceeded", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else {
                TerminalEvent event(0, "AW", "HTTP error " + String(httpCode), TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            }
            
            http.end();
            MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
            return false;
        }
    } else {
        LOG_INFOF("HTTP connection failed: %s\n", http.errorToString(httpCode).c_str());
        TerminalEvent event(0, "AW", "Connection failed", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        http.end();
        MEMORY_RELEASE(MemoryManager::Operation::HTTP_REQUEST, "AccuWeather-Fetch");
        return false;
    }
}

bool AccuWeather::parseWeatherData(const String& jsonData) {
    // Request memory for JSON parsing operation - reduced size
    if (!MEMORY_REQUEST(MemoryManager::Operation::JSON_PARSING, 
                       MemoryManager::Priority::NORMAL, 
                       8192, "AccuWeather-Parse")) {
        LOG_INFO("AccuWeather: Cannot get memory for JSON parsing, skipping");
        TerminalEvent event(0, "AW", "Memory unavailable for parsing", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    // Log JSON data size for debugging
    LOG_INFOF("[AccuWeather] JSON data size: %d bytes\n", jsonData.length());
    
    // Log first 200 characters of JSON for debugging
    LOG_INFO("[AccuWeather] JSON response preview:");
    LOG_INFO(jsonData.substring(0, 200));
    
    // Check if response looks like an error
    if (jsonData.indexOf("\"fault\"") != -1 || jsonData.indexOf("\"error\"") != -1) {
        LOG_INFO("AccuWeather API returned error response");
        LOG_INFO("Full response: " + jsonData);
        TerminalEvent event(0, "AW", "API returned error", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        
        MEMORY_RELEASE(MemoryManager::Operation::JSON_PARSING, "AccuWeather-Parse");
        return false;
    }

    // Parse detailed response (12 hours, with details) - use smaller buffer
    DynamicJsonDocument doc(8 * 1024);  // 8KB - further reduced for memory constraints

    DeserializationError error = deserializeJson(doc, jsonData);
    if (error) {
        LOG_INFOF("[AccuWeather] JSON parsing failed: %s\n", error.c_str());
        LOG_INFOF("[AccuWeather] Free heap after failed parsing: %d bytes\n", ESP.getFreeHeap());
        
        // If memory error, try alternative simple parsing for first few forecasts
        if (error == DeserializationError::NoMemory) {
            LOG_INFO("[AccuWeather] Trying simple text parsing due to memory constraints...");
            return parseWeatherDataSimple(jsonData);
        }
        
        LOG_INFO("[AccuWeather] JSON that failed to parse:");
        LOG_INFO(jsonData.substring(0, 500)); // Show only first 500 chars
        TerminalEvent event(0, "AW", "JSON parsing failed", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    LOG_INFOF("[AccuWeather] JSON parsing successful, free heap: %d bytes\n", ESP.getFreeHeap());

    // Check if response is an array
    if (!doc.is<JsonArray>()) {
        LOG_INFO("[AccuWeather] API response is not an array");
        LOG_INFO("[AccuWeather] Full response: " + jsonData);
        TerminalEvent event(0, "AW", "Invalid response format", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }
    
    // Clear existing forecasts
    clearForecasts();

    // Get current time rounded to current hour for filtering (all in UTC for proper comparison)
    time_t currentTime = time(nullptr);  // This is UTC time
    
    // Get timezone offset to convert current time to local time for hour calculation
    ConfigManager* configManager = ConfigManager::getInstance();
    String timezone = configManager->getSystemTimezone();
    int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
    
    // Apply timezone offset to get local time
    time_t localCurrentTime = currentTime + timezoneOffset;
    
    // Round down to the current hour in local time using simple math
    time_t currentHourTimeUTC = (localCurrentTime / 3600) * 3600 - timezoneOffset;
    // We want to show forecasts from next hour onwards, not current hour
    time_t nextHourTimeUTC = currentHourTimeUTC + 3600;
    
    LOG_INFOF("[AccuWeather] Current UTC time: %ld, local time: %ld, current hour UTC: %ld, next hour UTC: %ld\n", 
                 currentTime, localCurrentTime, currentHourTimeUTC, nextHourTimeUTC);
    LOG_INFOF("[AccuWeather] Timezone: %s, offset: %d seconds\n", timezone.c_str(), timezoneOffset);

    // Process each forecast entry
    JsonArray forecastsArray = doc.as<JsonArray>();
    int index = 0;
    int processedEntries = 0;
    
    LOG_INFOF("[AccuWeather] Processing %d forecast entries\n", forecastsArray.size());

    for (JsonObject forecast : forecastsArray) {
        if (index >= 6) break;  // We only store first 6 out of 12 forecasts for memory optimization

        LOG_INFOF("[AccuWeather] === Processing forecast entry %d ===\n", processedEntries);
        
        // Extract forecast data with validation - detailed debugging
        LOG_INFOF("[AccuWeather] Forecast entry %d keys:\n", processedEntries);
        for (JsonPair kv : forecast) {
            LOG_INFOF("[AccuWeather]   '%s'\n", kv.key().c_str());
        }
        
        bool hasEpochDateTime = forecast.containsKey("EpochDateTime");
        bool hasDateTime = forecast.containsKey("DateTime");
        bool hasTemperature = forecast.containsKey("Temperature");
        bool hasIconPhrase = forecast.containsKey("IconPhrase");
        bool hasWeatherIcon = forecast.containsKey("WeatherIcon");
        bool hasHumidity = forecast.containsKey("RelativeHumidity");
        
        LOG_INFOF("Entry %d: EpochDateTime=%s, DateTime=%s, Temperature=%s, IconPhrase=%s, WeatherIcon=%s, Humidity=%s\n",
                     processedEntries, hasEpochDateTime ? "YES" : "NO", hasDateTime ? "YES" : "NO", 
                     hasTemperature ? "YES" : "NO", hasIconPhrase ? "YES" : "NO", hasWeatherIcon ? "YES" : "NO",
                     hasHumidity ? "YES" : "NO");
        
        if (!hasEpochDateTime && !hasDateTime) {
            LOG_INFOF("Forecast entry %d missing time field - SKIPPING\n", processedEntries);
            processedEntries++;
            continue;
        }
        if (!hasTemperature || !hasIconPhrase || !hasWeatherIcon) {
            LOG_INFOF("Forecast entry %d missing required fields - SKIPPING\n", processedEntries);
            processedEntries++;
            continue;
        }

        // Get time (prefer EpochDateTime, fallback to parsing DateTime)
        long epochTime = 0;
        if (hasEpochDateTime) {
            epochTime = forecast["EpochDateTime"];
            LOG_INFOF("Entry %d: Got EpochDateTime = %ld\n", processedEntries, epochTime);
        } else if (hasDateTime) {
            // TODO: Parse DateTime string if needed
            epochTime = 0; // For now, set to 0 to skip this entry
            LOG_INFOF("Entry %d: DateTime parsing not implemented yet - SKIPPING\n", processedEntries);
            processedEntries++;
            continue;
        }
        
        // Filter out forecasts from past and current hour (comparing UTC times)
        if (epochTime < nextHourTimeUTC) {
            LOG_INFOF("Entry %d: Forecast time %ld is before next hour %ld (UTC) - SKIPPING\n", 
                         processedEntries, epochTime, nextHourTimeUTC);
            processedEntries++;
            continue;
        }
        
        if (!forecast["Temperature"].containsKey("Value")) {
            LOG_INFOF("Forecast entry %d missing Temperature.Value - SKIPPING\n", processedEntries);
            processedEntries++;
            continue;
        }
        
        int temperature = forecast["Temperature"]["Value"];
        int humidity = hasHumidity ? (int)forecast["RelativeHumidity"] : 0;
        const char* phrase = forecast["IconPhrase"];
        int icon = forecast["WeatherIcon"];

        LOG_INFOF("[AccuWeather] Forecast %d BEFORE UPDATE: time=%ld, temp=%d, humidity=%d, icon=%d, phrase=%s\n", 
                     index, epochTime, temperature, humidity, icon, phrase);

        // Update forecast
        updateForecast(index, epochTime, temperature, humidity, phrase, icon);
        
        // Verify that the forecast was actually saved
        LOG_INFOF("[AccuWeather] Forecast %d AFTER UPDATE: time=%ld, temp=%d, humidity=%d, icon=%d\n", 
                     index, forecasts[index].time, forecasts[index].temperature, 
                     forecasts[index].humidity, forecasts[index].icon);
        
        index++;
        processedEntries++;
    }

    LOG_INFOF("[AccuWeather] Successfully parsed %d forecasts\n", index);
    
    if (index == 0) {
        LOG_INFO("[AccuWeather] No valid forecasts found in response");
        TerminalEvent event(0, "AW", "No forecasts in response", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }
    
    // Save all forecasts to EEPROM after successful parsing
    LOG_INFO("[AccuWeather] Saving all forecasts to EEPROM after parsing...");
    saveToEEPROM();
    
    // Debug: Print all forecasts after saving
    LOG_INFO("[AccuWeather] All forecasts after saving to EEPROM:");
    for (int i = 0; i < 6; i++) {
        LOG_INFOF("  Forecast %d: time=%ld, temp=%d, humidity=%d, icon=%d\n", 
                     i, forecasts[i].time, forecasts[i].temperature, 
                     forecasts[i].humidity, forecasts[i].icon);
    }
    
    MEMORY_RELEASE(MemoryManager::Operation::JSON_PARSING, "AccuWeather-Parse");
    return true;
}

bool AccuWeather::parseWeatherDataSimple(const String& jsonData) {
    LOG_INFO("[AccuWeather] Starting simple text-based parsing...");
    
    // Clear existing forecasts
    clearForecasts();
    
    // Get current time rounded to current hour for filtering (all in UTC for proper comparison)
    time_t currentTime = time(nullptr);  // This is UTC time
    
    // Get timezone offset to convert current time to local time for hour calculation  
    ConfigManager* configManager = ConfigManager::getInstance();
    String timezone = configManager->getSystemTimezone();
    int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
    
    // Apply timezone offset to get local time
    time_t localCurrentTime = currentTime + timezoneOffset;
    
    // Round down to the current hour in local time using simple math
    time_t currentHourTimeUTC = (localCurrentTime / 3600) * 3600 - timezoneOffset;
    // We want to show forecasts from next hour onwards, not current hour
    time_t nextHourTimeUTC = currentHourTimeUTC + 3600;
    
    LOG_INFOF("[AccuWeather] Simple parse - Current UTC time: %ld, local time: %ld, current hour UTC: %ld, next hour UTC: %ld\n", 
                 currentTime, localCurrentTime, currentHourTimeUTC, nextHourTimeUTC);
    LOG_INFOF("[AccuWeather] Simple parse - Timezone: %s, offset: %d seconds\n", timezone.c_str(), timezoneOffset);
    
    int forecastIndex = 0;
    int searchPos = 0;
    
    // Find up to 6 forecasts by searching for "EpochDateTime" pattern
    while (forecastIndex < 6 && searchPos < jsonData.length()) {
        // Find next EpochDateTime
        int epochPos = jsonData.indexOf("\"EpochDateTime\":", searchPos);
        if (epochPos == -1) break;
        
        // Extract epoch time
        int epochStart = epochPos + 16; // Length of "EpochDateTime":
        int epochEnd = jsonData.indexOf(",", epochStart);
        if (epochEnd == -1) epochEnd = jsonData.indexOf("}", epochStart);
        if (epochEnd == -1) break;
        
        String epochStr = jsonData.substring(epochStart, epochEnd);
        long epochTime = epochStr.toInt();
        
        // Filter out forecasts from past and current hour (comparing UTC times)
        if (epochTime < nextHourTimeUTC) {
            LOG_INFOF("[AccuWeather] Simple parse: Forecast time %ld is before next hour %ld (UTC) - SKIPPING\n", 
                         epochTime, nextHourTimeUTC);
            searchPos = epochEnd;
            continue;
        }
        
        // Find temperature value after this epoch
        int tempPos = jsonData.indexOf("\"Temperature\":{\"Value\":", epochPos);
        if (tempPos == -1 || tempPos > epochPos + 2000) { // Look within reasonable range
            searchPos = epochEnd;
            continue;
        }
        
        int tempStart = tempPos + 23; // Length of "Temperature":{"Value":
        int tempEnd = jsonData.indexOf(",", tempStart);
        if (tempEnd == -1) tempEnd = jsonData.indexOf("}", tempStart);
        if (tempEnd == -1) {
            searchPos = epochEnd;
            continue;
        }
        
        String tempStr = jsonData.substring(tempStart, tempEnd);
        int temperature = (int)tempStr.toFloat();
        
        // Find humidity
        int humidityPos = jsonData.indexOf("\"RelativeHumidity\":", epochPos);
        int humidity = 50; // Default if not found
        if (humidityPos != -1 && humidityPos < epochPos + 2000) {
            int humStart = humidityPos + 19; // Length of "RelativeHumidity":
            int humEnd = jsonData.indexOf(",", humStart);
            if (humEnd == -1) humEnd = jsonData.indexOf("}", humStart);
            if (humEnd != -1) {
                String humStr = jsonData.substring(humStart, humEnd);
                humidity = humStr.toInt();
            }
        }
        
        // Find weather icon
        int iconPos = jsonData.indexOf("\"WeatherIcon\":", epochPos);
        int icon = 1; // Default sunny if not found
        if (iconPos != -1 && iconPos < tempPos) {
            int iconStart = iconPos + 14; // Length of "WeatherIcon":
            int iconEnd = jsonData.indexOf(",", iconStart);
            if (iconEnd == -1) iconEnd = jsonData.indexOf("}", iconStart);
            if (iconEnd != -1) {
                String iconStr = jsonData.substring(iconStart, iconEnd);
                icon = iconStr.toInt();
            }
        }
        
        // Find icon phrase
        int phrasePos = jsonData.indexOf("\"IconPhrase\":\"", epochPos);
        String phrase = "Weather";
        if (phrasePos != -1 && phrasePos < tempPos) {
            int phraseStart = phrasePos + 14; // Length of "IconPhrase":"
            int phraseEnd = jsonData.indexOf("\"", phraseStart);
            if (phraseEnd != -1) {
                phrase = jsonData.substring(phraseStart, phraseEnd);
            }
        }
        
        LOG_INFOF("[AccuWeather] Simple parse %d: time=%ld, temp=%d, humidity=%d, icon=%d, phrase=%s\n", 
                     forecastIndex, epochTime, temperature, humidity, icon, phrase.c_str());
        
        // Update forecast
        updateForecast(forecastIndex, epochTime, temperature, humidity, phrase.c_str(), icon);
        forecastIndex++;
        
        // Move search position forward
        searchPos = epochEnd;
    }
    
    LOG_INFOF("[AccuWeather] Simple parsing completed, extracted %d forecasts\n", forecastIndex);
    
    if (forecastIndex == 0) {
        LOG_INFO("[AccuWeather] No forecasts found in simple parsing");
        TerminalEvent event(0, "AW", "No forecasts found", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }
    
    // Save all forecasts to EEPROM after successful simple parsing
    LOG_INFO("[AccuWeather] Saving all forecasts to EEPROM after simple parsing...");
    saveToEEPROM();
    
    // Debug: Print all forecasts after saving
    LOG_INFO("[AccuWeather] All forecasts after simple parsing and saving to EEPROM:");
    for (int i = 0; i < 6; i++) {
        LOG_INFOF("  Forecast %d: time=%ld, temp=%d, humidity=%d, icon=%d\n", 
                     i, forecasts[i].time, forecasts[i].temperature, 
                     forecasts[i].humidity, forecasts[i].icon);
    }
    
    return true;
}

}  // namespace modules