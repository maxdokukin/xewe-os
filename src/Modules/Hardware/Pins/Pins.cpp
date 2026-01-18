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
      "gpio-read", "Reads the digital logic level of a specific pin (0 or 1).",
      "$"+ lower(module_name) + " gpio-read <pin>", 1,
      [this](string_view args){
        int pin = atoi(std::string(args).c_str());
        pinMode(pin, INPUT);
        this->controller.serial_port.print(std::to_string((int)digitalRead(pin)).c_str(), kCRLF);
      }
    });

    commands_storage.push_back({
      "gpio-write", "Sets a pin to HIGH (1) or LOW (0) logic level.",
      "$"+ lower(module_name) + " gpio-write <pin> <0|1>", 2,
      [this](string_view args){
        std::string s(args); auto sp = s.find(' ');
        if(sp == std::string::npos){ this->controller.serial_port.print("Error: Missing <pin> or <level>", kCRLF); return; }
        int pin = atoi(s.substr(0, sp).c_str());
        int lvl = atoi(s.substr(sp + 1).c_str());
        pinMode(pin, OUTPUT);
        digitalWrite(pin, lvl ? HIGH : LOW);
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    commands_storage.push_back({
      "gpio-mode", "Configures pin direction and internal resistors (in, out, in_pullup, in_pulldown).",
      "$"+ lower(module_name) + " gpio-mode <pin> <mode>", 2,
      [this](string_view args){
        std::string s(args); auto sp = s.find(' ');
        if(sp == std::string::npos){ this->controller.serial_port.print("Error: Missing <pin> or <mode>", kCRLF); return; }
        int pin = atoi(s.substr(0, sp).c_str()); std::string m = s.substr(sp + 1);
        if(m == "out") pinMode(pin, OUTPUT);
        else if(m == "in") pinMode(pin, INPUT);
#ifdef INPUT_PULLDOWN
        else if(m == "in_pulldown") pinMode(pin, INPUT_PULLDOWN);
#endif
        else if(m == "in_pullup") pinMode(pin, INPUT_PULLUP);
        else { this->controller.serial_port.print("Valid modes: in | in_pullup | in_pulldown | out", kCRLF); return; }
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    commands_storage.push_back({
      "adc-read", "Reads raw analog voltage from a pin (e.g., 0-4095 for 12-bit ADC).",
      "$"+ lower(module_name) + " adc-read <pin>", 1,
      [this](string_view args){
        int pin = atoi(std::string(args).c_str());
        int v = analogRead(pin);
        this->controller.serial_port.print(std::to_string(v).c_str(), kCRLF);
      }
    });

    commands_storage.push_back({
      "i2c-scan", "Scans the I2C bus for active device addresses (hexadecimal).",
      "$"+ lower(module_name) + " i2c-scan", 0,
      [this](string_view){
        Wire.begin();
        int found = 0;
        for(uint8_t addr = 1; addr < 0x78; ++addr){
          Wire.beginTransmission(addr);
          uint8_t err = Wire.endTransmission();
          if(err == 0){
            char ln[8]; snprintf(ln, sizeof(ln), "0x%02X", addr);
            this->controller.serial_port.print(ln, kCRLF);
            found++;
          }
        }
        if(found == 0) this->controller.serial_port.print("No I2C devices found", kCRLF);
      }
    });

    commands_storage.push_back({
      "pwm-setup", "Initializes a PWM frequency and resolution (bits) on a specific pin.",
      "$"+ lower(module_name) + " pwm-setup <ch> <pin> <freq_hz> <res_bits>", 4,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch, pin; double freq; int bits;
        if(!(is >> ch >> pin >> freq >> bits)){ this->controller.serial_port.print("Error: Required <CH> <PIN> <FREQ> <BITS>", kCRLF); return; }
        if(!ledcAttach(static_cast<uint8_t>(pin), static_cast<uint32_t>(freq), static_cast<uint8_t>(bits))){
          this->controller.serial_port.print("PWM attachment failed", kCRLF); return;
        }
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    commands_storage.push_back({
      "pwm-write", "Sets the duty cycle for a PWM channel based on established resolution.",
      "$"+ lower(module_name) + " pwm-write <ch> <duty_value>", 2,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch; int duty;
        if(!(is >> ch >> duty)){ this->controller.serial_port.print("Error: Required <CH> <DUTY>", kCRLF); return; }
        ledcWrite(static_cast<uint8_t>(ch), static_cast<uint32_t>(duty));
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    commands_storage.push_back({
      "pwm-stop", "Disables PWM on a pin and sets the duty cycle to 0.",
      "$"+ lower(module_name) + " pwm-stop <ch> [pin]", 2,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch; int pin = -1;
        if(!(is >> ch)){ this->controller.serial_port.print("Error: Required <CH> and optional [PIN]", kCRLF); return; }
        if(is >> pin){ ledcDetach(static_cast<uint8_t>(pin)); }
        ledcWrite(static_cast<uint8_t>(ch), 0);
        this->controller.serial_port.print("ok", kCRLF);
      }
    });
}
