/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Hardware/Buttons/Buttons.cpp

#include "Buttons.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

#include "../../Module/ModuleController.h"


Buttons::Buttons(ModuleController& controller)
      : Module(controller,
               /* id                  */ "buttons",
               /* name                */ "Buttons",
               /* description         */ "Allows to bind CLI cmds to physical buttons",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
{
    commands_storage.push_back(Command{
        "add",
        "Add a button mapping: <pin> \"<$cmd ...>\" [pullup|pulldown] [on_press|on_release|on_change] [debounce_ms]",
        std::string("$") + id + " add 9 \"$system reboot\" pullup on_press 50",
        5,
        [this](std::span<const std::string> args){ button_add_cli(args); }
    });
    commands_storage.push_back(Command{
        "remove",
        "Remove ALL button mappings bound to a specific pin",
        std::string("$") + id + " remove 9",
        1,
        [this](std::span<const std::string> args){ button_remove_cli(args); }
    });
}

void Buttons::begin_routines_regular(const ModuleConfig& cfg) {
    (void)cfg;

    if (is_enabled() && !loaded_from_nvs) {
        load_from_nvs();
    }
}

void Buttons::loop() {
    for (auto& button : data.buttons) {
        const int current_state = digitalRead(button.pin);

        if (current_state != button.last_flicker_state) {
            button.last_debounce_time = millis();
        }
        button.last_flicker_state = current_state;

        if ((millis() - button.last_debounce_time) <= button.debounce_interval) {
            continue;
        }
        if (current_state == button.last_steady_state) {
            continue;
        }

        button.last_steady_state = current_state;

        const auto input_mode = static_cast<ButtonInputMode>(button.type);
        const auto trigger = static_cast<ButtonTriggerEvent>(button.event);
        const bool is_pressed =
            (input_mode == ButtonInputMode::Pullup)
                ? (current_state == LOW)
                : (current_state == HIGH);

        const bool should_trigger =
            trigger == ButtonTriggerEvent::OnChange ||
            (trigger == ButtonTriggerEvent::OnPress && is_pressed) ||
            (trigger == ButtonTriggerEvent::OnRelease && !is_pressed);

        if (should_trigger) {
            controller.command_executor.parse(button.command);
        }
    }
}

void Buttons::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    clear_nvs();
    data.buttons.clear();
    loaded_from_nvs = false;
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string Buttons::status(const bool verbose) const {
    if (is_disabled()) return "Buttons module disabled";

    std::string s;
    if (data.buttons.empty()) {
        s = "No buttons are currently active in memory.";
    } else {
        s = "--- Active Button Instances (Live) ---\n";
        for (const auto& btn : data.buttons) {
            s += "  - Pin: " + std::to_string(btn.pin)
               + ", ID: " + std::to_string(btn.b_id)
               + ", CMD: \"" + btn.command + "\"\n";
        }
        s += "------------------------------------";
    }

    if (verbose) controller.serial_port.print(s);
    return s;
}

void Buttons::load_configs(const std::vector<std::string>& configs) {
    if (is_disabled()) return;

    data.buttons.clear();
    for (const auto& cfg : configs) {
        if (!cfg.empty()) {
            (void)add_button_from_config(cfg);
        }
    }
    loaded_from_nvs = true;
}

bool Buttons::initialize_button(ButtonData& button) {
    const auto input_mode = static_cast<ButtonInputMode>(button.type);
    if (input_mode == ButtonInputMode::Pulldown) {
        pinMode(button.pin, INPUT_PULLDOWN);
    } else {
        pinMode(button.pin, INPUT_PULLUP);
    }

    button.last_steady_state = digitalRead(button.pin);
    button.last_flicker_state = button.last_steady_state;
    button.last_debounce_time = 0;
    return true;
}

bool Buttons::add_button_from_config(const std::string& config) {
    if (is_disabled()) return false;

    ButtonData new_button;
    if (!parse_config_string(config, new_button)) return false;

    uint32_t next_b_id = 0;
    for (const auto& button : data.buttons) {
        if (button.b_id >= next_b_id) {
            next_b_id = button.b_id + 1;
        }
    }
    new_button.b_id = next_b_id;

    if (!initialize_button(new_button)) return false;

    data.buttons.push_back(std::move(new_button));
    std::sort(data.buttons.begin(), data.buttons.end(),
        [](const ButtonData& a, const ButtonData& b) {
            return a.b_id < b.b_id;
        });

    return true;
}

void Buttons::remove_button(uint8_t pin) {
    if (is_disabled()) return;

    data.buttons.erase(
        std::remove_if(data.buttons.begin(), data.buttons.end(),
            [pin](const ButtonData& button) { return button.pin == pin; }),
        data.buttons.end());
}

bool Buttons::parse_config_string(const std::string& config, ButtonData& button) const {
    std::string s = config;
    xewe::str::trim(s);

    auto sp = s.find(' ');
    if (sp == std::string::npos) return false;

    try {
        const std::string pin_text = s.substr(0, sp);
        std::size_t consumed = 0;
        const long parsed_pin = std::stol(pin_text, &consumed);
        if (consumed != pin_text.size()
            || parsed_pin < 0
            || parsed_pin > std::numeric_limits<uint8_t>::max()) {
            return false;
        }
        button.pin = static_cast<uint8_t>(parsed_pin);
    } catch (...) {
        return false;
    }

    s = s.substr(sp + 1);
    xewe::str::trim(s);

    if (s.empty() || s.front() != '"') return false;
    const auto endq = s.find('"', 1);
    if (endq == std::string::npos) return false;

    button.command = s.substr(1, endq - 1);
    s = s.substr(endq + 1);
    xewe::str::trim(s);

    std::string type_str = "pullup";
    std::string event_str = "on_press";
    std::string debounce_str = "50";

    if (!s.empty()) {
        sp = s.find(' ');
        if (sp == std::string::npos) {
            type_str = s;
            s.clear();
        } else {
            type_str = s.substr(0, sp);
            s = s.substr(sp + 1);
            xewe::str::trim(s);
        }

        if (!s.empty()) {
            sp = s.find(' ');
            if (sp == std::string::npos) {
                event_str = s;
                s.clear();
            } else {
                event_str = s.substr(0, sp);
                s = s.substr(sp + 1);
                xewe::str::trim(s);
            }

            if (!s.empty()) debounce_str = s;
        }
    }

    button.type = static_cast<uint8_t>(
        type_str == "pulldown"
            ? ButtonInputMode::Pulldown
            : ButtonInputMode::Pullup);

    if (event_str == "release" || event_str == "on_release") {
        button.event = static_cast<uint8_t>(ButtonTriggerEvent::OnRelease);
    } else if (event_str == "change" || event_str == "on_change") {
        button.event = static_cast<uint8_t>(ButtonTriggerEvent::OnChange);
    } else {
        button.event = static_cast<uint8_t>(ButtonTriggerEvent::OnPress);
    }

    try {
        if (!debounce_str.empty() && debounce_str.front() == '-') return false;
        std::size_t consumed = 0;
        const unsigned long debounce = std::stoul(debounce_str, &consumed);
        if (consumed != debounce_str.size()
            || debounce > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        button.debounce_interval = static_cast<uint32_t>(debounce);
    } catch (...) {
        button.debounce_interval = 50;
    }

    return true;
}

bool Buttons::has_exact_button(const ButtonData& candidate) const {
    return std::any_of(data.buttons.begin(), data.buttons.end(),
        [&](const ButtonData& existing) {
            return existing.pin == candidate.pin
                && existing.command == candidate.command
                && existing.debounce_interval == candidate.debounce_interval
                && existing.type == candidate.type
                && existing.event == candidate.event;
        });
}

void Buttons::load_from_nvs() {
    if (is_disabled()) return;

    ButtonsData loaded;
    if (controller.nvs.read_flex(id, STORAGE_KEY, loaded)) {
        data = std::move(loaded);
        for (auto& button : data.buttons) {
            (void)initialize_button(button);
        }
        loaded_from_nvs = true;
        return;
    }

    if (!migrate_legacy_nvs()) {
        data.buttons.clear();
        loaded_from_nvs = true;
    }
}

bool Buttons::save_to_nvs() {
    return controller.nvs.write_flex(id, STORAGE_KEY, data);
}

void Buttons::clear_nvs() {
    controller.nvs.remove(id, STORAGE_KEY);

    const uint8_t legacy_count = controller.nvs.read<uint8_t>(id, "btn_count", 0);
    clear_legacy_nvs(legacy_count);
}

bool Buttons::migrate_legacy_nvs() {
    const uint8_t legacy_count = controller.nvs.read<uint8_t>(id, "btn_count", 0);
    if (legacy_count == 0) return false;

    std::vector<std::string> configs;
    configs.reserve(legacy_count);

    for (uint8_t i = 0; i < legacy_count; ++i) {
        const std::string key = "btn_cfg_" + std::to_string(i);
        std::string config = controller.nvs.read<std::string>(id, key);
        if (!config.empty()) {
            configs.push_back(std::move(config));
        }
    }

    load_configs(configs);
    if (!save_to_nvs()) {
        return false;
    }

    clear_legacy_nvs(legacy_count);
    loaded_from_nvs = true;
    return true;
}

void Buttons::clear_legacy_nvs(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        const std::string key = "btn_cfg_" + std::to_string(i);
        controller.nvs.remove(id, key);
    }
    controller.nvs.remove(id, "btn_count");
}

void Buttons::button_add_cli(std::span<const std::string> args) {
    if (is_disabled()) return;

    if (!is_enabled()) {
        controller.serial_port.print("Buttons Module is disabled. Use '$buttons enable'");
        return;
    }

    const std::string config = args[0] + " \"" + args[1] + "\" "
                             + args[2] + " " + args[3] + " " + args[4];

    ButtonData candidate;
    if (!parse_config_string(config, candidate)) {
        controller.serial_port.print("Error: Invalid button configuration string.");
        return;
    }

    if (has_exact_button(candidate)) {
        controller.serial_port.print("Error: This exact button configuration already exists.");
        return;
    }

    ButtonsData previous = data;
    if (!add_button_from_config(config)) {
        controller.serial_port.print("Error: Invalid button configuration string.");
        return;
    }

    if (!save_to_nvs()) {
        data = std::move(previous);
        controller.serial_port.print("Error: Could not save button configuration.");
        return;
    }

    controller.serial_port.print("Successfully added button action: " + config);
}

void Buttons::button_remove_cli(std::span<const std::string> args) {
    if (is_disabled()) return;

    if (!is_enabled()) {
        controller.serial_port.print("Buttons Module is disabled. Use '$buttons enable'");
        return;
    }

    std::string pin_str = args[0];
    xewe::str::trim(pin_str);

    long parsed_pin = -1;
    try {
        std::size_t consumed = 0;
        parsed_pin = std::stol(pin_str, &consumed);
        if (consumed != pin_str.size()) parsed_pin = -1;
    } catch (...) {
        parsed_pin = -1;
    }

    if (parsed_pin < 0 || parsed_pin > std::numeric_limits<uint8_t>::max()) {
        controller.serial_port.print("Error: Invalid pin number provided.");
        return;
    }

    const uint8_t pin = static_cast<uint8_t>(parsed_pin);
    const auto previous_size = data.buttons.size();
    ButtonsData previous = data;
    remove_button(pin);

    if (data.buttons.size() == previous_size) {
        controller.serial_port.print("Error: No active buttons found on pin " + pin_str);
        return;
    }

    if (!save_to_nvs()) {
        data = std::move(previous);
        controller.serial_port.print("Error: Could not save button configuration.");
        return;
    }

    controller.serial_port.print("Successfully removed ALL buttons mapped to pin " + pin_str);
}
