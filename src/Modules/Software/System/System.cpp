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
#include "../../../SystemController/SystemController.h"


System::System(SystemController& controller)
      : Module(controller,
               /* module_name         */ "System",
               /* module_description  */ "Stores integral commands and routines",
               /* nvs_key             */ "sys",
               /* requires_init_setup */ true, // this affects global logic, do not set to false
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ true) {

    commands_storage.push_back({
        "restart",
        "Restart the ESP",
        string("$") + lower(module_name) + " restart",
        0,
        [this](string_view) { ESP.restart(); }
    });

    commands_storage.push_back({
        "reboot",
        "Restart the ESP",
        string("$") + lower(module_name) + " reboot",
        0,
        [this](string_view) { ESP.restart(); }
    });

    commands_storage.push_back({
      "info","Chip and build info",
      string("$")+lower(module_name)+" info",
      0,
      [this](string_view){
        esp_chip_info_t ci; esp_chip_info(&ci);
        uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
        size_t   flash_sz = ESP.getFlashChipSize();
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
        this->controller.serial_port.print(s.c_str(), kCRLF);
      }
    });

    commands_storage.push_back({
      "mac","Print MAC addresses",
      string("$")+lower(module_name)+" mac",0,
      [this](string_view){
        struct Item{ const char* name; esp_mac_type_t t; } items[]={
          {"wifi_sta", ESP_MAC_WIFI_STA},
          {"wifi_ap",  ESP_MAC_WIFI_SOFTAP},
          {"bt",       ESP_MAC_BT},
          {"eth",      ESP_MAC_ETH},
        };
        for(auto& it: items){
          uint8_t m[6]; if(esp_read_mac(m, it.t)==ESP_OK){
            char line[40]; snprintf(line, sizeof(line), "%s %02X:%02X:%02X:%02X:%02X:%02X", it.name,m[0],m[1],m[2],m[3],m[4],m[5]);
            this->controller.serial_port.print(line, kCRLF);
          }
        }
      }
    });

    commands_storage.push_back({
      "uid","Device UID from eFuse base MAC (and SHA256-64)",
      string("$")+lower(module_name)+" uid",0,
      [this](string_view){
        uint8_t mac[6]; esp_efuse_mac_get_default(mac);
        uint8_t dig[32]; mbedtls_sha256(mac, sizeof(mac), dig, 0 /* is224 */);
        this->controller.serial_port.print(("base_mac "+to_hex(mac, sizeof(mac))).c_str(), kCRLF);
        this->controller.serial_port.print(("uid64 "+to_hex(dig, 8)).c_str(), kCRLF);
      }
    });

    commands_storage.push_back({
      "stack","Current task stack watermark (words)",
      string("$")+lower(module_name)+" stack",0,
      [this](string_view){
        this->controller.serial_port.print(std::to_string((unsigned)uxTaskGetStackHighWaterMark(nullptr)).c_str(), kCRLF);
      }
    });

    commands_storage.push_back({
      "time","Get or set RTC",
      string("$")+lower(module_name)+" time 2025-10-22 12:34:56",0,
      [this](string_view args){
        if(args.empty()){
          time_t now=time(nullptr); struct tm tm_; localtime_r(&now,&tm_);
          char out[32]; strftime(out, sizeof(out), "%F %T", &tm_);
          this->controller.serial_port.print(out, kCRLF); return;
        }
        int Y,M,D,h,m,s;
        if(sscanf(std::string(args).c_str(), "%d-%d-%d %d:%d:%d",&Y,&M,&D,&h,&m,&s)==6){
          struct tm tm_={}; tm_.tm_year=Y-1900; tm_.tm_mon=M-1; tm_.tm_mday=D; tm_.tm_hour=h; tm_.tm_min=m; tm_.tm_sec=s;
          struct timeval tv={.tv_sec=(long)mktime(&tm_), .tv_usec=0}; settimeofday(&tv,nullptr);
          this->controller.serial_port.print("ok", kCRLF);
        } else {
          this->controller.serial_port.print("format YYYY-MM-DD HH:MM:SS", kCRLF);
        }
      }
    });

    commands_storage.push_back({
      "random","Print N random bytes hex",
      string("$")+lower(module_name)+" random 16",1,
      [this](string_view args){
        size_t n = strtoul(std::string(args).c_str(), nullptr, 10);
        if(n==0 || n>1024) n=16;
        std::vector<uint8_t> buf(n);
        esp_fill_random(buf.data(), buf.size());
        auto hex = to_hex(buf.data(), buf.size());
        this->controller.serial_port.print(hex.c_str(), kCRLF);
      }
    });
}

void System::begin_routines_required (const ModuleConfig& cfg) {
    this->controller.serial_port.print_header(
        string("XeWe OS") + "\\sep" +
        "https://github.com/maxdokukin/XeWe-OS" + "\\sep" +
        "Version " + TO_STRING(BUILD_VERSION) + "\n" +
        "Build Timestamp " + TO_STRING(BUILD_TIMESTAMP)
    );
}

std::string System::get_device_name () { return controller.nvs.read_str(nvs_key, "dname"); };

void System::restart (uint16_t delay_ms) {
    controller.serial_port.print_header("Rebooting");
    delay(delay_ms);
    ESP.restart();
}
