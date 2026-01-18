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
  , wifi(*this)
  , buttons(*this)
{
    modules[0] = &serial_port;
    modules[1] = &nvs;
    modules[2] = &system;
    modules[3] = &command_parser;
    modules[4] = &wifi;
    modules[5] = &buttons;
}

void SystemController::begin() {
    bool init_setup_flag = !system.init_setup_complete();

    serial_port.begin           (SerialPortConfig   {});
    nvs.begin                   (NvsConfig          {});
    system.begin                (SystemConfig       {});
    wifi.begin                  (WifiConfig         {});
    buttons.begin               (ButtonsConfig      {});

    // should be initialized last to collect all cmds
    command_parser.begin        (CommandParserConfig(modules, MODULE_COUNT));

    if (init_setup_flag) {
        serial_port.print_header("Initial Setup Complete");
        system.restart();
    }
    serial_port.print_header("System Setup Complete");
}

void SystemController::loop() {
    for (size_t i = 0; i < MODULE_COUNT; ++i) {
        if (modules[i]->is_enabled())
            modules[i]->loop();
    }
    if (serial_port.has_line()) {
        command_parser.parse(serial_port.read_line());
    }
}