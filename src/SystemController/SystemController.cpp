/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/SystemController.cpp
#include "SystemController.h"

SystemController::SystemController()
  : serial_port(*this)
  , nvs(*this)
  , system(*this)
  , command_parser(*this)
{
    modules[0] = &serial_port;
    modules[1] = &nvs;
    modules[2] = &system;
    modules[3] = &command_parser;
}

void SystemController::begin() {
    bool init_setup_flag = !system.init_setup_complete();

    serial_port.begin           (SerialPortConfig   {});
    nvs.begin                   (NvsConfig          {});
    system.begin                (SystemConfig       {});

    if (init_setup_flag) {
        // serial_port.print_spacer();
        // serial_port.print_centered("Initial Setup Complete", 50);
        // serial_port.print_spacer();
        // serial_port.print_centered("Rebooting...");
        // serial_port.print_spacer();
        delay(3000);
        ESP.restart();
    }

    // this can be moved inside of the module begin
    command_groups.clear();
    for (auto module : modules) {
        auto grp = module->get_commands_group();
        if (!grp.commands.empty()) {
            command_groups.push_back(grp);
        }
    }

    CommandParserConfig parser_cfg;
    parser_cfg.groups      = command_groups.data();
    parser_cfg.group_count = command_groups.size();
    // this can be moved inside of the module begin
    command_parser.begin(parser_cfg);

    // serial_port.print_spacer();
    // serial_port.print_centered("System Setup Complete", 50);
    // serial_port.print_spacer();
}

void SystemController::loop() {
    for (size_t i = 0; i < MODULE_COUNT; ++i) {
        modules[i]->loop();
    }
    if (serial_port.has_line()) {
        command_parser.parse(serial_port.read_line());
    }
}