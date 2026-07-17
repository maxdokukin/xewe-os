/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Core/Nvs/Nvs.h
#pragma once

#include "../../Module/Module.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <Arduino.h>

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

    // Wipe the ENTIRE default NVS partition (every namespace). Use for a true
    // system-wide factory reset; Nvs::reset() only clears this module's namespace.
    bool                        factory_reset               ();

    template <typename T>
    bool                        write                       (std::string_view ns,
                                                             std::string_view key,
                                                             const T& value);

    template <typename T>
    T                           read                        (std::string_view ns,
                                                             std::string_view key,
                                                             T default_value = T());

    // Variable-length binary blob (one NVS record). Enables persisting an entire
    // object as a single key instead of sharding it across many keys.
    bool                        write_blob                  (std::string_view ns,
                                                             std::string_view key,
                                                             const std::vector<uint8_t>& data);

    // Returns empty on a missing key or any read error (callers treat empty as
    // "not found").
    std::vector<uint8_t>        read_blob                   (std::string_view ns,
                                                             std::string_view key);

    // Typed object persistence: T must derive from FlexData<T>. save serializes
    // the object to its blob and stores it as one record; load reads the record
    // back. load returns false (leaving out untouched) on a missing key or a
    // corrupt/version-mismatched blob.
    template <typename T>
    bool                        save                        (std::string_view ns,
                                                             std::string_view key,
                                                             const T& obj);

    template <typename T>
    bool                        load                        (std::string_view ns,
                                                             std::string_view key,
                                                             T& out);

    void                        remove                      (std::string_view ns,
                                                             std::string_view key);

    void                        reset_ns                    (std::string_view ns);

private:
    template <typename>
    struct always_false : std::false_type {};

    static constexpr std::size_t MAX_KEY_LEN = 15;

    struct ScopedHandle {
        nvs_handle_t handle = 0;
        ~ScopedHandle() { close(); }
        operator nvs_handle_t() const { return handle; }
        void close() {
            if (handle != 0) {
                nvs_close(handle);
                handle = 0;
            }
        }
    };

    bool                        m_nvs_ready = false;

    bool                        ensure_ready                ();

    esp_err_t                   open_handle                 (std::string_view ns,
                                                             nvs_open_mode_t mode,
                                                             ScopedHandle& scoped);

    bool                        commit_and_close            (ScopedHandle& scoped,
                                                             esp_err_t op_err,
                                                             const char* op_name,
                                                             const std::string& storage_key);

    // Normalizes a namespace or key name to NVS rules: NUL-terminated std::string
    // with embedded NULs replaced with '_'. Names longer than MAX_KEY_LEN are
    // REJECTED (returns empty) rather than truncated, since truncation silently
    // collides distinct names sharing the first MAX_KEY_LEN chars.
    std::string                 sanitize_name               (std::string_view name) const;
};

#include "Nvs.tpp"