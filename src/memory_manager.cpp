#include "memory_manager.h"
#include "logger.h"
#include <cstring>

// Static instance
MemoryManager* MemoryManager::instance = nullptr;

MemoryManager::MemoryManager() {
    memoryMutex = xSemaphoreCreateMutex();
    if (memoryMutex == nullptr) {
        LOG_INFO("MemoryManager: Failed to create mutex");
    }
    activeOperationCount = 0;
    cleanupCallbackCount = 0;
    
    // Initialize all operation slots to empty
    memset(activeOperations, 0, sizeof(activeOperations));
    memset(cleanupCallbacks, 0, sizeof(cleanupCallbacks));
    
    updateMinimumFreeHeap();
    LOG_INFO("MemoryManager initialized");
}

MemoryManager::~MemoryManager() {
    if (memoryMutex != nullptr) {
        vSemaphoreDelete(memoryMutex);
    }
}

MemoryManager* MemoryManager::getInstance() {
    if (instance == nullptr) {
        instance = new MemoryManager();
    }
    return instance;
}

void MemoryManager::initialize() {
    getInstance(); // Just creates the instance
    LOG_INFO("MemoryManager: Global memory coordination initialized");
}

void MemoryManager::setDefaultThresholds(size_t lowThreshold, size_t criticalThreshold) {
    MemoryManager* mgr = getInstance();
    mgr->setLowMemoryThreshold(lowThreshold);
    mgr->setCriticalMemoryThreshold(criticalThreshold);
}

bool MemoryManager::requestMemory(Operation operation, Priority priority, size_t estimatedBytes, const char* moduleName) {
    LOG_INFOF("🔍 DEBUG: %s requesting %zu bytes, heap: %zu\n", moduleName, estimatedBytes, getFreeHeap());
    
    if (memoryMutex == nullptr || moduleName == nullptr) {
        LOG_INFOF("❌ DEBUG: Invalid parameters for %s\n", moduleName);
        return false;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        LOG_INFOF("❌ DEBUG: Failed to acquire mutex for %s\n", moduleName);
        return false;
    }

    LOG_INFOF("✅ DEBUG: %s got mutex, checking slots...\n", moduleName);
    LOG_INFOF("MemoryManager: %s requesting %zu bytes for operation %d, priority %d\n", 
              moduleName, estimatedBytes, (int)operation, (int)priority);

    bool success = false;
    
    // Check if we have a slot available
    LOG_INFOF("🔍 DEBUG: %s checking slots: %d/%d active\n", moduleName, activeOperationCount, MAX_ACTIVE_OPERATIONS);
    if (!hasAvailableSlot()) {
        LOG_INFOF("❌ DEBUG: No available slots for %s (%d/%d used)\n", moduleName, activeOperationCount, MAX_ACTIVE_OPERATIONS);
        xSemaphoreGive(memoryMutex);
        return false;
    }
    LOG_INFOF("✅ DEBUG: %s has available slot, checking memory...\n", moduleName);

    // Check if we can allocate the memory
    if (!canAllocate(estimatedBytes, priority)) {
        size_t freeHeap = getFreeHeap();
        size_t requiredFree = 0;
        switch (priority) {
            case Priority::CRITICAL: requiredFree = criticalMemoryThreshold / 2; break;
            case Priority::IMPORTANT: requiredFree = criticalMemoryThreshold; break;
            case Priority::NORMAL: requiredFree = lowMemoryThreshold; break;
            case Priority::BACKGROUND: requiredFree = lowMemoryThreshold + 10000; break;
        }
        size_t totalNeeded = estimatedBytes + requiredFree;
        
        LOG_INFOF("MemoryManager: Cannot allocate %zu bytes for %s (priority %d)\n", 
                  estimatedBytes, moduleName, (int)priority);
        LOG_INFOF("  Free heap: %zu, Required reserve: %zu, Total needed: %zu\n", 
                  freeHeap, requiredFree, totalNeeded);
        
        // Release mutex temporarily for cleanup
        xSemaphoreGive(memoryMutex);
        
        // Perform cleanup
        performGlobalCleanup();
        
        // Wait for memory to become available
        waitForMemory(estimatedBytes, priority);
        
        // Re-acquire mutex
        if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
            LOG_INFOF("MemoryManager: Failed to re-acquire mutex for %s\n", moduleName);
            return false;
        }
        
        // Check again after cleanup
        if (!canAllocate(estimatedBytes, priority)) {
            LOG_INFOF("MemoryManager: Still cannot allocate memory for %s after cleanup\n", moduleName);
            xSemaphoreGive(memoryMutex);
            return false;
        }
    }

    // Find available slot and register operation
    LOG_INFOF("🔍 DEBUG: Searching for empty slot for %s\n", moduleName);
    for (int i = 0; i < MAX_ACTIVE_OPERATIONS; i++) {
        LOG_INFOF("🔍 DEBUG: Slot %d: moduleName[0]=%d ('%c'), moduleName='%s'\n", 
                  i, (int)activeOperations[i].moduleName[0], 
                  activeOperations[i].moduleName[0] == '\0' ? '0' : activeOperations[i].moduleName[0],
                  activeOperations[i].moduleName);
        if (activeOperations[i].moduleName[0] == '\0') {
            LOG_INFOF("✅ DEBUG: Found empty slot %d for %s\n", i, moduleName);
            activeOperations[i].operation = operation;
            activeOperations[i].priority = priority;
            activeOperations[i].estimatedBytes = estimatedBytes;
            activeOperations[i].startTime = millis();
            strncpy(activeOperations[i].moduleName, moduleName, sizeof(activeOperations[i].moduleName) - 1);
            activeOperations[i].moduleName[sizeof(activeOperations[i].moduleName) - 1] = '\0';
            activeOperationCount++;
            success = true;
            break;
        }
    }
    
    if (!success) {
        LOG_INFOF("❌ DEBUG: No empty slot found for %s despite hasAvailableSlot()=true, activeCount=%d\n", 
                  moduleName, activeOperationCount);
    }

    updateMinimumFreeHeap();
    
    if (success) {
        LOG_INFOF("MemoryManager: ✅ GRANTED %zu bytes for %s (priority %d), active ops: %d/%d, free heap: %zu\n", 
                  estimatedBytes, moduleName, (int)priority, activeOperationCount, MAX_ACTIVE_OPERATIONS, getFreeHeap());
    }

    xSemaphoreGive(memoryMutex);
    return success;
}

void MemoryManager::releaseMemory(Operation operation, const char* moduleName) {
    if (memoryMutex == nullptr || moduleName == nullptr) {
        return;
    }

    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }

    // Find and remove the operation
    int slot = findOperationSlot(operation, moduleName);
    if (slot >= 0) {
        size_t releasedBytes = activeOperations[slot].estimatedBytes;
        removeOperation(slot);
        
        LOG_INFOF("MemoryManager: Released %zu bytes for %s, active ops: %d, free heap: %zu\n", 
                  releasedBytes, moduleName, activeOperationCount, getFreeHeap());
    } else {
        LOG_INFOF("MemoryManager: Operation not found for release: %s\n", moduleName);
    }

    // Force garbage collection after release
    forceGarbageCollection();
    updateMinimumFreeHeap();

    xSemaphoreGive(memoryMutex);
}

bool MemoryManager::requestMemoryQuiet(Operation operation, Priority priority, size_t estimatedBytes, const char* moduleName) {
    if (memoryMutex == nullptr || moduleName == nullptr) {
        return false;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        // Only log failures for quiet operations
        return false;
    }

    bool success = false;
    
    // Check if we have a slot available
    if (!hasAvailableSlot()) {
        xSemaphoreGive(memoryMutex);
        return false;
    }

    // Check if we can allocate the memory
    if (!canAllocate(estimatedBytes, priority)) {
        xSemaphoreGive(memoryMutex);
        return false; // Don't wait or cleanup for frequent operations
    }

    // Find available slot and register operation
    for (int i = 0; i < MAX_ACTIVE_OPERATIONS; i++) {
        if (activeOperations[i].moduleName[0] == '\0') {
            activeOperations[i].operation = operation;
            activeOperations[i].priority = priority;
            activeOperations[i].estimatedBytes = estimatedBytes;
            activeOperations[i].startTime = millis();
            strncpy(activeOperations[i].moduleName, moduleName, sizeof(activeOperations[i].moduleName) - 1);
            activeOperations[i].moduleName[sizeof(activeOperations[i].moduleName) - 1] = '\0';
            activeOperationCount++;
            success = true;
            break;
        }
    }

    updateMinimumFreeHeap();
    
    // No verbose logging for quiet operations - only log if failed
    if (!success) {
        LOG_INFOF("MemoryManager: Failed quiet request for %s\n", moduleName);
    }

    xSemaphoreGive(memoryMutex);
    return success;
}

void MemoryManager::releaseMemoryQuiet(Operation operation, const char* moduleName) {
    if (memoryMutex == nullptr || moduleName == nullptr) {
        return;
    }

    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    // Find and remove the operation
    int slot = findOperationSlot(operation, moduleName);
    if (slot >= 0) {
        removeOperation(slot);
        // No logging for quiet release
    }

    // Force garbage collection after release (but don't log)
    forceGarbageCollection();
    updateMinimumFreeHeap();

    xSemaphoreGive(memoryMutex);
}

size_t MemoryManager::getFreeHeap() {
    return ESP.getFreeHeap();
}

size_t MemoryManager::getMinimumFreeHeap() {
    return minimumFreeHeap;
}

bool MemoryManager::isMemoryLow() {
    return getFreeHeap() < lowMemoryThreshold;
}

bool MemoryManager::isMemoryCritical() {
    return getFreeHeap() < criticalMemoryThreshold;
}

void MemoryManager::forceGarbageCollection() {
    // Trigger ESP32 garbage collection
    ESP.getFreeHeap();
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP.getFreeHeap();
}

void MemoryManager::performGlobalCleanup() {
    LOG_INFO("MemoryManager: Performing global cleanup");
    
    unsigned long startTime = millis();
    size_t heapBefore = getFreeHeap();
    
    // Call all registered cleanup callbacks
    for (int i = 0; i < cleanupCallbackCount; i++) {
        if (cleanupCallbacks[i].callback != nullptr) {
            LOG_INFOF("MemoryManager: Calling cleanup for %s\n", cleanupCallbacks[i].moduleName);
            cleanupCallbacks[i].callback();
            vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between cleanups
        }
    }
    
    // Force garbage collection
    forceGarbageCollection();
    vTaskDelay(pdMS_TO_TICKS(50));
    forceGarbageCollection();
    
    size_t heapAfter = getFreeHeap();
    lastCleanupTime = millis();
    
    LOG_INFOF("MemoryManager: Global cleanup completed in %lu ms, freed %zu bytes (from %zu to %zu)\n", 
              millis() - startTime, heapAfter - heapBefore, heapBefore, heapAfter);
}

void MemoryManager::registerCleanupCallback(const char* moduleName, void (*callback)()) {
    if (cleanupCallbackCount >= MAX_CLEANUP_CALLBACKS || moduleName == nullptr || callback == nullptr) {
        return;
    }

    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strncpy(cleanupCallbacks[cleanupCallbackCount].moduleName, moduleName, 
                sizeof(cleanupCallbacks[cleanupCallbackCount].moduleName) - 1);
        cleanupCallbacks[cleanupCallbackCount].moduleName[sizeof(cleanupCallbacks[cleanupCallbackCount].moduleName) - 1] = '\0';
        cleanupCallbacks[cleanupCallbackCount].callback = callback;
        cleanupCallbackCount++;
        
        LOG_INFOF("MemoryManager: Registered cleanup callback for %s\n", moduleName);
        
        xSemaphoreGive(memoryMutex);
    }
}

void MemoryManager::setLowMemoryThreshold(size_t threshold) {
    lowMemoryThreshold = threshold;
    LOG_INFOF("MemoryManager: Low memory threshold set to %zu bytes\n", threshold);
}

void MemoryManager::setCriticalMemoryThreshold(size_t threshold) {
    criticalMemoryThreshold = threshold;
    LOG_INFOF("MemoryManager: Critical memory threshold set to %zu bytes\n", threshold);
}

void MemoryManager::logMemoryStatus(const char* context) {
    size_t freeHeap = getFreeHeap();
    size_t allocated = getAllocatedBytes();
    
    LOG_INFOF("MemoryManager Status [%s]: Free: %zu, Min: %zu, Allocated: %zu, Active ops: %d\n", 
              context ? context : "General", freeHeap, minimumFreeHeap, allocated, activeOperationCount);
    
    if (activeOperationCount > 0) {
        LOG_INFO("Active operations:");
        if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < MAX_ACTIVE_OPERATIONS; i++) {
                if (activeOperations[i].moduleName[0] != '\0') {
                    unsigned long duration = millis() - activeOperations[i].startTime;
                    LOG_INFOF("  %s: op=%d, pri=%d, bytes=%zu, duration=%lu ms\n", 
                              activeOperations[i].moduleName, 
                              (int)activeOperations[i].operation,
                              (int)activeOperations[i].priority,
                              activeOperations[i].estimatedBytes,
                              duration);
                }
            }
            xSemaphoreGive(memoryMutex);
        }
    }
}

size_t MemoryManager::getAllocatedBytes() {
    size_t total = 0;
    if (xSemaphoreTake(memoryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < MAX_ACTIVE_OPERATIONS; i++) {
            if (activeOperations[i].moduleName[0] != '\0') {
                total += activeOperations[i].estimatedBytes;
            }
        }
        xSemaphoreGive(memoryMutex);
    }
    return total;
}

int MemoryManager::getActiveOperations() {
    return activeOperationCount;
}

// Private methods

bool MemoryManager::hasAvailableSlot() {
    return activeOperationCount < MAX_ACTIVE_OPERATIONS;
}

int MemoryManager::findOperationSlot(Operation operation, const char* moduleName) {
    for (int i = 0; i < MAX_ACTIVE_OPERATIONS; i++) {
        if (activeOperations[i].operation == operation && 
            strcmp(activeOperations[i].moduleName, moduleName) == 0) {
            return i;
        }
    }
    return -1;
}

void MemoryManager::removeOperation(int slot) {
    if (slot >= 0 && slot < MAX_ACTIVE_OPERATIONS) {
        memset(&activeOperations[slot], 0, sizeof(ActiveOperation));
        activeOperationCount--;
    }
}

bool MemoryManager::canAllocate(size_t bytes, Priority priority) {
    size_t freeHeap = getFreeHeap();
    size_t requiredFree = 0;
    
    // Different minimum free memory requirements based on priority
    switch (priority) {
        case Priority::CRITICAL:
            requiredFree = criticalMemoryThreshold / 2; // 2.5KB minimum
            break;
        case Priority::IMPORTANT:
            requiredFree = criticalMemoryThreshold; // 5KB minimum
            break;
        case Priority::NORMAL:
            requiredFree = lowMemoryThreshold; // 10KB minimum
            break;
        case Priority::BACKGROUND:
            requiredFree = lowMemoryThreshold + 10000; // 20KB minimum
            break;
    }
    
    size_t totalNeeded = bytes + requiredFree;
    bool canAlloc = (freeHeap >= totalNeeded);
    
    LOG_INFOF("🔍 canAllocate: free=%zu, need=%zu+%zu=%zu, result=%s\n", 
              freeHeap, bytes, requiredFree, totalNeeded, canAlloc ? "YES" : "NO");
    
    return canAlloc;
}

void MemoryManager::waitForMemory(size_t bytes, Priority priority, unsigned long timeoutMs) {
    unsigned long startTime = millis();
    unsigned long lastLogTime = 0;
    
    LOG_INFOF("MemoryManager: Waiting for %zu bytes to become available\n", bytes);
    
    while (millis() - startTime < timeoutMs) {
        if (canAllocate(bytes, priority)) {
            LOG_INFOF("MemoryManager: Memory became available after %lu ms\n", millis() - startTime);
            return;
        }
        
        // Log status every 5 seconds
        if (millis() - lastLogTime > 5000) {
            logMemoryStatus("Waiting");
            lastLogTime = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait 500ms between checks
    }
    
    LOG_INFOF("MemoryManager: Timeout waiting for memory after %lu ms\n", timeoutMs);
}

void MemoryManager::updateMinimumFreeHeap() {
    size_t current = getFreeHeap();
    if (current < minimumFreeHeap) {
        minimumFreeHeap = current;
    }
} 