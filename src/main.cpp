
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
#include "modules/module.h"
#include "modules/module_manager.h"
#include "timezone_utils.h"
#include "wifi_manager.h"

// Include modules to trigger auto-registration
#include "modules/accuweather.h"
#include "modules/clock.h"

// External config instance
extern Config config;
SemaphoreHandle_t spiMutex;

#define BUZZER_TASK_PRIORITY 5
#define BUZZER_TASK_STACK_SIZE 4096

#define BUTTON_TASK_PRIORITY 5
#define BUTTON_TASK_STACK_SIZE 4096

#define DISPLAY_TASK_PRIORITY 1
#define DISPLAY_TASK_STACK_SIZE 12288

#define WIFI_TASK_PRIORITY 1
#define WIFI_TASK_STACK_SIZE 8192

#define CONFIG_TASK_PRIORITY 1
#define CONFIG_TASK_STACK_SIZE 12288

#define SYSTEM_TASK_PRIORITY 2
#define SYSTEM_TASK_STACK_SIZE 2048

#define TIME_SYNC_TASK_PRIORITY 3
#define TIME_SYNC_TASK_STACK_SIZE 4096

TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t configTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;
TaskHandle_t timeSyncTaskHandle = NULL;

// Task wrapper functions
void buzzerTaskWrapper(void* parameter) { Buzzer::Run(); }

void buttonTaskWrapper(void* parameter) { Button::Run(); }

void displayTaskWrapper(void* parameter) { Display::Run(); }

void wifiTaskWrapper(void* parameter) { WiFiManager::Run(); }

void configTaskWrapper(void* parameter) {
    Serial.println("Initializing configuration...");

    // Get ConfigManager instance
    ConfigManager* configManager = ConfigManager::getInstance();

    // Initialize SD card and load configuration
    if (configManager->loadConfig("hoowachy_config.ini")) {
        Serial.println("Configuration loaded successfully");

        // Print current configuration
        configManager->printConfig();

        // Validate configuration
        if (configManager->validateConfig()) {
            Serial.println("Configuration is valid");
        } else {
            Serial.println("Configuration validation failed - some settings may be incorrect");
        }
    } else {
        Serial.println("Failed to load configuration");
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

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void timeSyncTaskWrapper(void* parameter) {
    Serial.println("Time sync task started");
    while (true) {
        while (!WiFiManager::IsConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        configTime(0, 0, config.system.ntpServer.c_str());
        Serial.println("Time synchronized successfully");

        // Wait a bit for sync to complete
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
}

void setup() {
    Serial.begin(115200);
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("Failed to initialize EEPROM");
        return;
    }
    delay(3000);

    spiMutex = xSemaphoreCreateMutex();

    if (spiMutex == NULL) {
        Serial.println("SPI Mutex creation failed!");
        while (true);
    }

    // Configure the default SPI instance with our custom pins
    SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS_PIN);

    Button::Setup();
    Buzzer::Setup();
    Display::Setup();

    WiFiManager::Setup();
    Serial.println("Setup done");

    delay(1000);

    Serial.println("Creating tasks...");

    BaseType_t result = xTaskCreate(buzzerTaskWrapper, "BuzzerTask", BUZZER_TASK_STACK_SIZE, NULL, BUZZER_TASK_PRIORITY,
                                    &buzzerTaskHandle);
    Serial.printf("BuzzerTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(buttonTaskWrapper, "ButtonTask", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY,
                         &buttonTaskHandle);
    Serial.printf("ButtonTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(displayTaskWrapper, "DisplayTask", DISPLAY_TASK_STACK_SIZE, NULL, DISPLAY_TASK_PRIORITY,
                         &displayTaskHandle);
    Serial.printf("DisplayTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(wifiTaskWrapper, "WifiTask", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, &wifiTaskHandle);
    Serial.printf("WifiTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(configTaskWrapper, "ConfigTask", CONFIG_TASK_STACK_SIZE, NULL, CONFIG_TASK_PRIORITY,
                         &configTaskHandle);
    Serial.printf("ConfigTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    // Start all registered modules
    modules::ModuleManager::StartAllModules();

    result = xTaskCreate(timeSyncTaskWrapper, "TimeSyncTask", TIME_SYNC_TASK_STACK_SIZE, NULL, TIME_SYNC_TASK_PRIORITY,
                         &timeSyncTaskHandle);
    Serial.printf("TimeSyncTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");

    result = xTaskCreate(systemTaskWrapper, "SystemTask", SYSTEM_TASK_STACK_SIZE, NULL, SYSTEM_TASK_PRIORITY,
                         &systemTaskHandle);
    Serial.printf("SystemTask created: %s\n", result == pdPASS ? "SUCCESS" : "FAILED");
}

void loop() {}
