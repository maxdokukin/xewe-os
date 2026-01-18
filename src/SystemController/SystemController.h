/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
#pragma once

#include <vector>

#include "../Modules/Module/Module.h"
#include "../Modules/Software/SerialPort/SerialPort.h"
#include "../Modules/Software/Nvs/Nvs.h"
#include "../Modules/Software/System/System.h"
#include "../Modules/Software/CommandParser/CommandParser.h"
constexpr size_t MODULE_COUNT    = 4;

class SystemController {
public:
    SystemController();

    void                        begin();
    void                        loop();
//    void add_module(Module& m)
    SerialPort                  serial_port;
    Nvs                         nvs;
    System                      system;
    CommandParser               command_parser;
private:
    Module*                     modules                     [MODULE_COUNT] = {};
    vector<CommandsGroup>       command_groups;
};