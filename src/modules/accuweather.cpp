#include "accuweather.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <time.h>
#include "../config_manager.h"
#include "../event_manager.h"
#include "../timezone_utils.h"
#include "../wifi_manager.h"
#include "module_registry.h"
#include "weather_icons.h"

// Auto-register this module
REGISTER_MODULE("AccuWeather", "accuweather", 5, 64 * 1024, modules::AccuWeather)

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

namespace modules {

void AccuWeather::Configure(const ModuleConfig& config) {
    // Cast to specific config type and store
    const AccuWeatherConfig& weatherConfig = static_cast<const AccuWeatherConfig&>(config);
    moduleConfig = weatherConfig;

    Serial.println("AccuWeather module configured");
    Serial.printf("  API Key: %s\n", moduleConfig.apiKey.isEmpty() ? "NOT SET" : "SET");
    Serial.printf("  City: %s\n", moduleConfig.city.c_str());
    Serial.printf("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    Serial.printf("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    Serial.printf("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");
}

bool AccuWeather::ConfigureFromSection(const ConfigSection& section) {
    Serial.println("AccuWeather configured from INI section");
    
    // Debug: Print all key-value pairs in the section
    Serial.println("Debug: All config section key-value pairs:");
    for (const auto& pair : section.keyValuePairs) {
        Serial.printf("  '%s' = '%s'\n", pair.first.c_str(), pair.second.c_str());
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
    Serial.printf("Debug: api_key value = '%s' (length: %d)\n", moduleConfig.apiKey.c_str(), moduleConfig.apiKey.length());
    Serial.printf("Debug: city value = '%s' (length: %d)\n", moduleConfig.city.c_str(), moduleConfig.city.length());

    // Validation
    if (moduleConfig.apiKey.isEmpty()) {
        Serial.println("AccuWeather: API key is required");
        return false;
    }

    if (moduleConfig.city.isEmpty()) {
        Serial.println("AccuWeather: City is required");
        return false;
    }

    Serial.println("AccuWeather configured from INI section");
    Serial.printf("  API Key: %s\n", moduleConfig.apiKey.isEmpty() ? "NOT SET" : "SET");
    Serial.printf("  City: %s\n", moduleConfig.city.c_str());
    Serial.printf("  Timezone: %s\n", moduleConfig.timezone.c_str());
    Serial.printf("  System Timezone: %s\n", moduleConfig.systemTimezone.c_str());
    Serial.printf("  Position: (%d, %d)\n", moduleConfig.positionX, moduleConfig.positionY);
    Serial.printf("  Size: %dx%d\n", moduleConfig.width, moduleConfig.height);
    Serial.printf("  Enabled: %s\n", moduleConfig.enable ? "YES" : "NO");

    return true;
}

void AccuWeather::Setup() {
    // Weather setup initialization
    Serial.println("AccuWeather module setup");
    loadFromEEPROM();
    Serial.println("Forecasts loaded from EEPROM");
}

void AccuWeather::saveToEEPROM() {
    Serial.println("Saving forecasts to EEPROM...");

    // Write magic number to indicate valid data
    uint32_t magic = 0xABCD1234;
    EEPROM.put(EEPROM_FORECAST_START, magic);

    // Write Unix timestamp when data was saved
    lastDataSaveTime = time(nullptr);
    Serial.printf("Saving lastDataSaveTime to EEPROM: %ld (Unix timestamp)\n", lastDataSaveTime);
    
    // Convert to human readable time for debugging
    struct tm* timeinfo = localtime(&lastDataSaveTime);
    if (timeinfo) {
        char timeString[64];
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.printf("Data saved at: %s\n", timeString);
    }
    
    EEPROM.put(EEPROM_FORECAST_START + sizeof(magic), lastDataSaveTime);

    // Write forecasts data
    int address = EEPROM_FORECAST_START + sizeof(magic) + sizeof(lastDataSaveTime);
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        EEPROM.put(address, forecasts[i]);
        address += sizeof(Forecast);
    }

    EEPROM.commit();
    Serial.println("Forecasts saved to EEPROM successfully");
}

void AccuWeather::loadFromEEPROM() {
    Serial.println("Loading forecasts from EEPROM...");

    // Read magic number to check if data is valid
    uint32_t magic;
    EEPROM.get(EEPROM_FORECAST_START, magic);

    if (magic != 0xABCD1234) {
        Serial.println("No valid forecast data found in EEPROM, initializing empty forecasts");

        // Initialize with empty forecasts
        for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
            forecasts[i] = Forecast();
        }
        lastDataSaveTime = 0;  // Mark as no data
        return;
    }

    // Read Unix timestamp when data was saved
    EEPROM.get(EEPROM_FORECAST_START + sizeof(magic), lastDataSaveTime);
    Serial.printf("Loaded lastDataSaveTime from EEPROM: %ld (Unix timestamp)\n", lastDataSaveTime);
    
    // Convert to human readable time for debugging
    if (lastDataSaveTime > 0) {
        struct tm* timeinfo = localtime(&lastDataSaveTime);
        if (timeinfo) {
            char timeString[64];
            strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);
            Serial.printf("Data was saved at: %s\n", timeString);
        }
    }
    
    // Show current time for comparison
    time_t currentTime = time(nullptr);
    if (currentTime > 0) {
        struct tm* currentTimeinfo = localtime(&currentTime);
        if (currentTimeinfo) {
            char currentTimeString[64];
            strftime(currentTimeString, sizeof(currentTimeString), "%Y-%m-%d %H:%M:%S", currentTimeinfo);
            Serial.printf("Current time: %s\n", currentTimeString);
        }
    }

    // Read forecasts data
    int address = EEPROM_FORECAST_START + sizeof(magic) + sizeof(lastDataSaveTime);
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        EEPROM.get(address, forecasts[i]);
        address += sizeof(Forecast);
    }

    Serial.printf("Successfully loaded %d forecasts from EEPROM\n", 6);
    Serial.printf("Data saved at: %lu, current time: %lu\n", lastDataSaveTime, millis());

    // Check data freshness immediately after loading
    Serial.printf("Is data fresh after loading? %s\n", isDataFresh() ? "YES" : "NO");

    // Print loaded data for verification
    for (int i = 0; i < 6; i++) {  // Updated for 6 forecasts
        if (forecasts[i].time != 0) {
            Serial.printf("Forecast %d: temp=%d, humidity=%d, icon=%d, phrase=%.20s\n", i, forecasts[i].temperature,
                          forecasts[i].humidity, forecasts[i].icon, forecasts[i].phrase);
        }
    }
}

void AccuWeather::Run(void* parameter) {
    Serial.println("Weather Run");

    // Wait for configuration to be ready
    ConfigManager* configManager = ConfigManager::getInstance();
    while (!configManager->IsReady()) {
        Serial.println("Waiting for config to be ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Re-configure from INI section now that config is ready
    ConfigSection moduleSection = configManager->getConfigSection("accuweather");
    // Add system timezone to the section
    moduleSection.keyValuePairs["systemTimezone"] = configManager->getSystemTimezone();
    
    if (!ConfigureFromSection(moduleSection)) {
        Serial.println("Failed to re-configure AccuWeather module after config ready");
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

    TerminalEvent event(0, "AW", "Load data from EEPROM", TerminalEvent::State::SUCCESS);
    EventManager::Emit(event);
    ready = true;

    // Check if EEPROM data is fresh, if not - fetch immediately
    if (!isDataFresh()) {
        Serial.println("EEPROM data is stale, fetching fresh weather data on startup...");
        if (!fetchWeatherData()) {
            Serial.println("Failed to fetch fresh data on startup, using stale EEPROM data");
            TerminalEvent startupEvent(0, "AW", "Using stale cached data", TerminalEvent::State::PROCESSING);
            EventManager::Emit(startupEvent);
        }
    } else {
        Serial.println("EEPROM data is fresh, skipping immediate fetch");
        TerminalEvent freshEvent(0, "AW", "Using fresh cached data", TerminalEvent::State::SUCCESS);
        EventManager::Emit(freshEvent);
    }

    while (true) {
        // Use shorter intervals if data is stale, normal interval if fresh
        unsigned long waitTimeMinutes = isDataFresh() ? 30 : 5; // 30 min if fresh, 5 min if stale
        unsigned long waitTime = waitTimeMinutes * 60 * 1000; // Convert to milliseconds
        
        Serial.printf("Waiting %lu minutes until next update...\n", waitTimeMinutes);
        vTaskDelay(pdMS_TO_TICKS(waitTime));
        
        Serial.println("Scheduled weather data update...");
        if (!fetchWeatherData()) {
            Serial.println("Scheduled update failed, retrying in 30 seconds");
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

    u8g2.setFont(u8g2_font_4x6_tr);
    for (int i = 0; i < 2; i++) {
        const uint8_t* forecastIcon = weatherIcon(forecasts[i].icon);
        yPos = yPos + ((i - 1) * 16) + 16;

        if (i == 0) {
            unsigned long currentTime = millis();
            float scale = 1.0f + 0.1f * sin(currentTime / 1000.0f);

            int iconSize = 19;

            for (int y = 0; y < iconSize; y++) {
                for (int x = 0; x < iconSize; x++) {
                    int srcX = ((x * 18) / iconSize) / scale;
                    int srcY = ((y * 18) / iconSize) / scale;
                    if (srcX < 16 && srcY < 16) {
                        int byteIndex = srcY * 2 + srcX / 8;
                        int bitIndex = srcX % 8;
                        if (forecastIcon[byteIndex] & (1 << bitIndex)) {
                            u8g2.drawPixel(xPos + x, yPos + y);
                        }
                    }
                }
            }

        } else {
            u8g2.drawXBMP(xPos, yPos, 16, 16, forecastIcon);
        }

        u8g2.setFont(u8g2_font_4x6_tr);
        char time_buffer[8];

        // Convert Unix timestamp to HH:MM format with timezone
        if (forecasts[i].time != 0) {
            time_t timestamp = forecasts[i].time;

            // Get timezone from config (use system timezone if AccuWeather timezone is not set)
            String timezone = moduleConfig.timezone;
            if (timezone == "") {
                timezone = moduleConfig.systemTimezone;
            }

            // Apply timezone offset
            int timezoneOffset = TimezoneUtils::getTimezoneOffset(timezone);
            timestamp += timezoneOffset;

            struct tm* timeinfo = gmtime(&timestamp);
            snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
        } else {
            snprintf(time_buffer, sizeof(time_buffer), "--:--");
        }

        u8g2.drawStr(xPos + 21, yPos + 6, time_buffer);
        char forecast_temp_buffer[8];
        snprintf(forecast_temp_buffer, sizeof(forecast_temp_buffer), "%d°C", forecasts[i].temperature);
        u8g2.drawStr(xPos + 21, yPos + 14, forecast_temp_buffer);
        char forecast_humidity_buffer[8];
        snprintf(forecast_humidity_buffer, sizeof(forecast_humidity_buffer), "%d%%", forecasts[i].humidity);
        u8g2.drawStr(xPos + 40, yPos + 14, forecast_humidity_buffer);
    }
}

// Forecast management methods
void AccuWeather::updateForecast(int index, const long time, int temperature, int humidity, const char* phrase,
                                 int icon) {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        Serial.printf("Invalid forecast index: %d\n", index);
        return;
    }

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

    Serial.printf("Updated forecast %d: temp=%d, humidity=%d, icon=%d\n", index, temperature, humidity, icon);

    // Automatically save to EEPROM after update
    saveToEEPROM();
}

void AccuWeather::updateForecast(int index, const Forecast& forecast) {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        Serial.printf("Invalid forecast index: %d\n", index);
        return;
    }

    forecasts[index] = forecast;

    Serial.printf("Updated forecast %d from Forecast object\n", index);

    // Automatically save to EEPROM after update
    saveToEEPROM();
}

AccuWeather::Forecast AccuWeather::getForecast(int index) const {
    if (index < 0 || index >= 6) {  // Updated for 6 forecasts
        Serial.printf("Invalid forecast index: %d\n", index);
        return Forecast();  // Return empty forecast
    }

    return forecasts[index];
}

void AccuWeather::clearForecasts() {
    Serial.println("Clearing all forecasts...");

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
        Serial.println("No data saved, data is not fresh");
        return false;
    }

    // Get current Unix timestamp
    time_t currentTime = time(nullptr);
    
    // Check if system time is synchronized (should be > year 2020)
    if (currentTime < 1577836800) {  // Jan 1, 2020 00:00:00 UTC
        Serial.println("System time not synchronized, treating data as stale");
        return false;
    }

    // Calculate age in seconds
    time_t dataAge = currentTime - lastDataSaveTime;
    const time_t THIRTY_MINUTES = 30 * 60;  // 30 minutes in seconds

    // Convert to human readable for debugging
    struct tm* savedTimeinfo = localtime(&lastDataSaveTime);
    struct tm* currentTimeinfo = localtime(&currentTime);
    
    if (savedTimeinfo && currentTimeinfo) {
        char savedTimeStr[64], currentTimeStr[64];
        strftime(savedTimeStr, sizeof(savedTimeStr), "%H:%M:%S", savedTimeinfo);
        strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M:%S", currentTimeinfo);
        Serial.printf("Data saved at: %s, current time: %s, age: %ld seconds\n", 
                     savedTimeStr, currentTimeStr, dataAge);
    }

    if (dataAge < THIRTY_MINUTES && dataAge >= 0) {
        Serial.printf("Data is fresh: age=%ld seconds (<%ld seconds)\n", dataAge, THIRTY_MINUTES);
        return true;
    } else {
        Serial.printf("Data is stale: age=%ld seconds (>=%ld seconds)\n", dataAge, THIRTY_MINUTES);
        return false;
    }
}

bool AccuWeather::fetchWeatherData() {
    // Check if API key and city are configured
    if (moduleConfig.apiKey.isEmpty() || moduleConfig.city.isEmpty()) {
        Serial.println("AccuWeather API key or city not configured");
        Serial.printf("API key: %s\n", moduleConfig.apiKey.isEmpty() ? "EMPTY" : "SET");
        Serial.printf("City: %s\n", moduleConfig.city.isEmpty() ? "EMPTY" : moduleConfig.city.c_str());
        TerminalEvent event(0, "AW", "API key or city not configured", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    // Create HTTP client
    HTTPClient http;

    // Build URL for 6-hour forecast without details to reduce JSON size
    String url = "http://dataservice.accuweather.com/forecasts/v1/hourly/6hour/" + moduleConfig.city +
                 "?apikey=" + moduleConfig.apiKey + "&language=en-us&details=false&metric=true";

    Serial.println("Fetching weather data from AccuWeather API");
    Serial.printf("City ID: %s\n", moduleConfig.city.c_str());
    Serial.printf("API Key: %s\n", moduleConfig.apiKey.c_str());
    Serial.println("Full URL: " + url);

    // Configure HTTP client
    http.begin(url);
    http.setTimeout(10000);  // 10 seconds timeout

    // Send GET request
    int httpCode = http.GET();

    Serial.printf("HTTP response code: %d\n", httpCode);

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.printf("Successfully fetched weather data, payload size: %d bytes\n", payload.length());

            // Parse JSON response
            if (!parseWeatherData(payload)) {
                http.end();
                return false;
            }

            TerminalEvent event(0, "AW", "Weather data updated", TerminalEvent::State::SUCCESS);
            EventManager::Emit(event);
            http.end();
            return true;
        } else {
            // Get error response for debugging
            String errorResponse = http.getString();
            Serial.printf("HTTP error: %d\n", httpCode);
            Serial.printf("Error response: %s\n", errorResponse.c_str());
            
            // Handle specific AccuWeather API error codes
            if (httpCode == 401) {
                Serial.println("Invalid API key");
                TerminalEvent event(0, "AW", "Invalid API key", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else if (httpCode == 400) {
                Serial.println("Bad request - check city ID");
                TerminalEvent event(0, "AW", "Bad request", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else if (httpCode == 403) {
                Serial.println("API key exceeded quota");
                TerminalEvent event(0, "AW", "API quota exceeded", TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            } else {
                TerminalEvent event(0, "AW", "HTTP error " + String(httpCode), TerminalEvent::State::FAILURE);
                EventManager::Emit(event);
            }
            
            http.end();
            return false;
        }
    } else {
        Serial.printf("HTTP connection failed: %s\n", http.errorToString(httpCode).c_str());
        TerminalEvent event(0, "AW", "Connection failed", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        http.end();
        return false;
    }
}

bool AccuWeather::parseWeatherData(const String& jsonData) {
    // Log JSON data size for debugging
    Serial.printf("JSON data size: %d bytes\n", jsonData.length());
    Serial.printf("Free heap before parsing: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Allocated JSON buffer size: %d bytes\n", 64 * 1024);
    
    // Log first 200 characters of JSON for debugging
    Serial.println("JSON response preview:");
    Serial.println(jsonData.substring(0, 200));
    
    // Check if response looks like an error
    if (jsonData.indexOf("\"fault\"") != -1 || jsonData.indexOf("\"error\"") != -1) {
        Serial.println("AccuWeather API returned error response");
        Serial.println("Full response: " + jsonData);
        TerminalEvent event(0, "AW", "API returned error", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    // Parse reduced response (6 hours, no details) - should be much smaller
    DynamicJsonDocument doc(8 * 1024);  // 8KB should be enough for simplified response

    DeserializationError error = deserializeJson(doc, jsonData);
    if (error) {
        Serial.printf("JSON parsing failed: %s\n", error.c_str());
        Serial.printf("Free heap after failed parsing: %d bytes\n", ESP.getFreeHeap());
        Serial.println("JSON that failed to parse:");
        Serial.println(jsonData);
        TerminalEvent event(0, "AW", "JSON parsing failed", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }

    Serial.printf("JSON parsing successful, free heap: %d bytes\n", ESP.getFreeHeap());

    // Check if response is an array
    if (!doc.is<JsonArray>()) {
        Serial.println("AccuWeather API response is not an array");
        Serial.println("Full response: " + jsonData);
        TerminalEvent event(0, "AW", "Invalid response format", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }
    
    // Clear existing forecasts
    clearForecasts();

    // Process each forecast entry
    JsonArray forecastsArray = doc.as<JsonArray>();
    int index = 0;
    
    Serial.printf("Processing %d forecast entries\n", forecastsArray.size());

    for (JsonObject forecast : forecastsArray) {
        if (index >= 6) break;  // We only store 6 forecasts (reduced from 12)

        // Extract forecast data with validation
        if (!forecast.containsKey("EpochDateTime") || 
            !forecast.containsKey("Temperature") || 
            !forecast.containsKey("RelativeHumidity") ||
            !forecast.containsKey("IconPhrase") ||
            !forecast.containsKey("WeatherIcon")) {
            Serial.printf("Forecast entry %d missing required fields\n", index);
            continue;
        }

        long epochTime = forecast["EpochDateTime"];
        
        if (!forecast["Temperature"].containsKey("Value")) {
            Serial.printf("Forecast entry %d missing Temperature.Value\n", index);
            continue;
        }
        
        int temperature = forecast["Temperature"]["Value"];
        int humidity = forecast["RelativeHumidity"];
        const char* phrase = forecast["IconPhrase"];
        int icon = forecast["WeatherIcon"];

        Serial.printf("Forecast %d: time=%ld, temp=%d, humidity=%d, icon=%d, phrase=%s\n", 
                     index, epochTime, temperature, humidity, icon, phrase);

        // Update forecast
        updateForecast(index, epochTime, temperature, humidity, phrase, icon);
        index++;
    }

    Serial.printf("Successfully parsed %d forecasts\n", index);
    
    if (index == 0) {
        Serial.println("No valid forecasts found in response");
        TerminalEvent event(0, "AW", "No forecasts in response", TerminalEvent::State::FAILURE);
        EventManager::Emit(event);
        return false;
    }
    
    return true;
}

}  // namespace modules