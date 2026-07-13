#pragma once

#include "Module.h"

#include "../Core/SerialPort/SerialPort.h"
#include "../Core/Nvs/Nvs.h"
#include "../Core/System/System.h"
#include "../Core/CommandExecutor/CommandExecutor.h"

#include <map>
#include <span>
#include <string>
#include <string_view>

class ModuleController {
public:
    using ModuleType = const void*;

    ModuleController();

    void begin();
    void loop();

    bool register_module(Module& module);

    template <typename T>
    bool register_module(T& module) {
        modules[module_type_key<T>()] = &module;
        index_module_commands(module);
        return true;
    }

    template <typename T>
    T* get_module() {
        auto it = modules.find(module_type_key<T>());

        if (it == modules.end()) {
            return nullptr;
        }

        return static_cast<T*>(it->second);
    }

    const std::map<ModuleType, Module*>& get_modules() const { return modules; }

    void send_command(
        Module* sender,
        std::span<const std::string> recipients,
        std::string_view command_name,
        std::span<const std::string> args
    );

    SerialPort                  serial_port;
    Nvs                         nvs;
    System                      system;
    CommandExecutor             command_executor;

private:
    template <typename T>
    static ModuleType module_type_key() {
        static const char key = 0;
        return &key;
    }

    void load_registered_modules();
    void index_module_commands(Module& module);

    std::map<ModuleType, Module*> modules {};

    std::map<
        std::string,
        std::map<std::string, const Command*>
    > command_index {};
};