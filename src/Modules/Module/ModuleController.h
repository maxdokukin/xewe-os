#pragma once

#include "Module.h"

#include "../Core/SerialPort/SerialPort.h"
#include "../Core/Nvs/Nvs.h"
#include "../Core/System/System.h"
#include "../Core/CommandExecutor/CommandExecutor.h"

#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class ModuleController {
public:
    ModuleController();

    void begin();
    void loop();

    bool register_module(Module& module);

    Module* get_module(std::string_view id);

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
    void load_registered_modules();
    void index_module_commands(Module& module);

    std::map<std::string, Module*> modules {};

    std::vector<std::unique_ptr<Module>> owned_modules {};

    std::map<
        std::string,
        std::map<std::string, const Command*>
    > command_index {};
};