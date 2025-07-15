
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>
#include "button.h"
#include "buzzer.h"
#include "config.h"
#include "config_manager.h"
#include "display.h"
#include "logger.h"
#include "memory_manager.h"
#include "modules/module.h"
#include "modules/module_manager.h"
#include "timezone_utils.h"
#include "wifi_manager.h"

// Include modules to trigger auto-registration
#include "modules/accuweather.h"
#include "modules/clock.h"
#include "modules/overlay.h"

// Function to register all modules explicitly
void registerModules() {
    LOG_INFO("Registering modules explicitly...");
    
    // Register Clock module
    modules::ModuleRegistry::RegisterModule("Clock", "clock", 2, 4096, 
                                           []() -> modules::IModule* { return new modules::Clock(); });
    
    // Register AccuWeather module  
    modules::ModuleRegistry::RegisterModule("AccuWeather", "accuweather", 5, 12 * 1024, 
                                           []() -> modules::IModule* { return new modules::AccuWeather(); });
    
    // Register Overlay module
    modules::ModuleRegistry::RegisterModule("Overlay", "overlay", 3, 4096, 
                                           []() -> modules::IModule* { return new modules::Overlay(); });
    
    LOG_INFO("All modules registered explicitly");
}

// External config instance
extern Config config;
SemaphoreHandle_t spiMutex;

#define BUZZER_TASK_PRIORITY 5
#define BUZZER_TASK_STACK_SIZE 4096

#define BUTTON_TASK_PRIORITY 5
#define BUTTON_TASK_STACK_SIZE 4096

#define DISPLAY_TASK_PRIORITY 1
#define DISPLAY_TASK_STACK_SIZE 6144

#define WIFI_TASK_PRIORITY 1
#define WIFI_TASK_STACK_SIZE 4096

#define CONFIG_TASK_PRIORITY 1
#define CONFIG_TASK_STACK_SIZE 6144

#define SYSTEM_TASK_PRIORITY 2
#define SYSTEM_TASK_STACK_SIZE 4096

#define TIME_SYNC_TASK_PRIORITY 3
#define TIME_SYNC_TASK_STACK_SIZE 6144

#define LOGGER_TASK_PRIORITY 2
#define LOGGER_TASK_STACK_SIZE 4096

TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t configTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;
TaskHandle_t timeSyncTaskHandle = NULL;
TaskHandle_t loggerTaskHandle = NULL;

// Task wrapper functions
void buzzerTaskWrapper(void* parameter) { Buzzer::Run(); }

void buttonTaskWrapper(void* parameter) { Button::Run(); }

void displayTaskWrapper(void* parameter) { Display::Run(); }

void wifiTaskWrapper(void* parameter) { WiFiManager::Run(); }

void loggerTaskWrapper(void* parameter) { Logger::getInstance().runFileWriterTask(); }

void configTaskWrapper(void* parameter) {
    LOG_INFO("Initializing configuration...");

    // Get ConfigManager instance
    ConfigManager* configManager = ConfigManager::getInstance();

    // Initialize SD card and load configuration
    if (configManager->loadConfig("hoowachy_config.ini")) {
        LOG_INFO("Configuration loaded successfully");

        // Print current configuration
        configManager->printConfig();

        // Validate configuration
        if (configManager->validateConfig()) {
            LOG_INFO("Configuration is valid");
        } else {
            LOG_WARNING("Configuration validation failed - some settings may be incorrect");
        }
        
        // Reinitialize logger with config settings
        LOG_INFO("Reinitializing logger with config settings...");
        Logger::getInstance().initFromConfig();
        LOG_INFO("Logger reinitialized from configuration");
        
    } else {
        LOG_ERROR("Failed to load configuration");
    }

    // Delete this task as it's no longer needed
    vTaskDelete(NULL);
}

void systemTaskWrapper(void* parameter) {
    Display::SetState(Display::State::TERMINAL);

    while (true) {
        bool allModulesReady = true;
        for (int i = 0; i < active_modules.size(); i++) {
            if (!active_modules[i]->IsReady()) {
                allModulesReady = false;
                break;
            }
        }

        if (allModulesReady && WiFiManager::IsConnected()) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            Display::SetState(Display::State::DASHBOARD);
        } else {
            Display::SetState(Display::State::TERMINAL);
        }

        // Memory monitoring - log status every 2 minutes instead of every 60 seconds
        static unsigned long lastMemoryCheck = 0;
        if (millis() - lastMemoryCheck > 120000) { // Every 2 minutes
            MEMORY_LOG("System Monitor");
            lastMemoryCheck = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void timeSyncTaskWrapper(void* parameter) {
    LOG_INFO("Time sync task started");
    
    // Wait for configuration to be ready
    while (!config.isReady()) {
        LOG_INFO("Waiting for config to be ready for time sync...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    while (true) {
        while (!WiFiManager::IsConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        configTime(0, 0, config.system.ntpServer.c_str());
        LOG_INFO("Time synchronized successfully");

        // Wait a bit for sync to complete
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize global memory coordination early
    MemoryManager::initialize();
    MemoryManager::setDefaultThresholds(10000, 5000); // 10KB low, 5KB critical (much more reasonable)
    MEMORY_LOG("System Startup");
    
    // Initialize logger with default settings (config will be loaded later)
    Logger::getInstance().init(true, false, "/hoowachy_boot.log");
    Logger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    LOG_INFO("Hoowachy system starting up...");
    LOG_INFOF("Initial free heap: %d bytes\n", ESP.getFreeHeap());
    
    if (!EEPROM.begin(EEPROM_SIZE)) {
        LOG_ERROR("Failed to initialize EEPROM");
        return;
    }
    delay(3000);

    spiMutex = xSemaphoreCreateMutex();

    if (spiMutex == NULL) {
        LOG_ERROR("SPI Mutex creation failed!");
        while (true);
    }

    // Configure the default SPI instance with our custom pins
    SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS_PIN);

    Button::Setup();
    Buzzer::Setup();
    Display::Setup();

    WiFiManager::Setup();
    LOG_INFO("Setup done");

    delay(1000);

    LOG_INFO("Creating tasks...");

    BaseType_t result = xTaskCreate(buzzerTaskWrapper, "BuzzerTask", BUZZER_TASK_STACK_SIZE, NULL, BUZZER_TASK_PRIORITY,
                                    &buzzerTaskHandle);
    LOG_INFOF("BuzzerTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(buttonTaskWrapper, "ButtonTask", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY,
                         &buttonTaskHandle);
    LOG_INFOF("ButtonTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(displayTaskWrapper, "DisplayTask", DISPLAY_TASK_STACK_SIZE, NULL, DISPLAY_TASK_PRIORITY,
                         &displayTaskHandle);
    LOG_INFOF("DisplayTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(wifiTaskWrapper, "WifiTask", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, &wifiTaskHandle);
    LOG_INFOF("WifiTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(configTaskWrapper, "ConfigTask", CONFIG_TASK_STACK_SIZE, NULL, CONFIG_TASK_PRIORITY,
                         &configTaskHandle);
    LOG_INFOF("ConfigTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    // Register modules explicitly
    registerModules();

    // Start all registered modules
    modules::ModuleManager::StartAllModules();

    result = xTaskCreate(timeSyncTaskWrapper, "TimeSyncTask", TIME_SYNC_TASK_STACK_SIZE, NULL, TIME_SYNC_TASK_PRIORITY,
                         &timeSyncTaskHandle);
    LOG_INFOF("TimeSyncTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(systemTaskWrapper, "SystemTask", SYSTEM_TASK_STACK_SIZE, NULL, SYSTEM_TASK_PRIORITY,
                         &systemTaskHandle);
    LOG_INFOF("SystemTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(loggerTaskWrapper, "LoggerTask", LOGGER_TASK_STACK_SIZE, NULL, LOGGER_TASK_PRIORITY,
                         &loggerTaskHandle);
    LOG_INFOF("LoggerTask created: %s", result == pdPASS ? "SUCCESS" : "FAILED");
    
    MEMORY_LOG("Setup Complete");
    LOG_INFOF("Setup completed, free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Periodic memory monitoring
    static unsigned long lastMemoryCheck = 0;
    if (millis() - lastMemoryCheck > 300000) { // Every 5 minutes instead of every 60 seconds
        MEMORY_LOG("Main Loop Check");
        lastMemoryCheck = millis();
    }
    
    // Check for critical memory situations
    if (MEMORY_CHECK_CRITICAL()) {
        LOG_WARNING("Critical memory situation detected in main loop");
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Small delay to prevent busy loop
}
