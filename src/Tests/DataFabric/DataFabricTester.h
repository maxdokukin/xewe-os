/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Tests/DataFabric/DataFabricTester.h
#pragma once

#include "../../Modules/Module/Module.h"

#include <string_view>


struct DataFabricTesterConfig : public ModuleConfig {};


// Test module that exercises the DataFabric module (and, through it, the Nvs blob
// primitives) end to end. Run from the CLI with: $df_test run
//
// Method bodies live in DataFabricTester.cpp so they see the complete
// ModuleController type (they dereference controller.data_fabric / controller.nvs
// / controller.serial_port). Here we only forward-declare ModuleController (via
// Module.h). Test structs are defined at file scope in the .cpp.
class DataFabricTester : public Module {
public:
    explicit DataFabricTester(ModuleController& controller);

private:
    // All test data lives under this namespace so real module data is never
    // touched; it is wiped before and after a run.
    static constexpr const char* kNs = "df_test";

    int m_pass = 0;
    int m_fail = 0;

    void report(const char* label, bool ok);
    void clean();
    void run_tests();
};
