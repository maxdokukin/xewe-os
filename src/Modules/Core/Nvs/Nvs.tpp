/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Nvs/Nvs.tpp
#pragma once


template <typename T>
bool Nvs::write(std::string_view ns,
                std::string_view key,
                const T& value) {
    using U = typename std::decay<T>::type;

    if (key.empty()) {
        DBG_PRINTLN(Nvs, "write(): ERROR: empty key.");
        return false;
    }

    const std::string storage_key = format_key(ns, key);

    if (storage_key.empty()) {
        DBG_PRINTLN(Nvs, "write(): ERROR: generated empty storage key.");
        return false;
    }

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(NVS_READWRITE, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "write(): open failed for key '%s': %s (%d).\n",
                   storage_key.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return false;
    }

    DBG_PRINTF(Nvs,
               "write(): ns='%.*s', key='%.*s', storage_key='%s'.\n",
               static_cast<int>(ns.size()), ns.data(),
               static_cast<int>(key.size()), key.data(),
               storage_key.c_str());

    esp_err_t write_err = ESP_ERR_INVALID_ARG;

    if constexpr (std::is_same_v<U, std::string>) {
        write_err = nvs_set_str(sh, storage_key.c_str(), value.c_str());
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        write_err = nvs_set_str(sh, storage_key.c_str(), std::string(value).c_str());
    } else if constexpr (std::is_convertible_v<U, const char*>) {
        write_err = nvs_set_str(sh, storage_key.c_str(), value ? static_cast<const char*>(value) : "");
    } else if constexpr (std::is_same_v<U, bool>) {
        write_err = nvs_set_u8(sh, storage_key.c_str(), value ? 1u : 0u);
    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1)      write_err = nvs_set_i8(sh, storage_key.c_str(), static_cast<int8_t>(value));
        else if constexpr (sizeof(U) == 2) write_err = nvs_set_i16(sh, storage_key.c_str(), static_cast<int16_t>(value));
        else if constexpr (sizeof(U) == 4) write_err = nvs_set_i32(sh, storage_key.c_str(), static_cast<int32_t>(value));
        else if constexpr (sizeof(U) == 8) write_err = nvs_set_i64(sh, storage_key.c_str(), static_cast<int64_t>(value));
    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
        if constexpr (sizeof(U) == 1)      write_err = nvs_set_u8(sh, storage_key.c_str(), static_cast<uint8_t>(value));
        else if constexpr (sizeof(U) == 2) write_err = nvs_set_u16(sh, storage_key.c_str(), static_cast<uint16_t>(value));
        else if constexpr (sizeof(U) == 4) write_err = nvs_set_u32(sh, storage_key.c_str(), static_cast<uint32_t>(value));
        else if constexpr (sizeof(U) == 8) write_err = nvs_set_u64(sh, storage_key.c_str(), static_cast<uint64_t>(value));
    } else if constexpr (std::is_floating_point_v<U>) {
        write_err = nvs_set_blob(sh, storage_key.c_str(), &value, sizeof(value));
    } else {
        static_assert(always_false<U>::value, "Unsupported Nvs::write<T>() type.");
    }

    return commit_and_close(sh, write_err, "write()", storage_key);
}


template <typename T>
T Nvs::read(std::string_view ns,
            std::string_view key,
            T default_value) {
    using U = typename std::decay<T>::type;

    if (key.empty()) {
        DBG_PRINTLN(Nvs, "read(): ERROR: empty key. Returning default.");
        return default_value;
    }

    const std::string storage_key = format_key(ns, key);

    if (storage_key.empty()) {
        DBG_PRINTLN(Nvs, "read(): ERROR: generated empty storage key. Returning default.");
        return default_value;
    }

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(NVS_READONLY, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "read(): open failed for key '%s': %s (%d). Returning default.\n",
                   storage_key.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return default_value;
    }

    if constexpr (std::is_same_v<U, std::string>) {
        std::size_t required = 0;
        esp_err_t read_err = nvs_get_str(sh, storage_key.c_str(), nullptr, &required);

        if (read_err != ESP_OK || required == 0) {
            DBG_PRINTF(Nvs,
                       "read<std::string>(): key '%s' missing/invalid: %s (%d). Returning default.\n",
                       storage_key.c_str(),
                       esp_err_to_name(read_err),
                       static_cast<int>(read_err));
            return default_value;
        }

        std::string result(required, '\0');
        read_err = nvs_get_str(sh, storage_key.c_str(), &result[0], &required);

        if (read_err != ESP_OK) {
            DBG_PRINTF(Nvs,
                       "read<std::string>(): read failed for key '%s': %s (%d). Returning default.\n",
                       storage_key.c_str(),
                       esp_err_to_name(read_err),
                       static_cast<int>(read_err));
            return default_value;
        }

        if (!result.empty() && result.back() == '\0') {
            result.pop_back();
        }

        DBG_PRINTF(Nvs,
                   "read<std::string>(): key '%s', value='%s'.\n",
                   storage_key.c_str(),
                   result.c_str());

        return result;

    } else if constexpr (std::is_same_v<U, bool>) {
        uint8_t raw = default_value ? 1u : 0u;
        if (nvs_get_u8(sh, storage_key.c_str(), &raw) == ESP_OK) {
            return raw != 0;
        }
    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
        if constexpr (sizeof(U) == 1) {
            int8_t r = static_cast<int8_t>(default_value);
            if (nvs_get_i8(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 2) {
            int16_t r = static_cast<int16_t>(default_value);
            if (nvs_get_i16(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 4) {
            int32_t r = static_cast<int32_t>(default_value);
            if (nvs_get_i32(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 8) {
            int64_t r = static_cast<int64_t>(default_value);
            if (nvs_get_i64(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        }
    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
        if constexpr (sizeof(U) == 1) {
            uint8_t r = static_cast<uint8_t>(default_value);
            if (nvs_get_u8(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 2) {
            uint16_t r = static_cast<uint16_t>(default_value);
            if (nvs_get_u16(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 4) {
            uint32_t r = static_cast<uint32_t>(default_value);
            if (nvs_get_u32(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        } else if constexpr (sizeof(U) == 8) {
            uint64_t r = static_cast<uint64_t>(default_value);
            if (nvs_get_u64(sh, storage_key.c_str(), &r) == ESP_OK) return static_cast<T>(r);
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        U result = default_value;
        std::size_t size = sizeof(result);
        if (nvs_get_blob(sh, storage_key.c_str(), &result, &size) == ESP_OK && size == sizeof(result)) {
            return result;
        }
    } else {
        static_assert(always_false<U>::value, "Unsupported Nvs::read<T>() type.");
    }

    return default_value;
}