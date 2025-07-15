#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <freertos/FreeRTOS.h>
#include "logger.h"
#include <freertos/task.h>
#include "../config_manager.h"
#include "module_registry.h"

namespace modules {

class ModuleManager {
  public:
    // Start all registered modules
    static void StartAllModules();

    // Universal module task wrapper
    static void ModuleTaskWrapper(void* parameter);

    // Stop all modules
    static void StopAllModules();

  private:
    static std::vector<TaskHandle_t> taskHandles;
};

}  // namespace modules

#endif  // MODULE_MANAGER_H