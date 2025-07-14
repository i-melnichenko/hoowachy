#include "module_registry.h"

namespace modules {

std::vector<ModuleInfo> ModuleRegistry::modules;

void ModuleRegistry::RegisterModule(const String& name, const String& configSection, int priority, int stackSize,
                                    std::function<IModule*()> factory) {
    Serial.printf("Registering module: %s\n", name.c_str());
    modules.emplace_back(name, configSection, priority, stackSize, factory);
}

const std::vector<ModuleInfo>& ModuleRegistry::GetModules() { return modules; }

const ModuleInfo* ModuleRegistry::GetModule(const String& name) {
    for (const auto& module : modules) {
        if (module.name == name) {
            return &module;
        }
    }
    return nullptr;
}

void ModuleRegistry::PrintRegisteredModules() {
    Serial.println("Registered modules:");
    for (const auto& module : modules) {
        Serial.printf("  - %s (config: %s, priority: %d, stack: %d)\n", module.name.c_str(),
                      module.configSection.c_str(), module.taskPriority, module.stackSize);
    }
}

}  // namespace modules