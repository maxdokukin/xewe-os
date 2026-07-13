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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>


struct NvsConfig : public ModuleConfig {};


class Nvs : public Module {
public:
    explicit                    Nvs                         (ModuleController& controller);

    void                        reset                       (const bool verbose=false,
                                                             const bool do_restart=true,
                                                             const bool keep_enabled=true) override;

    template <typename T>
    bool                        write                       (std::string_view ns,
                                                             std::string_view key,
                                                             const T& value);

    template <typename T>
    T                           read                        (std::string_view ns,
                                                             std::string_view key,
                                                             T default_value = T());

    void                        remove                      (std::string_view ns,
                                                             std::string_view key);

    void                        reset_ns                    (std::string_view ns);

    bool                        init_setup_complete         ();
    void                        set_init_setup_complete     ();

private:
    template <typename>
    struct always_false : std::false_type {};

    static constexpr std::size_t MAX_KEY_LEN = 15;

    bool                        m_nvs_ready = false;

    bool                        ensure_ready                ();

    esp_err_t                   open_handle                 (nvs_open_mode_t mode,
                                                             nvs_handle_t& handle);

    bool                        commit_and_close            (nvs_handle_t handle,
                                                             esp_err_t op_err,
                                                             const char* op_name,
                                                             const std::string& storage_key);

    std::string                 full_key                    (std::string_view ns,
                                                             std::string_view key) const;

    std::string                 namespace_prefix            (std::string_view ns) const;
};

#include "Nvs.tpp"
