#ifndef MODULE_REGISTRY_H
#define MODULE_REGISTRY_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include "module.h"

namespace modules {

// Structure to hold module metadata
struct ModuleInfo {
    String name;
    String configSection;
    int taskPriority;
    int stackSize;
    std::function<IModule*()> factory;

    ModuleInfo(const String& name, const String& configSection, int priority, int stack,
               std::function<IModule*()> factory)
        : name(name), configSection(configSection), taskPriority(priority), stackSize(stack), factory(factory) {}
};

class ModuleRegistry {
  private:
    static std::vector<ModuleInfo> modules;

  public:
    // Register a module
    static void RegisterModule(const String& name, const String& configSection, int priority, int stackSize,
                               std::function<IModule*()> factory);

    // Get all registered modules
    static const std::vector<ModuleInfo>& GetModules();

    // Get module by name
    static const ModuleInfo* GetModule(const String& name);

    // Print all registered modules
    static void PrintRegisteredModules();
};

// Helper class for auto-registration
class ModuleRegistrar {
  public:
    ModuleRegistrar(const String& name, const String& configSection, int priority, int stackSize,
                    std::function<IModule*()> factory) {
        ModuleRegistry::RegisterModule(name, configSection, priority, stackSize, factory);
    }
};

}  // namespace modules

// Macro for easy module registration
#define REGISTER_MODULE(name, configSection, priority, stackSize, moduleClass)                               \
    namespace {                                                                                              \
    static modules::ModuleRegistrar auto_registrar(name, configSection, priority, stackSize,                 \
                                                   []() -> modules::IModule* { return new moduleClass(); }); \
    }

#endif  // MODULE_REGISTRY_H