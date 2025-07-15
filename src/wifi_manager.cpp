#include "wifi_manager.h"
#include "logger.h"
#include "memory_manager.h"
#include <WiFi.h>
#include "config_manager.h"
#include "event_manager.h"

extern Config config;

void WiFiManager::Setup() { WiFi.mode(WIFI_AP); }

void WiFiManager::Run() {
    while (!config.isReady()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // WiFiManager will request memory only when connecting (not permanently)

    static int attempt = 0;
    static int attemptReconnect = 0;

    EventManager::Emit(TerminalEvent(0, "WIFI", "Connecting to WiFi", TerminalEvent::State::PROCESSING));
    static bool prevConnectedStatus = false;
    char attempt_str[255];
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (IsConnected()) {
            if (!prevConnectedStatus) {
                snprintf(attempt_str, sizeof(attempt_str), "Connected to %s", config.wifi.ssid.c_str());
                EventManager::Emit(
                    TerminalEvent(attemptReconnect, "WIFI", String(attempt_str), TerminalEvent::State::SUCCESS));
                prevConnectedStatus = true;
            }
            continue;
        }

        if (prevConnectedStatus) {
            attemptReconnect++;
            prevConnectedStatus = false;
        }

        snprintf(attempt_str, sizeof(attempt_str), "%d connecting to %s", attempt, config.wifi.ssid.c_str());
        EventManager::Emit(
            TerminalEvent(attemptReconnect, "WIFI", String(attempt_str), TerminalEvent::State::PROCESSING));

        // WiFiManager bypasses MemoryManager - WiFi connection is critical for system
        WiFi.begin(config.wifi.ssid, config.wifi.password);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for connection attempt

        attempt++;
    }
    
    // Note: Memory is intentionally not released here as WiFi operations are continuous
}

bool WiFiManager::IsConnected() {
    wl_status_t status = WiFi.status();
    bool connected = (status == WL_CONNECTED);

    return connected;
}