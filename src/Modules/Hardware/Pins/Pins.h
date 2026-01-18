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

#include <Arduino.h>
#include <Wire.h>
#include <sstream>

struct PinsConfig : public ModuleConfig {};


class Pins : public Module {
public:
    explicit                    Pins                        (SystemController& controller);

    void                        begin_routines_required     (const ModuleConfig& cfg)       override;
    void                        begin_routines_init         (const ModuleConfig& cfg)       override;
    void                        begin_routines_regular      (const ModuleConfig& cfg)       override;
    void                        begin_routines_common       (const ModuleConfig& cfg)       override;

    void                        loop                        ()                              override;

    void                        enable                      (const bool verbose=false,
                                                             const bool do_restart=true)    override;
    void                        disable                     (const bool verbose=false,
                                                             const bool do_restart=true)    override;
    void                        reset                       (const bool verbose=false,
                                                             const bool do_restart=true)    override;

    string                      status                      (const bool verbose=false)      const override;

private:

};