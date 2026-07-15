/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
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
