/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/



// src/Modules/Nvs/Nvs.h
#pragma once

#include "../../Module/Module.h"
// #include "../../../Config.h"
// #include "../../../Debug.h"

#include <Preferences.h>
#include <array>
#include <string_view>
#include <string>


struct NvsConfig : public ModuleConfig {};


class Nvs : public Module {
public:
    explicit                    Nvs                         (SystemController& controller);

    // optional implementation
    void                        reset                       (const bool verbose=false,
                                                             const bool do_restart=true)            override;

    // other methods
    void                        write_str                   (string_view ns,
                                                             string_view key,
                                                             string_view value);
    void                        write_uint8                 (string_view ns,
                                                             string_view key,
                                                             uint8_t value);
    void                        write_uint16                (string_view ns,
                                                             string_view key,
                                                             uint16_t value);
    void                        write_bool                  (string_view ns,
                                                             string_view key,
                                                             bool value);
    void                        remove                      (string_view ns,
                                                             string_view key);

    // Generic NVS Read Methods
    string                 read_str                    (string_view ns,
                                                             string_view key,
                                                             string_view default_value = "");
    uint8_t                     read_uint8                  (string_view ns,
                                                             string_view key,
                                                             uint8_t default_value = 0);
    uint16_t                    read_uint16                 (string_view ns,
                                                             string_view key,
                                                             uint16_t default_value = 0);
    bool                        read_bool                   (string_view ns,
                                                             string_view key,
                                                               bool default_value = false);

private:
    static constexpr size_t     MAX_KEY_LEN                 = 15;
    Preferences                 preferences;
    string                      full_key                    (string_view ns,
                                                             string_view key) const;
};
