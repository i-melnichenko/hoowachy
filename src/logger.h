#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string>
#include <ctime>
#include <vector>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

struct LogEntry {
    LogLevel level;
    String message;
    unsigned long timestamp;
    
    LogEntry() : level(LogLevel::INFO), timestamp(0) {}
    LogEntry(LogLevel l, const String& msg, unsigned long ts) 
        : level(l), message(msg), timestamp(ts) {}
};

class Logger {
public:
    static Logger& getInstance();
    
    // Initialize logger with Serial and optionally SD card
    void init(bool enableSerial = true, bool enableFile = false, const char* logFileName = "/log.txt");
    
    // Initialize from config (preferred method)
    void initFromConfig();
    
    // Set minimum log level (messages below this level will be filtered out)
    void setLogLevel(LogLevel level);
    
    // Generate log filename with date if needed
    String generateLogFilename(const String& prefix, bool includeDate);
    
    // Basic logging methods
    void log(LogLevel level, const char* message);
    void log(LogLevel level, const String& message);
    void logf(LogLevel level, const char* format, ...);
    
    // Convenience methods for different log levels
    void debug(const char* message);
    void debug(const String& message);
    void debugf(const char* format, ...);
    
    void info(const char* message);
    void info(const String& message);
    void infof(const char* format, ...);
    
    void warning(const char* message);
    void warning(const String& message);
    void warningf(const char* format, ...);
    
    void error(const char* message);
    void error(const String& message);
    void errorf(const char* format, ...);
    
    // Print methods for easy Serial replacement
    void print(const char* message);
    void print(const String& message);
    void println(const char* message);
    void println(const String& message);
    void printf(const char* format, ...);
    
    // Public method for external task creation
    void runFileWriterTask();

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void writeLog(LogLevel level, const String& message);
    void addToBuffer(LogLevel level, const String& message);
    void flushBufferToFile();
    String formatLogMessage(LogLevel level, const String& message, unsigned long timestamp = 0);
    String getLevelString(LogLevel level);
    String getCurrentTimestamp();
    static void fileWriterTask(void* parameter);
    
    SemaphoreHandle_t logMutex = nullptr;
    SemaphoreHandle_t bufferMutex = nullptr;
    LogLevel currentLogLevel = LogLevel::DEBUG;
    bool serialEnabled = true;
    bool fileEnabled = false;
    String logFilePath = "/log.txt";
    
    // Buffered file logging
    std::vector<LogEntry> logBuffer;
    TaskHandle_t fileWriterTaskHandle = nullptr;
    static const size_t MAX_BUFFER_SIZE = 50;  // Reduced from 100 to 50 to save memory
    static const uint32_t FILE_WRITE_INTERVAL_MS = 15000; // 15 seconds
};

// Global logger instance accessor
#define LOG Logger::getInstance()

// Convenient macros for logging
#define LOG_DEBUG(msg) LOG.debug(msg)
#define LOG_DEBUGF(fmt, ...) LOG.debugf(fmt, ##__VA_ARGS__)

#define LOG_INFO(msg) LOG.info(msg)
#define LOG_INFOF(fmt, ...) LOG.infof(fmt, ##__VA_ARGS__)

#define LOG_WARNING(msg) LOG.warning(msg)
#define LOG_WARNINGF(fmt, ...) LOG.warningf(fmt, ##__VA_ARGS__)

#define LOG_ERROR(msg) LOG.error(msg)
#define LOG_ERRORF(fmt, ...) LOG.errorf(fmt, ##__VA_ARGS__)

// Macros to replace Serial.print/println easily
#define SERIAL_PRINT(msg) LOG.print(msg)
#define SERIAL_PRINTLN(msg) LOG.println(msg)
#define SERIAL_PRINTF(fmt, ...) LOG.printf(fmt, ##__VA_ARGS__)

#endif // LOGGER_H 