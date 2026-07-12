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
#include "../../../SystemController/SystemController.h"


Pins::Pins(SystemController& controller)
      : Module(controller,
               /* module_name         */ "Pins",
               /* module_description  */ "Allows direct hardware control (GPIO, ADC, I2C, PWM)",
               /* nvs_key             */ "pns",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
{
    commands_storage.push_back({
        "gpio_read",
        "Read digital logic level. Returns: 0 (GND) or 1 (VCC). Configures pin as INPUT.",
        "$" + lower(module_name) + " gpio_read <pin>",
        1,
        [this](string_view args) {
            int pin = atoi(string(args).c_str());
            pinMode(pin, INPUT);
            this->controller.serial_port.print(to_string((int)digitalRead(pin)).c_str(), kCRLF);
        }
    });

    commands_storage.push_back({
        "gpio_write",
        "Force pin to logic HIGH (1) or LOW (0). Configures pin as OUTPUT.",
        "$" + lower(module_name) + " gpio_write <pin> <0|1>",
        2,
        [this](string_view args) {
            string s(args);
            auto sp = s.find(' ');
            if (sp == string::npos) {
                this->controller.serial_port.print("Error: Missing <pin> or <level>", kCRLF);
                return;
            }
            int pin = atoi(s.substr(0, sp).c_str());
            int lvl = atoi(s.substr(sp + 1).c_str());
            pinMode(pin, OUTPUT);
            digitalWrite(pin, lvl ? HIGH : LOW);
            this->controller.serial_port.print("ok", kCRLF);
        }
    });

    commands_storage.push_back({
        "gpio_toggle",
        "Inverts the current state of a pin (HIGH -> LOW or LOW -> HIGH). Forces OUTPUT mode.",
        "$" + lower(module_name) + " gpio_toggle <pin>",
        1,
        [this](string_view args) {
            int pin = atoi(string(args).c_str());
            pinMode(pin, OUTPUT);
            int newState = !digitalRead(pin);
            digitalWrite(pin, newState);
            this->controller.serial_port.print(to_string(newState).c_str(), kCRLF);
        }
    });

    commands_storage.push_back({
        "gpio_mode",
        "Set IO mode/resistors. Modes: 'in' (floating), 'out' (push-pull), 'in_pullup' (weak VCC), 'in_pulldown' (weak GND).",
        "$" + lower(module_name) + " gpio_mode <pin> <in|out|in_pullup|in_pulldown>",
        2,
        [this](string_view args) {
            string s(args);
            auto sp = s.find(' ');
            if (sp == string::npos) {
                this->controller.serial_port.print("Error: Missing <pin> or <mode>", kCRLF);
                return;
            }
            int pin = atoi(s.substr(0, sp).c_str());
            string m = s.substr(sp + 1);

            if (m == "out") pinMode(pin, OUTPUT);
            else if (m == "in") pinMode(pin, INPUT);
#ifdef INPUT_PULLDOWN
            else if (m == "in_pulldown") pinMode(pin, INPUT_PULLDOWN);
#endif
            else if (m == "in_pullup") pinMode(pin, INPUT_PULLUP);
            else {
                this->controller.serial_port.print("Valid modes: in | in_pullup | in_pulldown | out", kCRLF);
                return;
            }
            this->controller.serial_port.print("ok", kCRLF);
        }
    });

    commands_storage.push_back({
        "adc_read",
        "Read analog voltage. Returns raw integer (usually 0-4095 for 12-bit).",
        "$" + lower(module_name) + " adc_read <pin>",
        1,
        [this](string_view args) {
            int pin = atoi(string(args).c_str());
            int v = analogRead(pin);
            this->controller.serial_port.print(to_string(v).c_str(), kCRLF);
        }
    });

    commands_storage.push_back({
        "pwm_setup",
        "Attach PWM timer. Freq range: 1Hz-40MHz. Bits: 1-16. (ESP32 Core v3+ uses Pins directly).",
        "$" + lower(module_name) + " pwm_setup <pin> <freq_hz> <res_bits>",
        3,
        [this](string_view args) {
            istringstream is{string(args)};
            int pin;
            double freq;
            int bits;
            // Removed 'ch' from parsing
            if (!(is >> pin >> freq >> bits)) {
                this->controller.serial_port.print("Error: Required <PIN> <FREQ> <BITS>", kCRLF);
                return;
            }

            // Core v3: ledcAttach(pin, freq, resolution)
            if (!ledcAttach(static_cast<uint8_t>(pin), static_cast<uint32_t>(freq), static_cast<uint8_t>(bits))) {
                this->controller.serial_port.print("PWM attachment failed", kCRLF);
                return;
            }
            this->controller.serial_port.print("ok", kCRLF);
        }
    });

    commands_storage.push_back({
        "pwm_write",
        "Set PWM duty cycle on a specific pin. Max value = (2^res_bits) - 1.",
        "$" + lower(module_name) + " pwm_write <pin> <duty_value>",
        2,
        [this](string_view args) {
            istringstream is{string(args)};
            int pin;
            int duty;
            // Changed 'ch' to 'pin'
            if (!(is >> pin >> duty)) {
                this->controller.serial_port.print("Error: Required <PIN> <DUTY>", kCRLF);
                return;
            }
            // Core v3: ledcWrite(pin, duty)
            ledcWrite(static_cast<uint8_t>(pin), static_cast<uint32_t>(duty));
            this->controller.serial_port.print("ok", kCRLF);
        }
    });

    commands_storage.push_back({
        "pwm_stop",
        "Stop PWM on a pin (sets duty 0). Optional argument '1' completely detaches hardware.",
        "$" + lower(module_name) + " pwm_stop <pin> [detach:0|1]",
        2,
        [this](string_view args) {
            istringstream is{string(args)};
            int pin;
            int should_detach = 0;

            if (!(is >> pin)) {
                this->controller.serial_port.print("Error: Required <PIN>", kCRLF);
                return;
            }
            // Check for optional detach flag
            is >> should_detach;

            ledcWrite(static_cast<uint8_t>(pin), 0);

            if (should_detach) {
                ledcDetach(static_cast<uint8_t>(pin));
            }
            this->controller.serial_port.print("ok", kCRLF);
        }
    });

    commands_storage.push_back({
        "i2c_scan",
        "Initializes I2C on specific SDA/SCL pins and scans for devices (0x01 - 0x77).",
        "$" + lower(module_name) + " i2c_scan <sda_pin> <scl_pin>",
        2,
        [this](string_view args) {
            istringstream is{string(args)};
            int sda, scl;
            if (!(is >> sda >> scl)) {
                this->controller.serial_port.print("Error: Required <SDA> <SCL>", kCRLF);
                return;
            }

            Wire.begin(sda, scl);
            int found = 0;
            for (uint8_t addr = 1; addr < 0x78; ++addr) {
                Wire.beginTransmission(addr);
                uint8_t err = Wire.endTransmission();
                if (err == 0) {
                    char ln[8];
                    snprintf(ln, sizeof(ln), "0x%02X", addr);
                    this->controller.serial_port.print(ln, kCRLF);
                    found++;
                }
            }
            if (found == 0) this->controller.serial_port.print("No I2C devices found", kCRLF);
        }
    });
}
