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
        "Add a button mapping: <pin> \"<$cmd ...>\" "
        "<pullup|pulldown> <on_press|on_release|on_change> <debounce_ms>",
        std::string("$") + id +
            " add 9 \"$system reboot\" pullup on_press 50",
        5,
        [this](std::span<const std::string> args) {
            button_add_cli(args);
        }
    });

    commands_storage.push_back(Command{
        "remove",
        "Remove a button mapping by its ID",
        std::string("$") + id + " remove 0",
        1,
        [this](std::span<const std::string> args) {
            button_remove_cli(args);
        }
    });
}

void Buttons::begin_routines_regular(const ModuleConfig& cfg) {
    load_from_nvs();
}

void Buttons::loop() {
    for (auto& button : data.buttons) {
        const uint32_t now = millis();
        const int current_state = digitalRead(button.pin);

        if (current_state != button.last_flicker_state) {
            button.last_debounce_time = now;
        }

        button.last_flicker_state = current_state;

        if ((now - button.last_debounce_time) <= button.debounce_interval) continue;
        if (current_state == button.last_steady_state) continue;

        button.last_steady_state = current_state;

        const auto type = static_cast<ButtonInputMode>(button.type);

        const auto event = static_cast<ButtonTriggerEvent>(button.event);

        const bool is_pressed = type == ButtonInputMode::PULL_UP
                                      ? current_state == LOW
                                      : current_state == HIGH;

        const bool should_trigger = event == ButtonTriggerEvent::ON_CHANGE ||
                                   (event == ButtonTriggerEvent::ON_PRESS && is_pressed) ||
                                   (event == ButtonTriggerEvent::ON_RELEASE && !is_pressed);

        if (should_trigger) controller.command_executor.parse(button.command);
    }
}

void Buttons::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    data.buttons.clear();
    controller.nvs.remove(id, "data");
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string Buttons::status(const bool verbose) const {
    if (is_disabled()) return Module::status(verbose);

    const auto mode_name = [](const uint8_t value) {
        return value == static_cast<uint8_t>(ButtonInputMode::PULL_UP)
            ? "pullup"
            : value == static_cast<uint8_t>(ButtonInputMode::PULL_DOWN)
                ? "pulldown"
                : "invalid";
    };

    const auto event_name = [](const uint8_t value) {
        switch (static_cast<ButtonTriggerEvent>(value)) {
            case ButtonTriggerEvent::ON_PRESS:   return "on_press";
            case ButtonTriggerEvent::ON_RELEASE: return "on_release";
            case ButtonTriggerEvent::ON_CHANGE:  return "on_change";
            default:                             return "invalid";
        }
    };

    std::ostringstream out;

    if (data.buttons.empty()) {
        out << "No buttons are currently active.";
    } else {
        out << "--- Active Buttons ---\n";

        for (const auto& button : data.buttons) {
            const bool pressed =
                button.type == static_cast<uint8_t>(ButtonInputMode::PULL_UP)
                    ? button.last_steady_state == LOW
                    : button.last_steady_state == HIGH;

            out << "ID: "             << button.id
                << ", Pin: "          << static_cast<unsigned>(button.pin)
                << ", CMD: \""        << button.command << '"'
                << ", Debounce: "     << button.debounce_interval << " ms"
                << ", Type: "         << mode_name(button.type) << " (" << static_cast<unsigned>(button.type) << ')'
                << ", Event: "        << event_name(button.event) << " (" << static_cast<unsigned>(button.event) << ')'
                << ", Debounce time: "<< button.last_debounce_time
                << ", Steady: "       << button.last_steady_state
                << ", Flicker: "      << button.last_flicker_state
                << ", State: "        << (pressed ? "pressed" : "released")
                << '\n';
        }

        out << "----------------------";
    }

    auto result = out.str();
    if (verbose) controller.serial_port.print(result);
    return result;
}


void Buttons::add(uint8_t pin, std::string command, ButtonInputMode type, ButtonTriggerEvent event, uint32_t debounce_interval) {
    if (is_disabled()) return;

    uint32_t next_id = 0;

    for (const auto& button : data.buttons)  next_id = std::max(next_id, button.id + 1);

    ButtonData button;

    button.id                = next_id;
    button.pin               = pin;
    button.command           = std::move(command);
    button.debounce_interval = debounce_interval;
    button.type              = static_cast<uint8_t>(type);
    button.event             = static_cast<uint8_t>(event);

    pinMode(button.pin, type == ButtonInputMode::PULL_UP ? INPUT_PULLUP : INPUT_PULLDOWN);

    button.last_steady_state  = digitalRead(button.pin);
    button.last_flicker_state = button.last_steady_state;
    button.last_debounce_time = 0;

    data.buttons.push_back(std::move(button));

    save_to_nvs();
}


void Buttons::remove(uint32_t button_id) {
    if (is_disabled()) return;

    const auto old_size = data.buttons.size();

    data.buttons.erase(
        std::remove_if(
            data.buttons.begin(),
            data.buttons.end(),
            [button_id](const ButtonData& button) {
                return button.id == button_id;
            }
        ),
        data.buttons.end()
    );

    if (data.buttons.size() != old_size) save_to_nvs();
}


void Buttons::load_from_nvs() {
    if (is_disabled()) return;

    ButtonsData loaded;

    if (controller.nvs.read_flex(id, "data", loaded)) {
        data = std::move(loaded);
    } else {
        data.buttons.clear();
    }

    for (auto& button : data.buttons) {
        const auto type = static_cast<ButtonInputMode>(button.type);

        pinMode(button.pin, type == ButtonInputMode::PULL_UP ? INPUT_PULLUP : INPUT_PULLDOWN);

        button.last_steady_state  = digitalRead(button.pin);
        button.last_flicker_state = button.last_steady_state;
        button.last_debounce_time = 0;
    }
}


void Buttons::save_to_nvs() {
    if (is_disabled()) return;

    controller.nvs.write_flex(id, "data", data);
}


void Buttons::button_add_cli(std::span<const std::string> args) {
    if (is_disabled()) return;

    ButtonInputMode type;

    if (args[2] == "pullup") {
        type = ButtonInputMode::PULL_UP;
    } else if (args[2] == "pulldown") {
        type = ButtonInputMode::PULL_DOWN;
    } else {
        controller.serial_port.print(
            "Error: input mode must be pullup or pulldown."
        );
        return;
    }

    ButtonTriggerEvent event;

    if (args[3] == "on_press") {
        event = ButtonTriggerEvent::ON_PRESS;
    } else if (args[3] == "on_release") {
        event = ButtonTriggerEvent::ON_RELEASE;
    } else if (args[3] == "on_change") {
        event = ButtonTriggerEvent::ON_CHANGE;
    } else {
        controller.serial_port.print(
            "Error: event must be on_press, on_release, or on_change."
        );
        return;
    }

    try {
        const unsigned long pin_value = std::stoul(args[0]);
        const unsigned long debounce  = std::stoul(args[4]);

        if (pin_value > std::numeric_limits<uint8_t>::max()) {
            controller.serial_port.print("Error: invalid pin.");
            return;
        }

        if (debounce > std::numeric_limits<uint32_t>::max()) {
            controller.serial_port.print("Error: invalid debounce value.");
            return;
        }

        add(
            static_cast<uint8_t>(pin_value),
            args[1],
            type,
            event,
            static_cast<uint32_t>(debounce)
        );

        controller.serial_port.print(
            "Successfully added button mapping."
        );
    } catch (...) {
        controller.serial_port.print(
            "Error: invalid pin or debounce value."
        );
    }
}


void Buttons::button_remove_cli(std::span<const std::string> args) {
    if (is_disabled()) return;

    try {
        const unsigned long value = std::stoul(args[0]);

        if (value > std::numeric_limits<uint32_t>::max()) {
            controller.serial_port.print("Error: invalid button ID.");
            return;
        }

        const uint32_t button_id = static_cast<uint32_t>(value);

        const bool exists = std::any_of(
            data.buttons.begin(),
            data.buttons.end(),
            [button_id](const ButtonData& button) {
                return button.id == button_id;
            }
        );

        if (!exists) {
            controller.serial_port.print(
                "Error: button ID not found."
            );
            return;
        }

        remove(button_id);

        controller.serial_port.print(
            "Successfully removed button mapping."
        );
    } catch (...) {
        controller.serial_port.print(
            "Error: invalid button ID."
        );
    }
}