# Logger System Documentation

## Overview

The Hoowachy project now includes a comprehensive logging system that replaces all `Serial.print/println/printf` calls with a more sophisticated logger. The logger provides multiple log levels, thread-safe operation, and support for both serial and file output.

## Features

- **Multiple Log Levels**: DEBUG, INFO, WARNING, ERROR
- **Thread-Safe**: Uses FreeRTOS mutexes for safe concurrent access
- **Dual Output**: Supports both Serial and SD card file logging
- **Formatted Output**: Includes timestamps and log levels
- **Memory Efficient**: Dynamic memory allocation for variable-length messages
- **Easy Migration**: Simple macros to replace existing Serial calls

## Basic Usage

### Include the Logger

```cpp
#include "logger.h"
```

### Initialize the Logger

#### Preferred Method: Configuration-Based Initialization

The logger is automatically configured from the `hoowachy_config.ini` file:

```ini
[logger]
# Enable file logging to SD card (true/false)
file_logging_enabled = true

# Log level: DEBUG, INFO, WARNING, ERROR
log_level = "INFO"

# Prefix for log file names
file_prefix = "hoowachy"

# Include date in filename (true/false)
# If true: hoowachy_20241215.log
# If false: hoowachy.log
include_date_in_filename = true
```

The logger is automatically initialized with these settings after config loading.

#### Manual Initialization (Alternative)

For testing or special cases:

```cpp
// Initialize with Serial output only
Logger::getInstance().init(true, false);

// Or with both Serial and file output
Logger::getInstance().init(true, true, "/system.log");

// Set log level (optional, defaults to DEBUG)
Logger::getInstance().setLogLevel(LogLevel::INFO);
```

### Using the Logger

#### Direct Logger Methods

```cpp
// Basic logging
Logger::getInstance().debug("Debug message");
Logger::getInstance().info("Info message");
Logger::getInstance().warning("Warning message");
Logger::getInstance().error("Error message");

// Formatted logging
Logger::getInstance().debugf("Value: %d", 42);
Logger::getInstance().infof("Temperature: %.2f°C", 25.67);
Logger::getInstance().warningf("Low battery: %d%%", batteryLevel);
Logger::getInstance().errorf("Failed to connect to %s", hostname);
```

#### Using Convenient Macros

```cpp
// Simple logging
LOG_DEBUG("Debug message");
LOG_INFO("System initialized");
LOG_WARNING("Connection unstable");
LOG_ERROR("Critical failure");

// Formatted logging
LOG_DEBUGF("Processing item %d of %d", current, total);
LOG_INFOF("WiFi connected to %s", ssid);
LOG_WARNINGF("Memory usage: %d%%", usage);
LOG_ERRORF("Failed to read sensor: error %d", errorCode);
```

#### Direct Instance Access

```cpp
// Access logger instance directly
LOG.info("Message using global LOG macro");
LOG.debugf("Formatted message: %s", text);
```

## Log Levels

| Level   | Usage                          | When to Use                  |
| ------- | ------------------------------ | ---------------------------- |
| DEBUG   | Detailed diagnostic info       | Development, troubleshooting |
| INFO    | General informational messages | Normal operation status      |
| WARNING | Potentially harmful situations | Non-critical issues          |
| ERROR   | Error events                   | Failures that need attention |

## Configuration

### Setting Log Level

```cpp
// Only show INFO, WARNING, and ERROR messages
Logger::getInstance().setLogLevel(LogLevel::INFO);

// Show all messages (default)
Logger::getInstance().setLogLevel(LogLevel::DEBUG);

// Only show ERROR messages
Logger::getInstance().setLogLevel(LogLevel::ERROR);
```

### Output Configuration

```cpp
// Serial output only (default)
Logger::getInstance().init(true, false);

// File output only
Logger::getInstance().init(false, true, "/app.log");

// Both Serial and file output
Logger::getInstance().init(true, true, "/system.log");

// Disable all output
Logger::getInstance().init(false, false);
```

## Migration from Serial.print

The logger system automatically replaced existing `Serial.print/println/printf` calls with appropriate logger calls. **The API is fully backward compatible** - no changes needed to existing LOG_XXX() calls.

### Automatic Migration Mapping:

| Old Code                         | New Code                     |
| -------------------------------- | ---------------------------- |
| `Serial.println("message");`     | `LOG_INFO("message");`       |
| `Serial.print("message");`       | `LOG_INFO("message");`       |
| `Serial.printf("format", args);` | `LOG_INFOF("format", args);` |

### What Changed (Internal):

- **Serial output**: Still immediate, no change in behavior
- **File logging**: Now buffered and asynchronous (15-second intervals)
- **Performance**: Significantly improved, no blocking on file operations
- **API**: **Completely unchanged** - all existing LOG_XXX() calls work identically

### What Stayed the Same:

- All LOG_XXX() macros work exactly as before
- Log levels and filtering behavior unchanged
- Serial output timing and format identical
- Thread safety guarantees maintained

## Log Output Format

```
[TIMESTAMP] [LEVEL] MESSAGE
```

Example output:

```
[2024-01-15 10:30:45] [INFO] Hoowachy system starting up...
[2024-01-15 10:30:45] [DEBUG] Button Run
[2024-01-15 10:30:46] [INFO] WiFi connected to MyNetwork
[2024-01-15 10:30:47] [WARNING] Low signal strength: -75 dBm
[2024-01-15 10:30:50] [ERROR] Failed to connect to API server
```

## Thread Safety

The logger is fully thread-safe and can be used from multiple FreeRTOS tasks without any additional synchronization. The logger uses three levels of protection:

- **Logger mutex**: Protects internal logger state and ensures atomic log operations
- **Buffer mutex**: Protects the log buffer from concurrent access
- **SPI mutex**: Shared with other SPI devices (display, etc.) to prevent bus conflicts during SD card access

### Architecture

```
┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐
│   Application   │    │    Logger    │    │  File Writer    │
│     Threads     │    │    Buffer    │    │      Task       │
│                 │    │              │    │                 │
│ LOG_INFO(...)   │───▶│ In-Memory    │───▶│ SD Card Write   │
│ LOG_DEBUG(...)  │    │ Buffer       │    │ (every 15s)     │
│ LOG_ERROR(...)  │    │ (50 entries) │    │                 │
└─────────────────┘    └──────────────┘    └─────────────────┘
       ▲                       │                     │
       │                       ▼                     ▼
┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐
│ Serial Output   │◀───│ Immediate    │    │ Batch File I/O  │
│ (immediate)     │    │ Processing   │    │ (buffered)      │
└─────────────────┘    └──────────────┘    └─────────────────┘
```

```cpp
// Task A
void taskA(void* parameter) {
    while (true) {
        LOG_INFO("Task A running");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task B
void taskB(void* parameter) {
    while (true) {
        LOG_DEBUG("Task B processing");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## Best Practices

### 1. Choose Appropriate Log Levels

```cpp
// Good examples
LOG_DEBUG("Entering function calculateTotal()");           // Development info
LOG_INFO("System initialized successfully");               // Status info
LOG_WARNING("Retrying connection attempt 3/5");           // Recoverable issue
LOG_ERROR("Critical: SD card not found");                 // Serious problem

// Avoid
LOG_ERROR("Debug: variable x = 5");                       // Wrong level
LOG_DEBUG("System crashed");                              // Wrong level
```

### 2. Use Formatted Logging

```cpp
// Good
LOG_INFOF("Temperature: %.1f°C, Humidity: %d%%", temp, humidity);

// Avoid multiple calls
LOG_INFO("Temperature: ");
LOG_INFO(String(temp));
LOG_INFO("°C");
```

### 3. Include Context

```cpp
// Good
LOG_ERRORF("WiFi: Failed to connect to %s (attempt %d)", ssid, attempt);

// Less helpful
LOG_ERROR("Connection failed");
```

### 4. Performance Considerations

```cpp
// For high-frequency debugging, use conditional logging
if (Logger::getInstance().currentLogLevel <= LogLevel::DEBUG) {
    String debugInfo = generateExpensiveDebugInfo();
    LOG_DEBUGF("Debug info: %s", debugInfo.c_str());
}
```

## File Logging

The logger uses **buffered file logging** for optimal performance. When file logging is enabled:

### Buffering System

- **In-memory buffer**: Logs are first stored in a circular buffer (max 50 entries, adaptive based on available memory)
- **Background task**: Separate FreeRTOS task writes buffer to SD card every 15 seconds
- **Non-blocking**: Main threads never wait for file I/O operations
- **Overflow protection**: When buffer is full, oldest entries are automatically discarded

### File Operations

- Create the log file if it doesn't exist
- Append new entries to existing files in batches
- Handle SD card errors gracefully (fallback to Serial only)
- **Use shared SPI mutex** to synchronize access with other SPI devices (display, etc.)
- Longer SPI timeout (2 seconds) for batch operations
- **Smart filename generation** with optional date inclusion

### Filename Generation

The logger automatically generates filenames based on configuration:

```cpp
// Configuration examples:
file_prefix = "hoowachy"
include_date_in_filename = true
// Result: /hoowachy_20241215.log

file_prefix = "debug"
include_date_in_filename = false
// Result: /debug.log

file_prefix = "sensor_data"
include_date_in_filename = true
// Result: /sensor_data_20241215.log
```

**Benefits of date-based filenames:**

- Automatic log rotation by date
- Easy to find logs for specific days
- Prevents single huge log files
- Organized log history

```cpp
// Enable buffered file logging
Logger::getInstance().init(true, true, "/system.log");

// Usage remains exactly the same
LOG_INFO("System started");
LOG_DEBUGF("Free memory: %d bytes", ESP.getFreeHeap());
LOG_WARNING("Low battery");

// File operations happen automatically in background
// Check log file (logs are written every 15 seconds)
File logFile = SD.open("/system.log");
if (logFile) {
    while (logFile.available()) {
        Serial.write(logFile.read());
    }
    logFile.close();
}
```

## Troubleshooting

### Logger Not Working

1. Ensure `logger.h` is included
2. Check if logger is initialized in `setup()`
3. Verify log level allows your messages
4. Check if SD card is properly mounted (for file logging)

### Memory Issues

The logger uses dynamic memory allocation. If you experience memory issues:

1. Reduce log level to ERROR only
2. Disable file logging
3. Use shorter log messages
4. Check for memory leaks in your application

### Performance Impact

The buffered logging system is highly optimized:

- **Immediate Serial output**: No delay for console logging
- **Non-blocking file logging**: Applications never wait for SD card operations
- **Batch writes**: More efficient SD card usage (writes every 15 seconds)
- **Minimal SPI contention**: Infrequent, scheduled file operations
- **Memory usage**: ~4KB for buffer (50 entries × ~80 bytes average)

Performance characteristics:

- **LOG_XXX() calls**: ~50-100μs (mutex + buffer operation)
- **File writes**: Background task, zero impact on main threads
- **DEBUG level**: Minimal overhead in production (filtered before buffering)

For production, DEBUG level can be safely enabled.

## Configuration Examples

### Complete Logger Configuration

Add this section to your `hoowachy_config.ini` file:

```ini
[logger]
# Enable file logging to SD card
file_logging_enabled = true

# Log level: DEBUG, INFO, WARNING, ERROR
log_level = "INFO"

# Prefix for log file names
file_prefix = "hoowachy"

# Include date in filename for automatic rotation
include_date_in_filename = true
```

### Different Use Cases

#### Development Configuration

```ini
[logger]
file_logging_enabled = true
log_level = "DEBUG"
file_prefix = "debug"
include_date_in_filename = true
# Results in: /debug_20241215.log with all messages
```

#### Production Configuration

```ini
[logger]
file_logging_enabled = true
log_level = "WARNING"
file_prefix = "hoowachy"
include_date_in_filename = true
# Results in: /hoowachy_20241215.log with warnings and errors only
```

#### Testing Configuration

```ini
[logger]
file_logging_enabled = false
log_level = "DEBUG"
file_prefix = "test"
include_date_in_filename = false
# Results in: Serial output only, all messages
```

### Buffering Behavior

The logger handles various scenarios intelligently:

#### Normal Operation

- **Logs accumulate** in memory buffer (up to 100 entries)
- **Background task flushes** buffer to SD card every 15 seconds
- **SPI mutex taken** for 2 seconds max during batch write

#### Buffer Overflow

- **Circular buffer**: Oldest entries automatically discarded when full
- **No blocking**: Applications continue logging without interruption
- **Warning indication**: Buffer overflow doesn't affect system stability

#### SPI Contention

- **Extended timeout**: 2-second SPI mutex timeout for batch operations
- **Graceful degradation**: If SPI unavailable, retry on next cycle (15s later)
- **Serial continues**: Console output unaffected by file logging issues

#### SD Card Errors

- **Error resilience**: File operation failures don't crash the system
- **Automatic retry**: Next flush cycle will attempt file operations again
- **Fallback logging**: Serial output remains functional

#### Memory Management

The logger includes advanced memory management to prevent crashes:

- **Memory monitoring**: Continuously monitors available heap memory
- **Adaptive buffer size**: Reduces buffer size when memory is low (50 → 25 entries)
- **Safe memory thresholds** (increased for stability):
  - 12KB free: Required for buffer flush operations
  - 8KB free: Emergency buffer clear, critical threshold
  - 6KB free: Required for adding new log entries
  - 2KB free: Emergency threshold, stops all processing
- **Exception handling**: Graceful handling of memory allocation failures
- **Move semantics**: Uses `std::move()` to avoid costly buffer copying
- **Chunked processing**: Processes log entries in small chunks to avoid memory spikes
- **Memory status logging**: Reports memory usage every 2.5 minutes to Serial
- **Emergency monitoring**: Checks memory every second, not just during flush cycles
- **Automatic buffer clearing**: Forces buffer clear when memory drops below 8KB

**Memory-related behavior:**

- **Emergency mode**: When memory < 8KB, buffer is immediately cleared and shrunk
- **Adaptive thresholds**: File logging suspended below 12KB, buffering stopped below 6KB
- **Frequent monitoring**: Memory checked every second, not just every 15 seconds
- **Serial output continues working** regardless of memory status
- **System remains stable** even under extreme memory pressure
- **Emergency counter**: Tracks how often emergency clearing was needed
