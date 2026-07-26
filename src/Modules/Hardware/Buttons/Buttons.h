/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Hardware/Buttons/Buttons.h
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "../../Core/Nvs/FlexData.h"
#include "../../Module/Module.h"


struct ButtonsConfig : public ModuleConfig {};


enum class ButtonInputMode : uint8_t {
    PULLUP   = 0,
    PULLDOWN = 1
};

enum class ButtonTriggerEvent : uint8_t {
    ON_PRESS   = 0,
    ON_RELEASE = 1,
    ON_CHANGE  = 2
};

struct ButtonData : FlexData<ButtonData> {
    uint32_t    id                   = 0;
    uint8_t     pin                  = 0;
    std::string command;
    uint32_t    debounce_interval    = 50;
    uint8_t     type                 = static_cast<uint8_t>(ButtonInputMode::PULLUP);
    uint8_t     event                = static_cast<uint8_t>(ButtonTriggerEvent::ON_PRESS);

    uint32_t    last_debounce_time   = 0;
    int         last_steady_state    = 0;
    int         last_flicker_state   = 0;

    static constexpr auto fields() {
        return std::make_tuple(
            fld("id",                &ButtonData::b_id),
            fld("pin",               &ButtonData::pin),
            fld("command",           &ButtonData::command),
            fld("debounce_interval", &ButtonData::debounce_interval),
            fld("type",              &ButtonData::type),
            fld("event",             &ButtonData::event)
        );
    }
};


struct ButtonsData : FlexData<ButtonsData> {
    std::vector<ButtonData> buttons;

    static constexpr auto fields() {
        return std::make_tuple(
            fld("buttons", &ButtonsData::buttons)
        );
    }
};


class Buttons : public Module {
public:
    explicit                    Buttons                     (ModuleController& controller);

    void                        begin_routines_regular      (const ModuleConfig& cfg)       override;
    void                        loop                        ()                              override;

    void                        reset                       (const bool verbose=false,
                                                             const bool do_restart=true,
                                                             const bool keep_enabled=true) override;

    std::string                 status                      (const bool verbose=false) const override;

    void                        add                         (uint8_t pin,
                                                             std::string command,
                                                             ButtonInputMode type,
                                                             ButtonTriggerEvent event,
                                                             uint32_t debounce_interval);
    void                        remove                      (button_id) // removes from ram and flushes changes to nvs

    void                        load_from_nvs               (loads all buttons in the ram) // only happens on the system startup
    void                        save_to_nvs                 (save all buttons in ram) // only happens when there is a new button added/removed


private:
    void                        button_add_cli              (std::span<const std::string> args);
    void                        button_remove_cli           (std::span<const std::string> args);

    ButtonsData                 data;
};
