#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "Module.h"

class ModuleController;

using ModuleFactory = std::function<std::unique_ptr<Module>(ModuleController&)>;

class ModuleRegistry {
public:
    static std::map<std::string, ModuleFactory>& get_registry() {
        static std::map<std::string, ModuleFactory> registry;
        return registry;
    }

    static bool register_module(
        std::string id,
        ModuleFactory factory
    ) {
        auto& registry = get_registry();

        if (registry.contains(id)) {
            return false;
        }

        registry.emplace(
            std::move(id),
            std::move(factory)
        );

        return true;
    }
};

template <typename T>
class ModuleRegistrar {
public:
    explicit ModuleRegistrar(std::string id) {
        const bool registered = ModuleRegistry::register_module(
            std::move(id),
            [](ModuleController& controller) {
                return std::make_unique<T>(controller);
            }
        );

        (void)registered;
    }
};