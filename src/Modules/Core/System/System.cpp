/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Software/System/System.cpp


#include "System.h"
#include "../../Module/ModuleController.h"


System::System(ModuleController& controller)
      : Module(controller,
               /* id                  */ "sys",
               /* name                */ "System",
               /* description         */ "Stores integral commands and routines",
               /* requires_init_setup */ true, // this affects global logic, do not set to false
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ true) {

    commands_storage.push_back({
        "restart",
        "Restart the ESP",
        std::string("$") + xewe::str::lower(name) + " restart",
        0,
        [this](std::string_view) { restart(1000); }
    });

    commands_storage.push_back({
        "reboot",
        "Restart the ESP",
        std::string("$") + xewe::str::lower(name) + " reboot",
        0,
        [this](std::string_view) { restart(1000); }
    });

    commands_storage.push_back({
      "info","Chip and build info",
      std::string("$")+xewe::str::lower(name)+" info",
      0,
      [this](std::string_view){
        esp_chip_info_t ci; esp_chip_info(&ci);
        uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
        std::size_t   flash_sz = ESP.getFlashChipSize();
        uint32_t flash_hz = ESP.getFlashChipSpeed();

        std::string s;
        s += "Model "; s += std::to_string((int)ci.model);
        s += "  Cores "; s += std::to_string((int)ci.cores);
        s += "  Rev "; s += std::to_string((int)ci.revision); s += "\n";
        s += "IDF "; s += esp_get_idf_version(); s += "\n";
        s += "Flash "; s += std::to_string((unsigned)flash_sz);
        s += " bytes @ "; s += std::to_string((unsigned)flash_hz); s += " Hz\n";
        char macs[18]; snprintf(macs, sizeof(macs), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        s += "MAC "; s += macs;
        this->controller.serial_port.print(s.c_str(), xewe::str::kCRLF);
      }
    });

    commands_storage.push_back({
      "set_device_name",
      "Set device name",
      std::string("$") + xewe::str::lower(name) + " set_device_name \"Kitchen Lights\"",
      1,
      [this](std::string_view args_sv){
        String args(args_sv.data(), args_sv.length());
        args.trim();

        if (args.isEmpty()) {
          this->controller.serial_port.print(
            ("Usage: " + xewe::str::lower(name) + " set_device_name \"<name>\"").c_str(),
            xewe::str::kCRLF
          );
          return;
        }

        if (args.length() >= 2 && args[0] == '"' && args[args.length() - 1] == '"') {
          args = args.substring(1, args.length() - 1);
          args.trim();
        }

        if (args.isEmpty()) {
          this->controller.serial_port.print("Device name cannot be empty", xewe::str::kCRLF);
          return;
        }

        std::string new_name = args.c_str();
        this->controller.nvs.write_str(id, "dname", new_name);
        this->controller.serial_port.print(
          ("Device name set to: " + new_name).c_str(),
          xewe::str::kCRLF
        );
      }
    });

    commands_storage.push_back({
      "mac","Print MAC addresses",
      std::string("$")+xewe::str::lower(name)+" mac",0,
      [this](std::string_view){
        struct Item{ const char* name; esp_mac_type_t t; } items[]={
          {"wifi_sta", ESP_MAC_WIFI_STA},
          {"wifi_ap",  ESP_MAC_WIFI_SOFTAP},
          {"bt",       ESP_MAC_BT},
          {"eth",      ESP_MAC_ETH},
        };
        for(auto& it: items){
          uint8_t m[6]; if(esp_read_mac(m, it.t)==ESP_OK){
            char line[40]; snprintf(line, sizeof(line), "%s %02X:%02X:%02X:%02X:%02X:%02X", it.name,m[0],m[1],m[2],m[3],m[4],m[5]);
            this->controller.serial_port.print(line, xewe::str::kCRLF);
          }
        }
      }
    });

    commands_storage.push_back({
      "uid","Device UID from eFuse base MAC (and SHA256-64)",
      std::string("$")+xewe::str::lower(name)+" uid",0,
      [this](std::string_view){
        uint8_t mac[6]; esp_efuse_mac_get_default(mac);
        uint8_t dig[32]; mbedtls_sha256(mac, sizeof(mac), dig, 0 /* is224 */);
        this->controller.serial_port.print(("base_mac "+xewe::str::to_hex(mac, sizeof(mac))).c_str(), xewe::str::kCRLF);
        this->controller.serial_port.print(("uid64 "+xewe::str::to_hex(dig, 8)).c_str(), xewe::str::kCRLF);
      }
    });

    commands_storage.push_back({
      "stack","Current task stack watermark (words)",
      std::string("$")+xewe::str::lower(name)+" stack",0,
      [this](std::string_view){
        this->controller.serial_port.print(std::to_string((unsigned)uxTaskGetStackHighWaterMark(nullptr)).c_str(), xewe::str::kCRLF);
      }
    });
}

void System::begin_routines_required (const ModuleConfig& cfg) {
    this->controller.serial_port.print_header(
        std::string(PROJECT_NAME) + "\\sep" +
        "https://github.com/maxdokukin/" + PROJECT_NAME + "\\sep" +
        "Version " + BUILD_VERSION + "\n" +
        "Build Timestamp " + BUILD_TIMESTAMP
    );
}

void System::begin_routines_init (const ModuleConfig& cfg) {
    std::string name = "";
    bool confirmed = false;
    while (!confirmed) {
        name = controller.serial_port.get_string("Name your device (ex: Kitchen Lights):");
        confirmed = controller.serial_port.get_yn("Confirm \"" + name + "\"?");
    }
    controller.nvs.write_str(id, "dname", name);
}

void System::reset (const bool verbose, const bool do_restart, const bool keep_enabled) {
    bool disable_confirmed = false;

    if (verbose) {
        controller.serial_port.print_header("[WARNING]\nResetting System\nWill reset all modules");
        disable_confirmed = controller.serial_port.get_yn("OK?");
    }

    if (!disable_confirmed) {
        controller.serial_port.print("Aborted");
        return;
    }

    auto& modules = controller.get_modules();
    for (auto* m : modules) {
        if (m == this) continue;
        m->reset(true, false, false);
    }

    Module::reset(verbose, do_restart, keep_enabled);
}

std::string System::status(const bool verbose) const {
    if (verbose) {
        std::vector<std::vector<std::string_view>> table_data;
        table_data.push_back({"Module Name", "Enabled", "Status"});
        std::vector<std::string> string_storage;
        string_storage.reserve(controller.get_modules().size() * 2);

        auto& modules = controller.get_modules();
        for (const auto* mod : modules) {
            if (!mod) continue;
            std::string_view name = mod->get_name();
            string_storage.push_back(mod->is_enabled() ? "Yes" : "No");
            std::string_view enabled_view = string_storage.back();

            string_storage.push_back(mod->status(false));
            std::string_view status_view = string_storage.back();

            table_data.push_back({name, enabled_view, status_view});
        }

        controller.serial_port.print_table(
            table_data,
            "System Status"
        );
    }
    return "System OK";
}

std::string System::get_device_name () { return controller.nvs.read_str(id, "dname"); };

void System::restart (uint16_t delay_ms) {
    controller.serial_port.print_header("Rebooting");
    delay(delay_ms);
    ESP.restart();
}
