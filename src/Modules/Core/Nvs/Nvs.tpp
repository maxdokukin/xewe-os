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
        DBG_PRINTLN(Nvs, "write_value(): ERROR: empty key.");
        return false;
    }

    const std::string storage_key = full_key(ns, key);

    if (storage_key.empty()) {
        DBG_PRINTLN(Nvs, "write_value(): ERROR: generated empty storage key.");
        return false;
    }

    nvs_handle_t handle;
    const esp_err_t open_err = open_handle(NVS_READWRITE, handle);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "write_value(): open failed for key '%s': %s (%d).\n",
                   storage_key.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return false;
    }

    esp_err_t write_err = ESP_ERR_INVALID_ARG;

    if constexpr (std::is_same_v<U, std::string>) {
        DBG_PRINTF(Nvs,
                   "write_value<std::string>(): ns='%.*s', key='%.*s', storage_key='%s', value='%s'.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   value.c_str());

        write_err = nvs_set_str(handle, storage_key.c_str(), value.c_str());

    } else if constexpr (std::is_same_v<U, std::string_view>) {
        const std::string value_copy(value);

        DBG_PRINTF(Nvs,
                   "write_value<std::string_view>(): ns='%.*s', key='%.*s', storage_key='%s', value='%s'.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   value_copy.c_str());

        write_err = nvs_set_str(handle, storage_key.c_str(), value_copy.c_str());

    } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
        const char* value_ptr = value;
        if (value_ptr == nullptr) {
            value_ptr = "";
        }

        DBG_PRINTF(Nvs,
                   "write_value<const char*>(): ns='%.*s', key='%.*s', storage_key='%s', value='%s'.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   value_ptr);

        write_err = nvs_set_str(handle, storage_key.c_str(), value_ptr);

    } else if constexpr (std::is_same_v<U, bool>) {
        DBG_PRINTF(Nvs,
                   "write_value<bool>(): ns='%.*s', key='%.*s', storage_key='%s', value=%s.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   value ? "true" : "false");

        write_err = nvs_set_u8(handle, storage_key.c_str(), value ? 1u : 0u);

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 1) {
        DBG_PRINTF(Nvs,
                   "write_value<int8_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%d.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<int>(value));

        write_err = nvs_set_i8(handle, storage_key.c_str(), static_cast<int8_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 1) {
        DBG_PRINTF(Nvs,
                   "write_value<uint8_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%u.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<unsigned>(value));

        write_err = nvs_set_u8(handle, storage_key.c_str(), static_cast<uint8_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 2) {
        DBG_PRINTF(Nvs,
                   "write_value<int16_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%d.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<int>(value));

        write_err = nvs_set_i16(handle, storage_key.c_str(), static_cast<int16_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 2) {
        DBG_PRINTF(Nvs,
                   "write_value<uint16_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%u.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<unsigned>(value));

        write_err = nvs_set_u16(handle, storage_key.c_str(), static_cast<uint16_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 4) {
        DBG_PRINTF(Nvs,
                   "write_value<int32_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%ld.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<long>(value));

        write_err = nvs_set_i32(handle, storage_key.c_str(), static_cast<int32_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 4) {
        DBG_PRINTF(Nvs,
                   "write_value<uint32_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%lu.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<unsigned long>(value));

        write_err = nvs_set_u32(handle, storage_key.c_str(), static_cast<uint32_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 8) {
        DBG_PRINTF(Nvs,
                   "write_value<int64_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%lld.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<long long>(value));

        write_err = nvs_set_i64(handle, storage_key.c_str(), static_cast<int64_t>(value));

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 8) {
        DBG_PRINTF(Nvs,
                   "write_value<uint64_t>(): ns='%.*s', key='%.*s', storage_key='%s', value=%llu.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<unsigned long long>(value));

        write_err = nvs_set_u64(handle, storage_key.c_str(), static_cast<uint64_t>(value));

    } else if constexpr (std::is_same_v<U, float>) {
        DBG_PRINTF(Nvs,
                   "write_value<float>(): ns='%.*s', key='%.*s', storage_key='%s', value=%f.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   static_cast<double>(value));

        write_err = nvs_set_blob(handle, storage_key.c_str(), &value, sizeof(value));

    } else if constexpr (std::is_same_v<U, double>) {
        DBG_PRINTF(Nvs,
                   "write_value<double>(): ns='%.*s', key='%.*s', storage_key='%s', value=%f.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   value);

        write_err = nvs_set_blob(handle, storage_key.c_str(), &value, sizeof(value));

    } else {
        nvs_close(handle);
        static_assert(always_false<U>::value, "Unsupported Nvs::write_value<T>() type.");
    }

    return commit_and_close(handle, write_err, "write_value()", storage_key);
}


template <typename T>
T Nvs::read(std::string_view ns,
            std::string_view key,
            T default_value) {
    using U = typename std::decay<T>::type;

    if (key.empty()) {
        DBG_PRINTLN(Nvs, "read_value(): ERROR: empty key. Returning default.");
        return default_value;
    }

    const std::string storage_key = full_key(ns, key);

    if (storage_key.empty()) {
        DBG_PRINTLN(Nvs, "read_value(): ERROR: generated empty storage key. Returning default.");
        return default_value;
    }

    nvs_handle_t handle;
    const esp_err_t open_err = open_handle(NVS_READONLY, handle);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "read_value(): open failed for key '%s': %s (%d). Returning default.\n",
                   storage_key.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return default_value;
    }

    if constexpr (std::is_same_v<U, std::string>) {
        std::size_t required = 0;
        esp_err_t read_err = nvs_get_str(handle, storage_key.c_str(), nullptr, &required);

        if (read_err != ESP_OK || required == 0) {
            DBG_PRINTF(Nvs,
                       "read_value<std::string>(): key '%s' missing or invalid: %s (%d). Returning default.\n",
                       storage_key.c_str(),
                       esp_err_to_name(read_err),
                       static_cast<int>(read_err));

            nvs_close(handle);
            return default_value;
        }

        std::string result(required, '\0');

        read_err = nvs_get_str(handle, storage_key.c_str(), &result[0], &required);
        nvs_close(handle);

        if (read_err != ESP_OK) {
            DBG_PRINTF(Nvs,
                       "read_value<std::string>(): read failed for key '%s': %s (%d). Returning default.\n",
                       storage_key.c_str(),
                       esp_err_to_name(read_err),
                       static_cast<int>(read_err));

            return default_value;
        }

        if (!result.empty() && result.back() == '\0') {
            result.pop_back();
        }

        DBG_PRINTF(Nvs,
                   "read_value<std::string>(): ns='%.*s', key='%.*s', storage_key='%s', value='%s'.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   result.c_str());

        return result;

    } else if constexpr (std::is_same_v<U, bool>) {
        uint8_t raw = default_value ? 1u : 0u;

        const esp_err_t read_err = nvs_get_u8(handle, storage_key.c_str(), &raw);
        nvs_close(handle);

        if (read_err != ESP_OK) {
            DBG_PRINTF(Nvs,
                       "read_value<bool>(): key '%s' missing or invalid: %s (%d). Returning default=%s.\n",
                       storage_key.c_str(),
                       esp_err_to_name(read_err),
                       static_cast<int>(read_err),
                       default_value ? "true" : "false");

            return default_value;
        }

        const bool result = raw != 0;

        DBG_PRINTF(Nvs,
                   "read_value<bool>(): ns='%.*s', key='%.*s', storage_key='%s', value=%s.\n",
                   static_cast<int>(ns.size()), ns.data(),
                   static_cast<int>(key.size()), key.data(),
                   storage_key.c_str(),
                   result ? "true" : "false");

        return result;

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 1) {
        int8_t result = static_cast<int8_t>(default_value);
        const esp_err_t read_err = nvs_get_i8(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 1) {
        uint8_t result = static_cast<uint8_t>(default_value);
        const esp_err_t read_err = nvs_get_u8(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 2) {
        int16_t result = static_cast<int16_t>(default_value);
        const esp_err_t read_err = nvs_get_i16(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 2) {
        uint16_t result = static_cast<uint16_t>(default_value);
        const esp_err_t read_err = nvs_get_u16(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 4) {
        int32_t result = static_cast<int32_t>(default_value);
        const esp_err_t read_err = nvs_get_i32(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 4) {
        uint32_t result = static_cast<uint32_t>(default_value);
        const esp_err_t read_err = nvs_get_u32(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 8) {
        int64_t result = static_cast<int64_t>(default_value);
        const esp_err_t read_err = nvs_get_i64(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && sizeof(U) == 8) {
        uint64_t result = static_cast<uint64_t>(default_value);
        const esp_err_t read_err = nvs_get_u64(handle, storage_key.c_str(), &result);
        nvs_close(handle);
        return read_err == ESP_OK ? static_cast<T>(result) : default_value;

    } else if constexpr (std::is_same_v<U, float>) {
        float result = default_value;
        std::size_t size = sizeof(result);

        const esp_err_t read_err = nvs_get_blob(handle, storage_key.c_str(), &result, &size);
        nvs_close(handle);

        return read_err == ESP_OK && size == sizeof(result) ? result : default_value;

    } else if constexpr (std::is_same_v<U, double>) {
        double result = default_value;
        std::size_t size = sizeof(result);

        const esp_err_t read_err = nvs_get_blob(handle, storage_key.c_str(), &result, &size);
        nvs_close(handle);

        return read_err == ESP_OK && size == sizeof(result) ? result : default_value;

    } else {
        nvs_close(handle);
        static_assert(always_false<U>::value, "Unsupported Nvs::read_value<T>() type.");
    }
}
