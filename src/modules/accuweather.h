#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include "logger.h"
#include <stdint.h>
#include "event_manager.h"
#include "module.h"

namespace modules {

// AccuWeather specific configuration
struct AccuWeatherConfig : public ModuleConfig {
    String apiKey = "";
    String city = "";
    String timezone = "";
    String systemTimezone = "UTC";  // Fallback timezone from system
};

class AccuWeather : public IModule {
  public:
    void Setup() override;
    void Run(void* parameter) override;
    void Draw() override;
    bool IsReady() override;
    void Configure(const ModuleConfig& config) override;
    bool ConfigureFromSection(const ConfigSection& section) override;

    class Forecast {
      public:
        long time;
        int temperature;
        int humidity;
        char phrase[64];  // Changed from pointer to fixed-size array for EEPROM storage
        int icon;

        // Default constructor
        Forecast() {
            time = 0;
            temperature = 0;
            humidity = 0;
            memset(phrase, 0, sizeof(phrase));
            icon = 0;
        }
    };

    // EEPROM methods
    void saveToEEPROM();
    void loadFromEEPROM();

    // Forecast management methods
    void updateForecast(int index, const long time, int temperature, int humidity, const char* phrase, int icon);
    void updateForecast(int index, const Forecast& forecast);
    Forecast getForecast(int index) const;
    void clearForecasts();
    bool hasForecastData() const;
    int getValidForecastCount() const;

    // API methods
    bool fetchWeatherData();

    // Data freshness check
    bool isDataFresh() const;

  private:
    const uint8_t* weatherIcon(int p);
    bool parseWeatherData(const String& jsonData);
    bool parseWeatherDataSimple(const String& jsonData);

    // EEPROM addresses
    static const int EEPROM_FORECAST_START = 0;
    static const int EEPROM_FORECAST_SIZE = sizeof(Forecast);
    static const int EEPROM_TOTAL_SIZE = EEPROM_FORECAST_SIZE * 6;  // Updated for 6 forecasts

    // Module configuration (injected)
    AccuWeatherConfig moduleConfig;

    // Module state
    Forecast forecasts[6];  // Reduced from 12 to 6 for memory optimization
    bool ready = false;
    time_t lastDataSaveTime = 0;  // Unix timestamp instead of millis()
};

}  // namespace modules

#endif