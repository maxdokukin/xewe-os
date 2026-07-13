/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Nvs/Nvs.cpp

#include "Nvs.h"
#include "../../Module/ModuleController.h"

#include <vector>
#include <esp_idf_version.h>




Nvs::Nvs(ModuleController& controller)
      : Module(controller,
               /* id                  */ "nvs",
               /* name                */ "Nvs",
               /* description         */ "Stores user settings even when the power is off",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}


bool Nvs::ensure_ready() {
    if (m_nvs_ready) {
        return true;
    }

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        DBG_PRINTF(Nvs,
                   "ensure_ready(): nvs_flash_init() returned %s (%d). Erasing default NVS partition.\n",
                   esp_err_to_name(err),
                   static_cast<int>(err));

        (void)nvs_flash_deinit();

        const esp_err_t erase_err = nvs_flash_erase();

        if (erase_err != ESP_OK) {
            DBG_PRINTF(Nvs,
                       "ensure_ready(): nvs_flash_erase() failed: %s (%d).\n",
                       esp_err_to_name(erase_err),
                       static_cast<int>(erase_err));
            return false;
        }

        err = nvs_flash_init();
    }

    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        m_nvs_ready = true;
        DBG_PRINTLN(Nvs, "ensure_ready(): NVS ready.");
        return true;
    }

    DBG_PRINTF(Nvs,
               "ensure_ready(): nvs_flash_init() failed: %s (%d).\n",
               esp_err_to_name(err),
               static_cast<int>(err));

    return false;
}


esp_err_t Nvs::open_handle(nvs_open_mode_t mode, ScopedHandle& scoped) {
    scoped.close();

    if (!ensure_ready()) {
        return ESP_FAIL;
    }

    const esp_err_t err = nvs_open(id.c_str(), mode, &scoped.handle);

    if (err != ESP_OK && !(mode == NVS_READONLY && err == ESP_ERR_NVS_NOT_FOUND)) {
        DBG_PRINTF(Nvs,
                   "open_handle(): nvs_open(namespace='%s', mode=%d) failed: %s (%d).\n",
                   id.c_str(),
                   static_cast<int>(mode),
                   esp_err_to_name(err),
                   static_cast<int>(err));
    }

    return err;
}


bool Nvs::commit_and_close(ScopedHandle& scoped,
                           esp_err_t op_err,
                           const char* op_name,
                           const std::string& storage_key) {
    if (op_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "%s: operation failed for key '%s': %s (%d).\n",
                   op_name,
                   storage_key.c_str(),
                   esp_err_to_name(op_err),
                   static_cast<int>(op_err));

        scoped.close();
        return false;
    }

    const esp_err_t commit_err = nvs_commit(scoped);
    scoped.close();

    if (commit_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "%s: commit failed for key '%s': %s (%d).\n",
                   op_name,
                   storage_key.c_str(),
                   esp_err_to_name(commit_err),
                   static_cast<int>(commit_err));
        return false;
    }

    DBG_PRINTF(Nvs,
               "%s: persisted key '%s'.\n",
               op_name,
               storage_key.c_str());

    return true;
}


std::string Nvs::format_key(std::string_view ns, std::string_view key) const {
    if (ns.empty() && key.empty()) {
        return {};
    }

    std::string combined;

    if (!ns.empty()) {
        combined.append(ns.data(), ns.size());
        combined.push_back(':');
    }

    combined.append(key.data(), key.size());

    for (char& c : combined) {
        if (c == '\0') {
            c = '_';
        }
    }

    if (combined.size() > MAX_KEY_LEN) {
        DBG_PRINTF(Nvs,
                   "format_key(): WARNING: key '%s' is too long (%u chars), truncating to %u.\n",
                   combined.c_str(),
                   static_cast<unsigned>(combined.size()),
                   static_cast<unsigned>(MAX_KEY_LEN));

        combined.resize(MAX_KEY_LEN);
    }

    return combined;
}


void Nvs::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    DBG_PRINTF(Nvs,
               "reset(): clearing all keys in NVS namespace '%s'.\n",
               id.c_str());

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(NVS_READWRITE, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "reset(): failed to open namespace '%s': %s (%d).\n",
                   id.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return;
    }

    const esp_err_t erase_err = nvs_erase_all(sh);

    if (!commit_and_close(sh, erase_err, "reset()", std::string(id.c_str()))) {
        DBG_PRINTLN(Nvs, "reset(): failed.");
        return;
    }

    DBG_PRINTLN(Nvs, "reset(): namespace cleared.");

    Module::reset(verbose, do_restart, keep_enabled);
}


void Nvs::remove(std::string_view ns, std::string_view key) {
    if (key.empty()) {
        DBG_PRINTLN(Nvs, "remove(): ERROR: empty key.");
        return;
    }

    const std::string storage_key = format_key(ns, key);

    DBG_PRINTF(Nvs,
               "remove(): ns='%.*s', key='%.*s', storage_key='%s'.\n",
               static_cast<int>(ns.size()), ns.data(),
               static_cast<int>(key.size()), key.data(),
               storage_key.c_str());

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(NVS_READWRITE, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "remove(): open failed for key '%s': %s (%d).\n",
                   storage_key.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return;
    }

    const esp_err_t erase_err = nvs_erase_key(sh, storage_key.c_str());

    if (erase_err == ESP_ERR_NVS_NOT_FOUND) {
        DBG_PRINTF(Nvs,
                   "remove(): key '%s' does not exist.\n",
                   storage_key.c_str());
        return;
    }

    (void)commit_and_close(sh, erase_err, "remove()", storage_key);
}


void Nvs::reset_ns(std::string_view ns) {
    const std::string prefix = format_key(ns);

    if (prefix.empty()) {
        DBG_PRINTLN(Nvs, "reset_ns(): ERROR: empty namespace.");
        return;
    }

    DBG_PRINTF(Nvs,
               "reset_ns(): removing keys with logical namespace '%.*s', prefix '%s'.\n",
               static_cast<int>(ns.size()), ns.data(),
               prefix.c_str());

    if (!ensure_ready()) {
        DBG_PRINTLN(Nvs, "reset_ns(): NVS is not ready.");
        return;
    }

    std::vector<std::string> keys_to_remove;

    nvs_iterator_t it = nullptr;
    esp_err_t iter_err = nvs_entry_find("nvs", id.c_str(), NVS_TYPE_ANY, &it);

    while (iter_err == ESP_OK && it != nullptr) {
        nvs_entry_info_t info{};
        (void)nvs_entry_info(it, &info);

        const std::string current_key(info.key);

        if (current_key.rfind(prefix, 0) == 0) {
            keys_to_remove.push_back(current_key);

            DBG_PRINTF(Nvs,
                       "reset_ns(): matched key '%s'.\n",
                       current_key.c_str());
        }

        iter_err = nvs_entry_next(&it);
    }

    if (it != nullptr) {
        nvs_release_iterator(it);
    }

    if (keys_to_remove.empty()) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): no keys found for namespace '%.*s'.\n",
                   static_cast<int>(ns.size()), ns.data());
        return;
    }

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(NVS_READWRITE, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): open failed: %s (%d).\n",
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return;
    }

    std::size_t removed = 0;

    for (const std::string& storage_key : keys_to_remove) {
        const esp_err_t erase_err = nvs_erase_key(sh, storage_key.c_str());

        if (erase_err == ESP_OK) {
            ++removed;
        } else {
            DBG_PRINTF(Nvs,
                       "reset_ns(): failed to erase key '%s': %s (%d).\n",
                       storage_key.c_str(),
                       esp_err_to_name(erase_err),
                       static_cast<int>(erase_err));
        }
    }

    const esp_err_t commit_err = nvs_commit(sh);
    sh.close();

    if (commit_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): commit failed: %s (%d).\n",
                   esp_err_to_name(commit_err),
                   static_cast<int>(commit_err));
        return;
    }

    DBG_PRINTF(Nvs,
               "reset_ns(): removed %u keys for namespace '%.*s'.\n",
               static_cast<unsigned>(removed),
               static_cast<int>(ns.size()), ns.data());
}
