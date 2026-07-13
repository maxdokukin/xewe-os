#pragma once

#include "Module.h"

#include "../Core/SerialPort/SerialPort.h"
#include "../Core/Nvs/Nvs.h"
#include "../Core/System/System.h"
#include "../Core/CommandExecutor/CommandExecutor.h"

#include "../Software/Wifi/Wifi.h"
#include "../Software/WebInterface/WebInterface.h"

#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class ModuleController {
public:
    using ModuleType = const void*;

    struct ModuleRecord {
        ModuleType type {};
        Module* module {};

        explicit operator bool() const {
            return module != nullptr;
        }

        operator Module*() const {
            return module;
        }

        Module* operator->() const {
            return module;
        }
    };

    ModuleController();

    void begin();
    void loop();

    bool register_module(Module& module);

    template <typename T>
    bool register_module(T& module) {
        return register_module_with_type(
            module_type_key<T>(),
            static_cast<Module&>(module)
        );
    }

    Module* get_module_by_id(std::string_view id);

    template <typename T>
    T* get_module_by_type() {
        const ModuleType type = module_type_key<T>();

        for (const auto& [id, record] : modules) {
            if (record.type == type) {
                return static_cast<T*>(record.module);
            }
        }

        return nullptr;
    }

    const std::map<std::string, ModuleRecord>& get_modules() const { return modules; }

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

    bool register_module_with_type(ModuleType type, Module& module);

    void load_registered_modules();
    void index_module_commands(Module& module);

    std::map<std::string, ModuleRecord> modules {};

    std::vector<std::unique_ptr<Module>> owned_modules {};

    std::map<
        std::string,
        std::map<std::string, const Command*>
    > command_index {};
};