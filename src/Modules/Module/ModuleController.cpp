#include "ModuleController.h"

ModuleController::ModuleController()
  : serial_port(*this)
  , nvs(*this)
  , system(*this)
  , command_executor(*this)
//   , wifi(*this)
{
    register_module(serial_port);
    register_module(nvs);
    register_module(system);
    register_module(command_executor);
//     register_module(wifi);
}

void ModuleController::begin() {
    const bool init_setup_flag = !nvs.read_bool("root", "init_setup_flag");

    serial_port.begin               (SerialPortConfig       {});
    nvs.begin                       (NvsConfig              {});
    system.begin                    (SystemConfig           {});
    command_executor.begin          (CommandExecutorConfig  {});

//     wifi.begin                      (WifiConfig  {});

    if (init_setup_flag) {
        serial_port.print_header("Initial Setup Complete");
        nvs.write_bool("root", "init_setup_flag", true);
        system.restart();
    }

    serial_port.print_header("System Setup Complete");
}

void ModuleController::loop() {
    for (auto& [id, module] : modules) {
        if (module->is_enabled()) {
            module->loop();
        }
    }
}

bool ModuleController::register_module(Module& module) {
    auto [it, inserted] = modules.emplace(
        std::string(module.get_id()),
        &module
    );

    return inserted;
}

Module* ModuleController::get_module(std::string_view id) {
    auto it = modules.find(std::string(id));

    if (it == modules.end()) { return nullptr; }

    return it->second;
}

void ModuleController::send_command(std::span<const std::string> recipients,
                                    std::string_view command_name,
                                    std::span<const std::string> args
                                   ) {
    for (const std::string& recipient_id : recipients) {
        Module* recipient = get_module(recipient_id);

        if (recipient == nullptr || recipient->is_disabled()) { continue; }

        for (const Command& command : recipient->get_commands()) {
            if (command.name != command_name)       { continue; }
            if (!command.function)                  { continue; }
            if (args.size() != command.arg_count)   { continue; }

            command.function(args);
            break;
        }
    }
}