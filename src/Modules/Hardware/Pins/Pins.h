// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// src/Modules/Hardware/Pins/Pins.h
#pragma once

#include "../../Module/Module.h"

#include <Wire.h>

struct PinsConfig : public ModuleConfig {};


class Pins : public Module {
public:
    explicit                    Pins                        (ModuleController& controller);

private:
    void                        gpio_read_cli               (std::span<const std::string> args);
    void                        gpio_write_cli              (std::span<const std::string> args);
    void                        gpio_toggle_cli             (std::span<const std::string> args);
    void                        gpio_mode_cli               (std::span<const std::string> args);
    void                        adc_read_cli                (std::span<const std::string> args);
    void                        pwm_setup_cli               (std::span<const std::string> args);
    void                        pwm_write_cli               (std::span<const std::string> args);
    void                        pwm_stop_cli                (std::span<const std::string> args);
    void                        i2c_scan_cli                (std::span<const std::string> args);
};
