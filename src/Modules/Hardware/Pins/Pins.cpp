/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Hardware/Pins/Pins.cpp


#include "Pins.h"
#include "../../Module/ModuleController.h"


Pins::Pins(ModuleController& controller)
      : Module(controller,
               /* id                  */ "pins",
               /* name                */ "Pins",
               /* description         */ "Allows direct hardware control (GPIO, ADC, I2C, PWM)",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
{
    commands_storage.push_back(Command{
        "gpio_read",
        "Read digital logic level. Returns: 0 (GND) or 1 (VCC). Configures pin as INPUT.",
        std::string("$") + id + " gpio_read 9",
        1,
        [this](std::span<const std::string> args){ gpio_read_cli(args); }
    });

    commands_storage.push_back(Command{
        "gpio_write",
        "Force pin to logic HIGH (1) or LOW (0). Configures pin as OUTPUT.",
        std::string("$") + id + " gpio_write 9 1",
        2,
        [this](std::span<const std::string> args){ gpio_write_cli(args); }
    });

    commands_storage.push_back(Command{
        "gpio_toggle",
        "Inverts the current state of a pin (HIGH -> LOW or LOW -> HIGH). Forces OUTPUT mode.",
        std::string("$") + id + " gpio_toggle 9",
        1,
        [this](std::span<const std::string> args){ gpio_toggle_cli(args); }
    });

    commands_storage.push_back(Command{
        "gpio_mode",
        "Set IO mode/resistors. Modes: 'in' (floating), 'out' (push-pull), 'in_pullup' (weak VCC), 'in_pulldown' (weak GND).",
        std::string("$") + id + " gpio_mode 9 in_pullup",
        2,
        [this](std::span<const std::string> args){ gpio_mode_cli(args); }
    });

    commands_storage.push_back(Command{
        "adc_read",
        "Read analog voltage. Returns raw integer (usually 0-4095 for 12-bit).",
        std::string("$") + id + " adc_read 4",
        1,
        [this](std::span<const std::string> args){ adc_read_cli(args); }
    });

    commands_storage.push_back(Command{
        "pwm_setup",
        "Attach PWM timer. Freq range: 1Hz-40MHz. Bits: 1-16. (ESP32 Core v3+ uses Pins directly).",
        std::string("$") + id + " pwm_setup 9 5000 8",
        3,
        [this](std::span<const std::string> args){ pwm_setup_cli(args); }
    });

    commands_storage.push_back(Command{
        "pwm_write",
        "Set PWM duty cycle on a specific pin. Max value = (2^res_bits) - 1.",
        std::string("$") + id + " pwm_write 9 128",
        2,
        [this](std::span<const std::string> args){ pwm_write_cli(args); }
    });

    commands_storage.push_back(Command{
        "pwm_stop",
        "Stop PWM on a pin (sets duty 0) and detaches the hardware timer.",
        std::string("$") + id + " pwm_stop 9",
        1,
        [this](std::span<const std::string> args){ pwm_stop_cli(args); }
    });

    commands_storage.push_back(Command{
        "i2c_scan",
        "Initializes I2C on specific SDA/SCL pins and scans for devices (0x01 - 0x77).",
        std::string("$") + id + " i2c_scan 21 22",
        2,
        [this](std::span<const std::string> args){ i2c_scan_cli(args); }
    });
}

void Pins::gpio_read_cli(std::span<const std::string> args) {
    int pin;
    if (!xewe::str::parse_int(args[0], pin)) {
        controller.serial_port.print("Error: invalid <pin>");
        return;
    }
    pinMode(pin, INPUT);
    controller.serial_port.print(std::to_string(static_cast<int>(digitalRead(pin))));
}

void Pins::gpio_write_cli(std::span<const std::string> args) {
    int pin, lvl;
    if (!xewe::str::parse_int(args[0], pin) || !xewe::str::parse_int(args[1], lvl)) {
        controller.serial_port.print("Error: invalid <pin> or <level>");
        return;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, lvl ? HIGH : LOW);
    controller.serial_port.print("ok");
}

void Pins::gpio_toggle_cli(std::span<const std::string> args) {
    int pin;
    if (!xewe::str::parse_int(args[0], pin)) {
        controller.serial_port.print("Error: invalid <pin>");
        return;
    }
    pinMode(pin, OUTPUT);
    int new_state = !digitalRead(pin);
    digitalWrite(pin, new_state);
    controller.serial_port.print(std::to_string(new_state));
}

void Pins::gpio_mode_cli(std::span<const std::string> args) {
    int pin;
    if (!xewe::str::parse_int(args[0], pin)) {
        controller.serial_port.print("Error: invalid <pin>");
        return;
    }
    const std::string& m = args[1];

    if      (m == "out") pinMode(pin, OUTPUT);
    else if (m == "in")  pinMode(pin, INPUT);
#ifdef INPUT_PULLDOWN
    else if (m == "in_pulldown") pinMode(pin, INPUT_PULLDOWN);
#endif
    else if (m == "in_pullup")   pinMode(pin, INPUT_PULLUP);
    else {
        controller.serial_port.print("Valid modes: in | in_pullup | in_pulldown | out");
        return;
    }
    controller.serial_port.print("ok");
}

void Pins::adc_read_cli(std::span<const std::string> args) {
    int pin;
    if (!xewe::str::parse_int(args[0], pin)) {
        controller.serial_port.print("Error: invalid <pin>");
        return;
    }
    controller.serial_port.print(std::to_string(analogRead(pin)));
}

void Pins::pwm_setup_cli(std::span<const std::string> args) {
    uint8_t pin, bits;
    uint32_t freq;
    if (!xewe::str::parse_int(args[0], pin) ||
        !xewe::str::parse_int(args[1], freq) ||
        !xewe::str::parse_int(args[2], bits)) {
        controller.serial_port.print("Error: required <pin> <freq_hz> <res_bits>");
        return;
    }

    // Core v3: ledcAttach(pin, freq, resolution)
    if (!ledcAttach(pin, freq, bits)) {
        controller.serial_port.print("PWM attachment failed");
        return;
    }
    controller.serial_port.print("ok");
}

void Pins::pwm_write_cli(std::span<const std::string> args) {
    uint8_t pin;
    uint32_t duty;
    if (!xewe::str::parse_int(args[0], pin) || !xewe::str::parse_int(args[1], duty)) {
        controller.serial_port.print("Error: required <pin> <duty_value>");
        return;
    }
    // Core v3: ledcWrite(pin, duty)
    ledcWrite(pin, duty);
    controller.serial_port.print("ok");
}

void Pins::pwm_stop_cli(std::span<const std::string> args) {
    uint8_t pin;
    if (!xewe::str::parse_int(args[0], pin)) {
        controller.serial_port.print("Error: required <pin>");
        return;
    }
    ledcWrite(pin, 0);
    ledcDetach(pin);
    controller.serial_port.print("ok");
}

void Pins::i2c_scan_cli(std::span<const std::string> args) {
    int sda, scl;
    if (!xewe::str::parse_int(args[0], sda) || !xewe::str::parse_int(args[1], scl)) {
        controller.serial_port.print("Error: required <sda_pin> <scl_pin>");
        return;
    }

    Wire.begin(sda, scl);
    int found = 0;
    for (uint8_t addr = 1; addr < 0x78; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            char ln[8];
            snprintf(ln, sizeof(ln), "0x%02X", addr);
            controller.serial_port.print(ln);
            found++;
        }
    }
    if (found == 0) controller.serial_port.print("No I2C devices found");
}
