#include "ModuleController.h"

#include "ModuleRegistry.h"

#include <utility>

ModuleController::ModuleController()
  : serial_port(*this)
  , nvs(*this)
  , system(*this)
  , command_executor(*this)
{
    register_module(serial_port);
    register_module(nvs);
    register_module(system);
    register_module(command_executor);

    load_registered_modules();
}

bool ModuleController::register_module(Module& module) {
    return register_module_with_type(nullptr, module);
}

bool ModuleController::register_module_with_type(ModuleType type, Module& module) {
    auto [it, inserted] = modules.emplace(
        std::string(module.get_id()),
        ModuleRecord{
            type,
            &module
        }
    );

    if (!inserted) {
        return false;
    }

    index_module_commands(module);

    return true;
}

void ModuleController::load_registered_modules() {
    for (const auto& [id, factory] : ModuleRegistry::get_registry()) {
        std::unique_ptr<Module> owned_module = factory(*this);

        if (!owned_module) {
            continue;
        }

        Module& module = *owned_module;

        if (!register_module(module)) {
            continue;
        }

        owned_modules.push_back(std::move(owned_module));
    }
}

void ModuleController::index_module_commands(Module& module) {
    const std::string id(module.get_id());

    auto& module_commands = command_index[id];

    for (const Command& command : module.get_commands()) {
        if (command.name.empty()) {
            continue;
        }

        if (!command.function) {
            continue;
        }

        module_commands.emplace(command.name, &command);
    }
}

Module* ModuleController::get_module_by_id(std::string_view id) {
    auto it = modules.find(std::string(id));

    if (it == modules.end()) {
        return nullptr;
    }

    return it->second.module;
}

void ModuleController::send_command(
    Module* sender,
    std::span<const std::string> recipients,
    std::string_view command_name,
    std::span<const std::string> args
) {
    if (sender == nullptr) {
        return;
    }

    for (const std::string& recipient_id : recipients) {
        auto module_it = command_index.find(recipient_id);

        if (module_it == command_index.end()) {
            continue;
        }

        auto& module_commands = module_it->second;

        auto command_it = module_commands.find(std::string(command_name));

        if (command_it == module_commands.end()) {
            continue;
        }

        const Command* command = command_it->second;

        if (command == nullptr) {
            continue;
        }

        if (args.size() != command->arg_count) {
            continue;
        }

        command->function(args);
    }
}

void ModuleController::begin() {
    const bool init_setup_flag = !nvs.read_bool("root", "init_setup_flag");

    serial_port.begin               (SerialPortConfig       {});
    nvs.begin                       (NvsConfig              {});
    system.begin                    (SystemConfig           {});
    command_executor.begin          (CommandExecutorConfig  {});

    for (auto& module : owned_modules) {
        if (module) {
            module->begin(ModuleConfig{});
        }
    }

    if (init_setup_flag) {
        serial_port.print_header("Initial Setup Complete");
        nvs.write_bool("root", "init_setup_flag", true);
        system.restart();
    }

    serial_port.print_header("System Setup Complete");
}

void ModuleController::loop() {
    for (auto& [id, module] : modules) {
        if (module && module->is_enabled()) {
            module->loop();
        }
    }
}