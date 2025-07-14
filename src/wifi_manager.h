#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Forward declarations
class WiFiClass;
class ConfigManager;

class WiFiManager {
  private:
  public:
    static void Setup();
    static void Run();
    static bool IsConnected();
};

#endif  // WIFI_MANAGER_H