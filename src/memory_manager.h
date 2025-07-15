#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

/**
 * Global Memory Manager
 * 
 * Coordinates memory usage across all modules to prevent memory conflicts
 * and fragmentation. Provides centralized memory monitoring and cleanup.
 */
class MemoryManager {
public:
    enum class Priority {
        BACKGROUND = 0,  // Background tasks
        NORMAL = 1,      // Regular module operations
        IMPORTANT = 2,   // Critical operations (display, user input)
        CRITICAL = 3     // System operations (WiFi, config)
    };

    enum class Operation {
        JSON_PARSING,    // JSON document parsing
        HTTP_REQUEST,    // HTTP requests
        DATA_PROCESSING, // Heavy data operations
        DISPLAY_UPDATE,  // Display operations
        CONFIG_OPERATION // Configuration operations
    };

    static MemoryManager* getInstance();
    
    // Easy initialization
    static void initialize();
    static void setDefaultThresholds(size_t lowThreshold = 20000, size_t criticalThreshold = 10000);
    
    // Memory reservation and release
    bool requestMemory(Operation operation, Priority priority, size_t estimatedBytes, const char* moduleName);
    void releaseMemory(Operation operation, const char* moduleName);
    
    // Memory reservation with quiet mode for frequent operations
    bool requestMemoryQuiet(Operation operation, Priority priority, size_t estimatedBytes, const char* moduleName);
    void releaseMemoryQuiet(Operation operation, const char* moduleName);
    
    // Memory status
    size_t getFreeHeap();
    size_t getMinimumFreeHeap();
    bool isMemoryLow();
    bool isMemoryCritical();
    
    // Memory cleanup
    void forceGarbageCollection();
    void performGlobalCleanup();
    void registerCleanupCallback(const char* moduleName, void (*callback)());
    
    // Memory thresholds
    void setLowMemoryThreshold(size_t threshold);
    void setCriticalMemoryThreshold(size_t threshold);
    
    // Statistics and monitoring
    void logMemoryStatus(const char* context = "");
    size_t getAllocatedBytes();
    int getActiveOperations();

private:
    MemoryManager();
    ~MemoryManager();
    
    static MemoryManager* instance;
    SemaphoreHandle_t memoryMutex;
    
    // Memory thresholds
    size_t lowMemoryThreshold = 20000;     // 20KB
    size_t criticalMemoryThreshold = 10000; // 10KB
    
    // Active operations tracking
    struct ActiveOperation {
        Operation operation;
        Priority priority;
        size_t estimatedBytes;
        char moduleName[32];
        unsigned long startTime;
    };
    
    static const int MAX_ACTIVE_OPERATIONS = 16;
    ActiveOperation activeOperations[MAX_ACTIVE_OPERATIONS];
    int activeOperationCount = 0;
    
    // Cleanup callbacks
    struct CleanupCallback {
        char moduleName[32];
        void (*callback)();
    };
    
    static const int MAX_CLEANUP_CALLBACKS = 10;
    CleanupCallback cleanupCallbacks[MAX_CLEANUP_CALLBACKS];
    int cleanupCallbackCount = 0;
    
    // Statistics
    size_t minimumFreeHeap = SIZE_MAX;
    unsigned long lastCleanupTime = 0;
    
    // Internal methods
    bool hasAvailableSlot();
    int findOperationSlot(Operation operation, const char* moduleName);
    void removeOperation(int slot);
    bool canAllocate(size_t bytes, Priority priority);
    void waitForMemory(size_t bytes, Priority priority, unsigned long timeoutMs = 30000);
    void updateMinimumFreeHeap();
};

// Convenience macros for common operations
#define MEMORY_REQUEST(op, pri, bytes, module) MemoryManager::getInstance()->requestMemory(op, pri, bytes, module)
#define MEMORY_RELEASE(op, module) MemoryManager::getInstance()->releaseMemory(op, module)

// Quiet macros for frequent operations (reduced logging)
#define MEMORY_REQUEST_QUIET(op, pri, bytes, module) MemoryManager::getInstance()->requestMemoryQuiet(op, pri, bytes, module)
#define MEMORY_RELEASE_QUIET(op, module) MemoryManager::getInstance()->releaseMemoryQuiet(op, module)

#define MEMORY_CHECK_LOW() MemoryManager::getInstance()->isMemoryLow()
#define MEMORY_CHECK_CRITICAL() MemoryManager::getInstance()->isMemoryCritical()
#define MEMORY_FORCE_GC() MemoryManager::getInstance()->forceGarbageCollection()
#define MEMORY_LOG(context) MemoryManager::getInstance()->logMemoryStatus(context)

#endif // MEMORY_MANAGER_H 