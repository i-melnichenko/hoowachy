#include "module_manager.h"
#include "../config.h"

namespace modules {

std::vector<TaskHandle_t> ModuleManager::taskHandles;

void ModuleManager::StartAllModules() {
    Serial.println("Starting all registered modules...");

    // Print registered modules
    ModuleRegistry::PrintRegisteredModules();

    const auto& modules = ModuleRegistry::GetModules();

    for (const auto& moduleInfo : modules) {
        Serial.printf("Starting module: %s\n", moduleInfo.name.c_str());

        TaskHandle_t taskHandle = NULL;

        // Create a copy of module info for the task
        ModuleInfo* moduleInfoCopy = new ModuleInfo(moduleInfo);

        BaseType_t result = xTaskCreate(ModuleTaskWrapper, moduleInfo.name.c_str(), moduleInfo.stackSize,
                                        moduleInfoCopy, moduleInfo.taskPriority, &taskHandle);

        if (result == pdPASS) {
            taskHandles.push_back(taskHandle);
            Serial.printf("Module %s started successfully\n", moduleInfo.name.c_str());
        } else {
            Serial.printf("Failed to start module %s\n", moduleInfo.name.c_str());
            delete moduleInfoCopy;
        }
    }
}

void ModuleManager::ModuleTaskWrapper(void* parameter) {
    ModuleInfo* moduleInfo = static_cast<ModuleInfo*>(parameter);

    if (!moduleInfo) {
        Serial.println("Invalid module info in task wrapper");
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("Module task wrapper started for: %s\n", moduleInfo->name.c_str());

    // Create module instance
    IModule* module = moduleInfo->factory();
    if (!module) {
        Serial.printf("Failed to create module instance: %s\n", moduleInfo->name.c_str());
        delete moduleInfo;
        vTaskDelete(NULL);
        return;
    }

    // Add to active modules
    active_modules.push_back(module);

    // Note: Configuration is now handled by the module itself in Run() method
    // after waiting for config readiness
    
    module->Setup();
    module->Run(parameter);

    // Remove from active modules
    active_modules.erase(std::remove(active_modules.begin(), active_modules.end(), module), active_modules.end());

    // Cleanup
    delete module;
    delete moduleInfo;

    vTaskDelete(NULL);
}

void ModuleManager::StopAllModules() {
    Serial.println("Stopping all modules...");

    for (TaskHandle_t taskHandle : taskHandles) {
        if (taskHandle != NULL) {
            vTaskDelete(taskHandle);
        }
    }

    taskHandles.clear();
    active_modules.clear();
}

}  // namespace modules