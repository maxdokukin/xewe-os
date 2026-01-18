/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// <filepath from project root>


#include "ModuleName.h"
#include "../../../SystemController/SystemController.h"


ModuleName::ModuleName(SystemController& controller)
      : Module(controller,
               /* module_name         */ "",
               /* module_description  */ "",
               /* nvs_key             */ "",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}

void ModuleName::begin_routines_required (const ModuleConfig& cfg) {
//    const auto& config = static_cast<const ModuleNameConfig&>(cfg);
    // do your custom routines here
}

void ModuleName::begin_routines_init (const ModuleConfig& cfg) {
//    const auto& config = static_cast<const ModuleNameConfig&>(cfg);
    // do your custom routines here
}

void ModuleName::begin_routines_regular (const ModuleConfig& cfg) {
//    const auto& config = static_cast<const ModuleNameConfig&>(cfg);
    // do your custom routines here
}

void ModuleName::begin_routines_common (const ModuleConfig& cfg) {
//    const auto& config = static_cast<const ModuleNameConfig&>(cfg);
    // do your custom routines here
}

void ModuleName::loop () {
    // do your custom routines here
}

void ModuleName::enable (const bool verbose, const bool do_restart) {
    // do your custom routines here
    return Module::enable(verbose, do_restart);
}

void ModuleName::disable (const bool verbose, const bool do_restart) {
    // do your custom routines here
    Module::disable(verbose, do_restart);
}

void ModuleName::reset (const bool verbose, const bool do_restart) {
    // do your custom routines here
    Module::reset(verbose, do_restart);
}

std::string ModuleName::status (const bool verbose, const bool do_restart) const {
    // do your custom routines here
    return Module::status(verbose, do_restart);
}
