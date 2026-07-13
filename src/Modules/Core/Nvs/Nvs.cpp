/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Core/Nvs/Nvs.cpp

#include "Nvs.h"
#include "../../Module/ModuleController.h"


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


esp_err_t Nvs::open_handle(std::string_view ns, nvs_open_mode_t mode, ScopedHandle& scoped) {
    scoped.close();

    if (!ensure_ready()) {
        return ESP_FAIL;
    }

    const std::string namespace_name = sanitize_name(ns);

    if (namespace_name.empty()) {
        DBG_PRINTLN(Nvs, "open_handle(): ERROR: empty namespace.");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t err = nvs_open(namespace_name.c_str(), mode, &scoped.handle);

    if (err != ESP_OK && !(mode == NVS_READONLY && err == ESP_ERR_NVS_NOT_FOUND)) {
        DBG_PRINTF(Nvs,
                   "open_handle(): nvs_open(namespace='%s', mode=%d) failed: %s (%d).\n",
                   namespace_name.c_str(),
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


std::string Nvs::sanitize_name(std::string_view name) const {
    if (name.empty()) {
        return {};
    }

    std::string out;
    out.append(name.data(), name.size());

    for (char& c : out) {
        if (c == '\0') {
            c = '_';
        }
    }

    if (out.size() > MAX_KEY_LEN) {
        DBG_PRINTF(Nvs,
                   "sanitize_name(): WARNING: name '%s' is too long (%u chars), truncating to %u.\n",
                   out.c_str(),
                   static_cast<unsigned>(out.size()),
                   static_cast<unsigned>(MAX_KEY_LEN));

        out.resize(MAX_KEY_LEN);
    }

    return out;
}


void Nvs::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    DBG_PRINTF(Nvs,
               "reset(): clearing all keys in NVS namespace '%s'.\n",
               id.c_str());

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(id, NVS_READWRITE, sh);

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


bool Nvs::factory_reset() {
    DBG_PRINTLN(Nvs, "factory_reset(): erasing the entire default NVS partition.");

    (void)nvs_flash_deinit();
    m_nvs_ready = false;

    const esp_err_t erase_err = nvs_flash_erase();

    if (erase_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "factory_reset(): nvs_flash_erase() failed: %s (%d).\n",
                   esp_err_to_name(erase_err),
                   static_cast<int>(erase_err));
        return false;
    }

    return ensure_ready();
}


void Nvs::remove(std::string_view ns, std::string_view key) {
    if (key.empty()) {
        DBG_PRINTLN(Nvs, "remove(): ERROR: empty key.");
        return;
    }

    const std::string storage_key = sanitize_name(key);

    DBG_PRINTF(Nvs,
               "remove(): ns='%.*s', key='%.*s', storage_key='%s'.\n",
               static_cast<int>(ns.size()), ns.data(),
               static_cast<int>(key.size()), key.data(),
               storage_key.c_str());

    ScopedHandle sh;
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);

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
    const std::string namespace_name = sanitize_name(ns);

    if (namespace_name.empty()) {
        DBG_PRINTLN(Nvs, "reset_ns(): ERROR: empty namespace.");
        return;
    }

    DBG_PRINTF(Nvs,
               "reset_ns(): clearing NVS namespace '%s'.\n",
               namespace_name.c_str());

    // Probe read-only first: a namespace that was never written does not exist,
    // and we must not create it just to erase it (read-only never creates one).
    ScopedHandle sh;
    const esp_err_t probe_err = open_handle(ns, NVS_READONLY, sh);

    if (probe_err == ESP_ERR_NVS_NOT_FOUND) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): namespace '%s' does not exist; nothing to erase.\n",
                   namespace_name.c_str());
        return;
    }

    if (probe_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): probe open failed for '%s': %s (%d).\n",
                   namespace_name.c_str(),
                   esp_err_to_name(probe_err),
                   static_cast<int>(probe_err));
        return;
    }

    // Reopen read-write (open_handle closes the read-only handle first).
    const esp_err_t open_err = open_handle(ns, NVS_READWRITE, sh);

    if (open_err != ESP_OK) {
        DBG_PRINTF(Nvs,
                   "reset_ns(): open failed for '%s': %s (%d).\n",
                   namespace_name.c_str(),
                   esp_err_to_name(open_err),
                   static_cast<int>(open_err));
        return;
    }

    const esp_err_t erase_err = nvs_erase_all(sh);

    if (!commit_and_close(sh, erase_err, "reset_ns()", namespace_name)) {
        DBG_PRINTLN(Nvs, "reset_ns(): failed.");
        return;
    }

    DBG_PRINTF(Nvs,
               "reset_ns(): namespace '%s' cleared.\n",
               namespace_name.c_str());
}
