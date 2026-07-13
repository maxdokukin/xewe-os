// src/Modules/Module/Module.cpp
#include "Module.h"
#include "ModuleController.h"


void Module::begin (const ModuleConfig& cfg) {
    // DBG_PRINTF(Module, "'%s'->begin(): Called.\n", name.c_str());

    bool first_boot = !controller.nvs.read<bool>(id, "not_first_boot");
    enabled = first_boot || controller.nvs.read<bool>(id, "is_enabled");

    if (can_be_disabled || requires_init_setup) {
         controller.serial_port.print_header(xewe::str::capitalize(name) + " Setup");
    }

    if (is_disabled(true)) return;

    if (!requirements_enabled(true)) {
        Serial.printf("%s requirements not enabled; skipping\n", name.c_str());
        enabled = false;
        controller.nvs.write<bool>(id, "is_enabled", false);
        controller.nvs.write<bool>(id, "not_first_boot", true);
        return;
    }

    if (first_boot) {
        if (can_be_disabled) {
            controller.serial_port.print_header(std::string("Would you like to enable ") + xewe::str::capitalize(name) + " module?\n\n" + description);
            enabled = controller.serial_port.get_yn();

            if (!enabled) {
                controller.nvs.write<bool>(id, "is_enabled", false);
                controller.nvs.write<bool>(id, "not_first_boot", true);
                return;
            }
        }
        controller.nvs.write<bool>(id, "is_enabled", true);
        controller.nvs.write<bool>(id, "not_first_boot", true);
    }

    begin_routines_required(cfg);

    if (!init_setup_complete()) {
        begin_routines_init(cfg);
        if (enabled) { // could have been disabled during begin_routines_init()
            controller.nvs.write<bool>(id, "init_complete", true);
        }
    } else {
        begin_routines_regular(cfg);
    }

    begin_routines_common(cfg);
}

void Module::begin_routines_required(const ModuleConfig&) {}
void Module::begin_routines_init(const ModuleConfig&) {}
void Module::begin_routines_regular(const ModuleConfig&) {}
void Module::begin_routines_common(const ModuleConfig&) {}

void Module::loop() {}

void Module::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    // DBG_PRINTF(Module, "'%s'->reset(v=%d, r=%d, k=%d): Called.\n", name.c_str(), verbose, do_restart, keep_enabled);

    controller.nvs.reset_ns(id);
    controller.nvs.write<bool>(id, "not_first_boot", true);
    // DBG_PRINTF(Module, "'%s': NVS namespace wiped and re-initialized.\n", name.c_str());

    enabled = !can_be_disabled || keep_enabled;

    if (keep_enabled) {
        // DBG_PRINTF(Module, "'%s': Persisting 'is_enabled'=true to NVS.\n", name.c_str());
        controller.nvs.write<bool>(id, "is_enabled", true);
    }

    if (verbose) Serial.printf("%s module reset\n", name.c_str());

    if (do_restart) {
        // DBG_PRINTF(Module, "'%s': do_restart is true. Rebooting system now.\n", name.c_str());
        if (verbose) Serial.printf("Restarting...\n\n\n");
        ESP.restart();
    }
}

// returns success of the operation
void Module::enable(const bool verbose, const bool do_restart) {
    // DBG_PRINTF(Module, "'%s'->enable(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    if (is_enabled()){
        // DBG_PRINTLN(Module, "enable(): Module is already enabled.");
        Serial.printf("%s module already enabled\n", name.c_str());
        return;
    }
    if (!requirements_enabled(true)) {
//        Serial.printf("%s Module: requirements not enabled; enable them first\n", name.c_str());
        return;
    }
    enabled = true;
    // DBG_PRINTLN(Module, "enable(): Writing 'is_enabled'=true to NVS.");
    controller.nvs.write<bool>(id, "is_enabled", true);
    if (verbose) Serial.printf("%s module enabled. Restarting...\n\n\n", name.c_str());
    ESP.restart();
    return;
}

void Module::disable(const bool verbose, const bool do_restart) {
    // DBG_PRINTF(Module, "'%s'->disable(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");

    if (is_disabled()){
        // DBG_PRINTF(Module, "'%s': Already disabled. Returning.\n", name.c_str());
        if (verbose) Serial.printf("%s module already disabled\n", name.c_str());
        return;
    }
    if (!can_be_disabled) {
        // DBG_PRINTF(Module, "'%s': Locked (can_be_disabled=false). Returning.\n", name.c_str());
        if (verbose) Serial.printf("%s module can't be disabled\n", name.c_str());
        return;
    }

    bool disable_confirmed = true;

    if (verbose) {
        // DBG_PRINTF(Module, "'%s': Preparing confirmation prompt.\n", name.c_str());
        std::string msg = "[WARNING]\nDisabling " + name + "\nWill reset it";
        if (!dependent_modules.empty()) {
            msg += ", and all dependents: \\sep";
            for (auto* m : dependent_modules) {
                msg += m->name + "\n";
            }
            if (!msg.empty() && msg.back() == '\n')
               msg.pop_back();
        }
        controller.serial_port.print_header(msg);
        disable_confirmed = controller.serial_port.get_yn("OK?");
        // DBG_PRINTF(Module, "'%s': User confirmation result: %s\n", name.c_str(), disable_confirmed ? "YES" : "NO");
    }

    if (!disable_confirmed) {
        // DBG_PRINTF(Module, "'%s': Action aborted.\n", name.c_str());
        controller.serial_port.print("Aborted");
        return;
    }

    if (!dependent_modules.empty()) {
        // DBG_PRINTF(Module, "'%s': Disabling %d dependencies.\n", name.c_str(), dependent_modules.size());
        for (auto* m : dependent_modules) {
            // DBG_PRINTF(Module, "'%s': recursing disable() on dependent '%s'.\n", name.c_str(), m->name.c_str());
            if (verbose) Serial.printf("%s module reset and disabled\n", m->name.c_str());
            m->disable(false, false); // disable with no verbose, and dont reboot
        }
    }
    if (verbose) {
        Serial.printf("%s module disabled\n", name.c_str());
    }

    // DBG_PRINTF(Module, "'%s': Executing final reset().\n", name.c_str());
    reset(verbose, do_restart, false);
    return;
}

std::string Module::status(bool verbose) const {
    // DBG_PRINTF(Module, "'%s'->status(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    std::string status_str = (name + " module " + (controller.nvs.read<bool>(id, "is_enabled") ? "enabled" : "disabled"));
    // DBG_PRINTF(Module, "status(): Generated status std::string: '%s'.\n", status_str.c_str());
    if (verbose) Serial.printf("%s\n", status_str.c_str());
    return status_str;
}

// only print the debug msg if true
bool Module::is_enabled(bool verbose) const {
    // DBG_PRINTF(Module, "'%s'->is_enabled(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    if (can_be_disabled) {
        // DBG_PRINTF(Module, "is_enabled(): Module can be disabled, read NVS 'is_enabled' flag as %s.\n", enabled ? "true" : "false");
        if (verbose && enabled) Serial.printf("%s module enabled\n", name.c_str());
        return enabled;
    }
    // DBG_PRINTLN(Module, "is_enabled(): Module cannot be disabled, returning true by default.");
    return true;
}

// only print the debug msg if true
bool Module::is_disabled(bool verbose) const {
    // DBG_PRINTF(Module, "'%s'->is_disabled(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    if (can_be_disabled) {
        if (verbose && !enabled) Serial.printf("%s module disabled; to enable:\n$%s enable\n", name.c_str(), xewe::str::lower(name).c_str());
        return !enabled;
    }
    // DBG_PRINTLN(Module, "is_disabled(): Module cannot be disabled, returning false by default.");
    return false;
}

bool Module::init_setup_complete (bool verbose) const {
    // DBG_PRINTF(Module, "'%s'->init_setup_complete(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    bool init_complete = controller.nvs.read<bool>(id, "init_complete");
    bool result = !requires_init_setup || init_complete;
    // DBG_PRINTF(Module, "init_setup_complete(): requires_init_setup=%s, nvs 'stp_cmp' flag=%s. Final result=%s\n",
//         requires_init_setup ? "true" : "false",
//         init_complete ? "true" : "false",
//         result ? "true" : "false"
//     );
    return result;
}

void Module::register_generic_commands() {
    commands_storage.push_back(Command{
        "status",
        "Get module status",
        std::string("$") + id + " status",
        0,
        [this](std::span<const std::string>) {
            status(true);
        }
    });

    commands_storage.push_back(Command{
        "reset",
        "Reset the module",
        std::string("$") + id + " reset",
        0,
        [this](std::span<const std::string>) {
            reset(true, true);
        }
    });

    if (can_be_disabled) {
        commands_storage.push_back(Command{
            "enable",
            "Enable this module",
            std::string("$") + id + " enable",
            0,
            [this](std::span<const std::string>) {
                enable(true, true);
            }
        });

        commands_storage.push_back(Command{
            "disable",
            "Disable this module",
            std::string("$") + id + " disable",
            0,
            [this](std::span<const std::string>) {
                disable(true, true);
            }
        });
    }
}

void Module::run_with_dots(const std::function<void()>& work, uint32_t duration_ms, uint32_t dot_interval_ms) {
  if (dot_interval_ms == 0) dot_interval_ms = 1;

  const uint32_t start = millis();
  uint32_t next = start;  // first dot at t=0

  while ((uint32_t)(millis() - start) < duration_ms) {
    work();  // run the target function

    const uint32_t now = millis();
    if ((int32_t)(now - next) >= 0) {
      // controller.serial_port.print(std::string_view{"."});

      // If we're late by multiple intervals, skip ahead (prevents dot bursts)
      const uint32_t late = now - next;
      const uint32_t intervals = 1u + (late / dot_interval_ms);
      next += intervals * dot_interval_ms;
    }
  }
  // controller.serial_port.print(std::string_view{"\n"});
}

void Module::add_requirement(Module& other) {
    required_modules.push_back(&other);
    other.dependent_modules.push_back(this);
}

bool Module::requirements_enabled(bool verbose) const {
    // DBG_PRINTF(Module, "'%s'->requirements_enabled(verbose=%s): Called.\n", name.c_str(), verbose ? "true" : "false");
    bool all_enabled = true;
    for (auto* r : required_modules) {
        bool req_enabled = r->is_enabled();
        all_enabled = all_enabled && req_enabled;
        if (!req_enabled && verbose)
            Serial.printf("%s Module requires %s module; to enable:\n$%s enable\n", name.c_str(), r->name.c_str(), xewe::str::lower(r->name).c_str());
    }
    // DBG_PRINTF(Module, "requirements_enabled(): Result=%s.\n", all_enabled ? "true" : "false");
    return all_enabled;
}
