#include "config_manager.h"
#include "logger.h"
#include "memory_manager.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include "event_manager.h"
#include "pins.h"

extern Config config;
extern SemaphoreHandle_t spiMutex;

// Static instance for singleton pattern
ConfigManager* ConfigManager::instance = nullptr;

ConfigManager::ConfigManager() : sdInitialized(false), configFileName("hoowachy_config.ini") {
    // Initialize empty config
}

ConfigManager* ConfigManager::getInstance() {
    if (instance == nullptr) {
        instance = new ConfigManager();
    }
    return instance;
}

bool ConfigManager::initializeSD() {
    if (sdInitialized) {
        return true;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    EventManager::Emit(TerminalEvent(0, "SD", "Initializing SD card", TerminalEvent::State::PROCESSING));

    LOG_INFO("Initializing SD card...");

    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // When working with SD card: DISPLAY_CS_PIN = HIGH, SD_CS_PIN = LOW
        digitalWrite(DISPLAY_CS_PIN, HIGH);
        digitalWrite(SD_CS_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(50));

        bool result = SD.begin(SD_CS_PIN);  // Use default SPI instance
        LOG_INFO("SD.begin() finished");
        LOG_INFOF("SD.begin() result: %s", result ? "SUCCESS" : "FAILED");
        vTaskDelay(pdMS_TO_TICKS(50));

        // Check if SD card is mounted
        if (SD.cardType() == CARD_NONE) {
            LOG_INFO("No SD card attached");
            EventManager::Emit(TerminalEvent(0, "SD", "No SD card attached", TerminalEvent::State::FAILURE));
            config.setReady(false);
            // Release CS pins before returning
            digitalWrite(SD_CS_PIN, HIGH);
            xSemaphoreGive(spiMutex);
            return false;
        }

        LOG_INFO("SD Card Type: ");
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_MMC) {
            LOG_INFO("MMC");
        } else if (cardType == CARD_SD) {
            LOG_INFO("SDSC");
        } else if (cardType == CARD_SDHC) {
            LOG_INFO("SDHC");
        } else {
            LOG_INFO("UNKNOWN");
        }

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        LOG_INFOF("SD Card Size: %lluMB\n", cardSize);

        sdInitialized = true;
        LOG_INFO("SD card initialized successfully");
        EventManager::Emit(TerminalEvent(0, "SD", "SD card initialized", TerminalEvent::State::SUCCESS));
        config.setReady(true);

        // Release CS pins after successful initialization
        digitalWrite(SD_CS_PIN, HIGH);

        xSemaphoreGive(spiMutex);
    }
    return true;
}

bool ConfigManager::isSDReady() const { return sdInitialized; }

bool ConfigManager::IsReady() const { 
    return sdInitialized && config.isReady(); 
}

String ConfigManager::getSystemTimezone() const {
    return config.system.timezone;
}

void ConfigManager::trim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) { return !std::isspace(ch); }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](int ch) { return !std::isspace(ch); }).base(), str.end());
}

String ConfigManager::readFile(const String& filePath) {
    if (!sdInitialized) {
        LOG_INFO("SD card not initialized");
        return "";
    }

    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        LOG_INFOF("Failed to open file: %s\n", filePath.c_str());
        return "";
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();

    return content;
}

bool ConfigManager::parseINIFile(const String& filePath) {
    String content = readFile(filePath);
    if (content.length() == 0) {
        return false;
    }

    String currentSection = "";

    int startPos = 0;
    int endPos = 0;

    while ((endPos = content.indexOf('\n', startPos)) != -1) {
        String line = content.substring(startPos, endPos);
        line.trim();

        // Skip empty lines and comments
        if (line.length() == 0 || line.charAt(0) == ';' || line.charAt(0) == '#') {
            startPos = endPos + 1;
            continue;
        }

        // Parse section headers [section]
        if (line.charAt(0) == '[' && line.charAt(line.length() - 1) == ']') {
            currentSection = line.substring(1, line.length() - 1);
            currentSection.toLowerCase();
            LOG_INFOF("Parsing section: [%s]\n", currentSection.c_str());
            startPos = endPos + 1;
            continue;
        }

        // Parse key=value pairs
        int equalPos = line.indexOf('=');
        if (equalPos > 0) {
            String key = line.substring(0, equalPos);
            String value = line.substring(equalPos + 1);

            key.trim();
            value.trim();

            // Remove quotes from value if present
            if (value.length() >= 2 && value.charAt(0) == '"' && value.charAt(value.length() - 1) == '"') {
                value = value.substring(1, value.length() - 1);
            }

            LOG_INFOF("Config: [%s] %s = %s\n", currentSection.c_str(), key.c_str(), value.c_str());

            // Parse based on section
            if (currentSection == "wifi") {
                parseWiFiSection(key, value);
            } else if (currentSection == "system") {
                parseSystemSection(key, value);
            } else if (currentSection == "display") {
                parseDisplaySection(key, value);
            } else if (currentSection == "buzzer") {
                parseBuzzerSection(key, value);
            } else if (currentSection == "logger") {
                parseLoggerSection(key, value);
            }
        }

        startPos = endPos + 1;
    }

    // Handle last line if no newline at end
    if (startPos < content.length()) {
        String line = content.substring(startPos);
        line.trim();

        if (line.length() > 0 && line.charAt(0) != ';' && line.charAt(0) != '#' && line.charAt(0) != '[') {
            int equalPos = line.indexOf('=');
            if (equalPos > 0) {
                String key = line.substring(0, equalPos);
                String value = line.substring(equalPos + 1);

                key.trim();
                value.trim();

                if (value.length() >= 2 && value.charAt(0) == '"' && value.charAt(value.length() - 1) == '"') {
                    value = value.substring(1, value.length() - 1);
                }

                LOG_INFOF("Config: [%s] %s = %s\n", currentSection.c_str(), key.c_str(), value.c_str());

                // Parse based on section
                if (currentSection == "wifi") {
                    parseWiFiSection(key, value);
                } else if (currentSection == "system") {
                    parseSystemSection(key, value);
                } else if (currentSection == "display") {
                    parseDisplaySection(key, value);
                } else if (currentSection == "buzzer") {
                    parseBuzzerSection(key, value);
                } else if (currentSection == "logger") {
                    parseLoggerSection(key, value);
                }
            }
        }
    }

    return true;
}

void ConfigManager::parseWiFiSection(const String& key, const String& value) {
    if (key == "ssid") {
        config.wifi.ssid = value;
    } else if (key == "password") {
        config.wifi.password = value;
    }
}

void ConfigManager::parseSystemSection(const String& key, const String& value) {
    LOG_INFOF("ConfigManager::parseSystemSection: key='%s', value='%s'\n", key.c_str(), value.c_str());
    if (key == "language") {
        config.system.language = value;
        LOG_INFOF("Set system.language = '%s'\n", config.system.language.c_str());
    } else if (key == "timezone") {
        config.system.timezone = value;
        LOG_INFOF("Set system.timezone = '%s'\n", config.system.timezone.c_str());
    } else if (key == "ntp_server") {
        config.system.ntpServer = value;
        LOG_INFOF("Set system.ntpServer = '%s'\n", config.system.ntpServer.c_str());
    }
}

void ConfigManager::parseDisplaySection(const String& key, const String& value) {
    if (key == "brightness") {
        config.display.brightness = value.toInt();
    }
}

void ConfigManager::parseBuzzerSection(const String& key, const String& value) {
    if (key == "volume") {
        config.buzzer.volume = value.toInt();
    } else if (key == "enabled") {
        config.buzzer.enabled = (value == "true" || value == "1");
    } else if (key == "startup_sound") {
        config.buzzer.startupSound = (value == "true" || value == "1");
    }
}

void ConfigManager::parseLoggerSection(const String& key, const String& value) {
    if (key == "file_logging_enabled") {
        config.logger.fileLoggingEnabled = (value == "true" || value == "1");
    } else if (key == "log_level") {
        config.logger.logLevel = value;
    } else if (key == "file_prefix") {
        config.logger.filePrefix = value;
    } else if (key == "include_date_in_filename") {
        config.logger.includeDateInFilename = (value == "true" || value == "1");
    }
}

// Load config from SD card
bool ConfigManager::loadConfig(const char* fileName) {
    // ConfigManager bypasses MemoryManager - configuration is critical for system operation
    LOG_INFO("ConfigManager: Loading config without MemoryManager restrictions");

    if (!initializeSD()) {
        return false;
    }

    configFileName = fileName;
    String filePath = String("/") + fileName;

    if (!fileExists(filePath)) {
        LOG_INFOF("Config file %s not found, creating default config\n", fileName);
        return true;
    }

    LOG_INFOF("Loading config from: %s\n", filePath.c_str());
    return parseINIFile(filePath);
}

bool ConfigManager::validateConfig() { return config.isValid(); }

modules::ConfigSection ConfigManager::getConfigSection(const String& sectionName, const String& filePath) {
    modules::ConfigSection section;

    // Debug: Print what we're looking for
    LOG_INFOF("Debug: ConfigManager::getConfigSection called with section='%s', file='%s'\n", 
                  sectionName.c_str(), filePath.c_str());

    // Add "/" prefix only if it's not already there
    String fullPath = filePath;
    if (!fullPath.startsWith("/")) {
        fullPath = String("/") + filePath;
    }
    LOG_INFOF("Debug: Full file path: '%s'\n", fullPath.c_str());

    String content = readFile(fullPath);
    if (content.length() == 0) {
        LOG_INFOF("Debug: File content is empty or file not found\n");
        return section;
    }

    LOG_INFOF("Debug: File content length: %d characters\n", content.length());

    String targetSection = sectionName;
    targetSection.toLowerCase();
    String currentSection = "";
    bool foundSection = false;

    LOG_INFOF("Debug: Looking for section '%s' (lowercase)\n", targetSection.c_str());

    // Parse INI content line by line
    int startPos = 0;
    int endPos = 0;

    while ((endPos = content.indexOf('\n', startPos)) != -1) {
        String line = content.substring(startPos, endPos);
        line.trim();

        // Skip empty lines and comments
        if (line.length() == 0 || line.charAt(0) == ';' || line.charAt(0) == '#') {
            startPos = endPos + 1;
            continue;
        }

        // Parse section headers [section]
        if (line.charAt(0) == '[' && line.charAt(line.length() - 1) == ']') {
            currentSection = line.substring(1, line.length() - 1);
            currentSection.toLowerCase();
            foundSection = (currentSection == targetSection);
            LOG_INFOF("Debug: Found section [%s], target match: %s\n", 
                          currentSection.c_str(), foundSection ? "YES" : "NO");
            startPos = endPos + 1;
            continue;
        }

        // Parse key=value pairs if we're in the right section
        if (foundSection) {
            int equalPos = line.indexOf('=');
            if (equalPos > 0) {
                String key = line.substring(0, equalPos);
                String value = line.substring(equalPos + 1);

                key.trim();
                value.trim();

                // Remove quotes from value if present
                if (value.length() >= 2 && value.charAt(0) == '"' && value.charAt(value.length() - 1) == '"') {
                    value = value.substring(1, value.length() - 1);
                }

                LOG_INFOF("Debug: Adding key='%s' value='%s'\n", key.c_str(), value.c_str());
                section.keyValuePairs[key] = value;
            }
        }

        startPos = endPos + 1;
    }

    // Handle last line if no newline at end
    if (startPos < content.length() && foundSection) {
        String line = content.substring(startPos);
        line.trim();

        if (line.length() > 0 && line.charAt(0) != ';' && line.charAt(0) != '#' && line.charAt(0) != '[') {
            int equalPos = line.indexOf('=');
            if (equalPos > 0) {
                String key = line.substring(0, equalPos);
                String value = line.substring(equalPos + 1);

                key.trim();
                value.trim();

                if (value.length() >= 2 && value.charAt(0) == '"' && value.charAt(value.length() - 1) == '"') {
                    value = value.substring(1, value.length() - 1);
                }

                LOG_INFOF("Debug: Adding key='%s' value='%s' (last line)\n", key.c_str(), value.c_str());
                section.keyValuePairs[key] = value;
            }
        }
    }

    LOG_INFOF("Debug: Final section contains %d key-value pairs\n", section.keyValuePairs.size());
    return section;
}

void ConfigManager::printConfig() { config.printConfig(); }

bool ConfigManager::configExists() {
    String filePath = String("/") + configFileName;
    return fileExists(filePath);
}

bool ConfigManager::fileExists(const String& filePath) {
    if (!sdInitialized) {
        return false;
    }

    File file = SD.open(filePath, FILE_READ);
    if (file) {
        file.close();
        return true;
    }

    return false;
}
