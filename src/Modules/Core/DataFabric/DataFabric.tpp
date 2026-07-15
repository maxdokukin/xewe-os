/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Core/DataFabric/DataFabric.tpp
#pragma once

#include "../Nvs/Nvs.h"


template <typename T>
bool DataFabric::save(std::string_view ns, std::string_view key, const T& obj) {
    static_assert(std::is_base_of_v<FlexData<T>, T>,
                  "DataFabric::save<T>() requires T : FlexData<T>.");
    return nvs_ref().write_blob(ns, key, obj.to_blob());
}


template <typename T>
bool DataFabric::load(std::string_view ns, std::string_view key, T& out) {
    static_assert(std::is_base_of_v<FlexData<T>, T>,
                  "DataFabric::load<T>() requires T : FlexData<T>.");
    const std::vector<uint8_t> bytes = nvs_ref().read_blob(ns, key);
    if (bytes.empty()) return false;
    return out.from_blob(bytes);
}


template <typename T>
std::string DataFabric::to_json(const T& obj) const {
    static_assert(std::is_base_of_v<FlexData<T>, T>,
                  "DataFabric::to_json<T>() requires T : FlexData<T>.");
    return obj.as_json_str();
}


template <typename T>
bool DataFabric::from_json(std::string_view json, T& out) const {
    static_assert(std::is_base_of_v<FlexData<T>, T>,
                  "DataFabric::from_json<T>() requires T : FlexData<T>.");
    out.update(json);
    return true;
}
