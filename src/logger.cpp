#include "logger.h"
#include "memory_manager.h"
#include "config.h"
#include <cstdarg>
#include <cstdio>

// External SPI mutex for SD card operations
extern SemaphoreHandle_t spiMutex;

// External config instance
extern Config config;

// Static member definitions
const size_t Logger::MAX_BUFFER_SIZE;
const uint32_t Logger::FILE_WRITE_INTERVAL_MS;

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::init(bool enableSerial, bool enableFile, const char* logFileName) {
    // Create mutexes if they don't exist
    if (logMutex == nullptr) {
        logMutex = xSemaphoreCreateMutex();
        if (logMutex == nullptr) {
            Serial.println("Logger: Failed to create log mutex!");
            return;
        }
    }
    
    if (bufferMutex == nullptr) {
        bufferMutex = xSemaphoreCreateMutex();
        if (bufferMutex == nullptr) {
            Serial.println("Logger: Failed to create buffer mutex!");
            return;
        }
    }
    
    serialEnabled = enableSerial;
    fileEnabled = enableFile;
    logFilePath = logFileName;
    
    if (serialEnabled) {
        Serial.println("Logger: Serial output enabled");
    }
    
    if (fileEnabled) {
        Serial.println("Logger: Buffered file logging enabled to " + logFilePath);
        Serial.println("Logger: File writer task will be started externally");
    }
}

void Logger::setLogLevel(LogLevel level) {
    if (logMutex != nullptr && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentLogLevel = level;
        xSemaphoreGive(logMutex);
    }
}

void Logger::log(LogLevel level, const char* message) {
    log(level, String(message));
}

void Logger::log(LogLevel level, const String& message) {
    if (level < currentLogLevel) {
        return; // Filter out messages below current log level
    }
    
    writeLog(level, message);
}

void Logger::logf(LogLevel level, const char* format, ...) {
    if (level < currentLogLevel) {
        return; // Filter out messages below current log level
    }
    
    va_list args;
    va_start(args, format);
    
    // Calculate required buffer size
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(level, String(buffer));
    delete[] buffer;
}

void Logger::debug(const char* message) {
    log(LogLevel::DEBUG, message);
}

void Logger::debug(const String& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::debugf(const char* format, ...) {
    if (LogLevel::DEBUG < currentLogLevel) return;
    
    va_list args;
    va_start(args, format);
    
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(LogLevel::DEBUG, String(buffer));
    delete[] buffer;
}

void Logger::info(const char* message) {
    log(LogLevel::INFO, message);
}

void Logger::info(const String& message) {
    log(LogLevel::INFO, message);
}

void Logger::infof(const char* format, ...) {
    if (LogLevel::INFO < currentLogLevel) return;
    
    va_list args;
    va_start(args, format);
    
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(LogLevel::INFO, String(buffer));
    delete[] buffer;
}

void Logger::warning(const char* message) {
    log(LogLevel::WARNING, message);
}

void Logger::warning(const String& message) {
    log(LogLevel::WARNING, message);
}

void Logger::warningf(const char* format, ...) {
    if (LogLevel::WARNING < currentLogLevel) return;
    
    va_list args;
    va_start(args, format);
    
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(LogLevel::WARNING, String(buffer));
    delete[] buffer;
}

void Logger::error(const char* message) {
    log(LogLevel::ERROR, message);
}

void Logger::error(const String& message) {
    log(LogLevel::ERROR, message);
}

void Logger::errorf(const char* format, ...) {
    if (LogLevel::ERROR < currentLogLevel) return;
    
    va_list args;
    va_start(args, format);
    
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(LogLevel::ERROR, String(buffer));
    delete[] buffer;
}

// Print methods for easy Serial replacement
void Logger::print(const char* message) {
    log(LogLevel::INFO, message);
}

void Logger::print(const String& message) {
    log(LogLevel::INFO, message);
}

void Logger::println(const char* message) {
    log(LogLevel::INFO, message);
}

void Logger::println(const String& message) {
    log(LogLevel::INFO, message);
}

void Logger::printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);
    
    if (size <= 0) return;
    
    char* buffer = new char[size];
    if (buffer == nullptr) return;
    
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    writeLog(LogLevel::INFO, String(buffer));
    delete[] buffer;
}

void Logger::writeLog(LogLevel level, const String& message) {
    if (logMutex == nullptr) {
        // Fallback to direct Serial output if mutex not available
        if (serialEnabled) {
            Serial.println(message);
        }
        return;
    }
    
    // Take logger mutex with timeout
    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String formattedMessage = formatLogMessage(level, message);
        
        // Output to Serial immediately
        if (serialEnabled) {
            Serial.print(formattedMessage);
        }
        
        // Add to buffer for file logging (if enabled)
        if (fileEnabled) {
            addToBuffer(level, message);
        }
        
        xSemaphoreGive(logMutex);
    }
}

void Logger::addToBuffer(LogLevel level, const String& message) {
    if (bufferMutex == nullptr) return;
    
    // Check available memory before adding to buffer - more aggressive
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 6144) {  // Need at least 6KB free memory (increased from 4KB)
        return;  // Skip buffering if memory is low
    }
    
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        try {
            // Aggressive buffer management based on memory pressure
            size_t maxSize;
            if (freeHeap < 8192) {
                maxSize = MAX_BUFFER_SIZE / 4;  // Very aggressive when low memory
            } else if (freeHeap < 12288) {
                maxSize = MAX_BUFFER_SIZE / 2;  // Moderate reduction
            } else {
                maxSize = MAX_BUFFER_SIZE;      // Normal size
            }
            
            // If critically low memory, clear buffer entirely
            if (freeHeap < 8192 && !logBuffer.empty()) {
                logBuffer.clear();
                logBuffer.shrink_to_fit();  // Force deallocation
            } else {
                while (logBuffer.size() >= maxSize) {
                    logBuffer.erase(logBuffer.begin());
                }
            }
            
            // Add new entry with current timestamp
            logBuffer.emplace_back(level, message, millis());
            
        } catch (const std::exception& e) {
            // If adding fails, clear some space and try again
            if (!logBuffer.empty()) {
                logBuffer.erase(logBuffer.begin(), logBuffer.begin() + std::min(static_cast<size_t>(10), logBuffer.size()));
                try {
                    logBuffer.emplace_back(level, message, millis());
                } catch (...) {
                    // If still fails, just ignore this log entry
                }
            }
        }
        
        xSemaphoreGive(bufferMutex);
    }
}

void Logger::flushBufferToFile() {
    if (bufferMutex == nullptr || spiMutex == nullptr) return;
    
    // Request memory for file operations through MemoryManager
    if (!MEMORY_REQUEST(MemoryManager::Operation::CONFIG_OPERATION, 
                       MemoryManager::Priority::BACKGROUND, 
                       2048, "Logger-Flush")) {
        // Skip this flush cycle if we can't get memory
        return;
    }
    
    std::vector<LogEntry> bufferCopy;
    
    // Only reserve if we have sufficient memory
    size_t freeHeap = ESP.getFreeHeap();
    size_t reserveSize = std::min(static_cast<size_t>(25), static_cast<size_t>(freeHeap / 500));
    try {
        bufferCopy.reserve(reserveSize);
    } catch (...) {
        // If reserve fails, release memory and return
        MEMORY_RELEASE(MemoryManager::Operation::CONFIG_OPERATION, "Logger-Flush");
        return;
    }
    
    // Move buffer contents under mutex protection (avoid copying)
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!logBuffer.empty()) {
            try {
                // Use move semantics to avoid copying
                bufferCopy = std::move(logBuffer);
                logBuffer.clear();
                
                // Only reserve if we have enough memory
                if (ESP.getFreeHeap() > (MAX_BUFFER_SIZE * 100 + 4096)) {  // Buffer size + safety margin
                    logBuffer.reserve(MAX_BUFFER_SIZE);
                }
            } catch (const std::exception& e) {
                // If move fails, fall back to processing in chunks
                size_t chunkSize = std::min(static_cast<size_t>(10), logBuffer.size());
                bufferCopy.assign(logBuffer.begin(), logBuffer.begin() + chunkSize);
                logBuffer.erase(logBuffer.begin(), logBuffer.begin() + chunkSize);
            }
        }
        xSemaphoreGive(bufferMutex);
    }
    
    // Write to file if we have entries (outside of buffer mutex)
    if (!bufferCopy.empty()) {
        if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            File logFile = SD.open(logFilePath, FILE_APPEND);
            if (logFile) {
                // Process entries in smaller chunks to avoid memory issues
                for (size_t i = 0; i < bufferCopy.size(); i++) {
                    const auto& entry = bufferCopy[i];
                    
                    // Check memory before processing each entry
                    if (ESP.getFreeHeap() < 2048) {
                        break;  // Stop if memory gets too low
                    }
                    
                    String formattedMessage = formatLogMessage(entry.level, entry.message, entry.timestamp);
                    logFile.print(formattedMessage);
                    
                    // Yield to other tasks every 5 entries
                    if (i % 5 == 0) {
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                }
                logFile.close();
            }
            xSemaphoreGive(spiMutex);
        }
    }
    MEMORY_RELEASE(MemoryManager::Operation::CONFIG_OPERATION, "Logger-Flush"); // Release memory after file operation
}

void Logger::fileWriterTask(void* parameter) {
    Logger* logger = static_cast<Logger*>(parameter);
    
    while (true) {
        // Wait for the specified interval
        vTaskDelay(pdMS_TO_TICKS(FILE_WRITE_INTERVAL_MS));
        
        // Flush buffer to file
        if (logger->fileEnabled) {
            logger->flushBufferToFile();
        }
    }
}

String Logger::formatLogMessage(LogLevel level, const String& message, unsigned long timestamp) {
    String timeStr;
    if (timestamp == 0) {
        timeStr = getCurrentTimestamp();
    } else {
        timeStr = String(timestamp);
    }
    String levelStr = getLevelString(level);
    
    // Format: [TIMESTAMP] [LEVEL] MESSAGE\n
    return "[" + timeStr + "] [" + levelStr + "] " + message + "\n";
}

String Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

String Logger::getCurrentTimestamp() {
    time_t now;
    struct tm timeInfo;
    
    if (time(&now) == -1 || !localtime_r(&now, &timeInfo)) {
        // If time is not available, use millis()
        return String(millis());
    }
    
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return String(timeString);
}

void Logger::initFromConfig() {
    // Convert string log level to enum
    LogLevel level = LogLevel::INFO;
    if (config.logger.logLevel == "DEBUG") {
        level = LogLevel::DEBUG;
    } else if (config.logger.logLevel == "INFO") {
        level = LogLevel::INFO;
    } else if (config.logger.logLevel == "WARNING") {
        level = LogLevel::WARNING;
    } else if (config.logger.logLevel == "ERROR") {
        level = LogLevel::ERROR;
    }

    // Generate filename
    String filename = generateLogFilename(config.logger.filePrefix, config.logger.includeDateInFilename);
    
    // Initialize logger
    init(true, config.logger.fileLoggingEnabled, filename.c_str());
    setLogLevel(level);
}

String Logger::generateLogFilename(const String& prefix, bool includeDate) {
    String filename = "/" + prefix;
    
    if (includeDate) {
        time_t now;
        struct tm timeInfo;
        
        if (time(&now) != -1 && localtime_r(&now, &timeInfo)) {
            char dateString[32];
            strftime(dateString, sizeof(dateString), "_%Y%m%d", &timeInfo);
            filename += String(dateString);
        } else {
            // Fallback to millis if time is not available
            filename += "_" + String(millis() / 86400000); // days since boot
        }
    }
    
    filename += ".log";
    return filename;
}

void Logger::runFileWriterTask() {
    uint32_t cycleCount = 0;
    uint32_t emergencyClears = 0;
    
    while (true) {
        // Check memory more frequently than file writes
        for (int i = 0; i < 15; i++) {  // Check every second for 15 seconds
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            size_t freeHeap = ESP.getFreeHeap();
            
            // Use MemoryManager to check critical memory situations
            if (MEMORY_CHECK_CRITICAL() && bufferMutex != nullptr) {
                if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    if (!logBuffer.empty()) {
                        logBuffer.clear();
                        logBuffer.shrink_to_fit();
                        emergencyClears++;
                        Serial.printf("[LOGGER] EMERGENCY: Cleared buffer, free heap: %d bytes\n", ESP.getFreeHeap());
                        
                        // Trigger global cleanup through MemoryManager
                        MEMORY_FORCE_GC();
                    }
                    xSemaphoreGive(bufferMutex);
                }
            }
        }
        
        // Flush buffer to file
        if (fileEnabled) {
            size_t freeHeap = ESP.getFreeHeap();
            size_t bufferSize = 0;
            
            if (bufferMutex != nullptr && xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                bufferSize = logBuffer.size();
                xSemaphoreGive(bufferMutex);
            }
            
            // Log memory status every 10 cycles (every 2.5 minutes)
            if (cycleCount % 10 == 0) {
                Serial.printf("[LOGGER] Memory: %d bytes free, Buffer: %d entries, Emergency clears: %d\n", 
                             freeHeap, bufferSize, emergencyClears);
            }
            
            // Only flush if we have sufficient memory
            if (freeHeap >= 12288) {
                flushBufferToFile();
            } else {
                Serial.printf("[LOGGER] Skipping flush due to low memory: %d bytes\n", freeHeap);
            }
            
            cycleCount++;
        }
    }
} 