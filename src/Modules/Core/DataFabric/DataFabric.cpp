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

// src/Modules/Core/DataFabric/DataFabric.cpp
#include "DataFabric.h"
#include "../../Module/ModuleController.h"


DataFabric::DataFabric(ModuleController& controller)
      : Module(controller,
               /* id                  */ "fabric",
               /* name                */ "DataFabric",
               /* description         */ "Persists typed objects as blobs and serves them as JSON",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}


Nvs& DataFabric::nvs_ref() const {
    return controller.nvs;
}


void DataFabric::remove(std::string_view ns, std::string_view key) {
    controller.nvs.remove(ns, key);
}
