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
  , pins(*this)
  , buttons(*this)
  , wifi(*this)
  , web_interface(*this)
{
    modules.push_back(&serial_port);
    modules.push_back(&nvs);
    modules.push_back(&system);
    modules.push_back(&command_parser);
    modules.push_back(&pins);
    modules.push_back(&buttons);
    modules.push_back(&wifi);
    modules.push_back(&web_interface);
}

void SystemController::begin() {
    bool init_setup_flag = !system.init_setup_complete();

    serial_port.begin               (SerialPortConfig       {});
    nvs.begin                       (NvsConfig              {});
    system.begin                    (SystemConfig           {});
    pins.begin                      (PinsConfig             {});
    buttons.begin                   (ButtonsConfig          {});
    wifi.begin                      (WifiConfig             {});
    web_interface.add_requirement   (wifi);
    web_interface.begin             (WebInterfaceConfig     {});

    // should be initialized last to collect all cmds
    command_parser.begin            (CommandParserConfig    {});

    if (init_setup_flag) {
        serial_port.print_header("Initial Setup Complete");
        system.restart();
    }
    serial_port.print_header("System Setup Complete");
}

void SystemController::loop() {
    for (Module* m : modules) {
        if (m && m->is_enabled())
            m->loop();
    }

    if (serial_port.has_line()) {
        command_parser.parse(serial_port.read_line());
    }
}
