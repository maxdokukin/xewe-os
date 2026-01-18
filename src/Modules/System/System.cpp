/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/XeWe-LED-OS
 *********************************************************************************/
// src/Modules/Software/System/System.cpp

#include "System.h"
#include "../../SystemController/SystemController.h"

#include <Arduino.h>
#include <Wire.h>
#include <sstream>
#include <vector>
#include <esp_rom_crc.h>
#include "mbedtls/sha256.h"

#include <Arduino.h>          // LEDC, millis, etc.
#include <sstream>            // std::istringstream
#include <esp_rom_crc.h>      // esp_rom_crc32_le
#include "mbedtls/sha256.h"   // mbedtls SHA256 API
#include "esp_task_wdt.h"     // Task WDT config
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_efuse.h>
#include <esp_random.h>
#include <esp_rom_crc.h>

#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <sys/time.h>

// Helpers
static std::string toHex(const uint8_t* b, size_t n) {
    static const char* k = "0123456789ABCDEF";
    std::string s; s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s.push_back(k[b[i] >> 4]); s.push_back(k[b[i] & 0x0F]); }
    return s;
}
static void hexdump_lines(SystemController& c, const uint8_t* buf, size_t len, uint32_t base = 0){
    char line[16];
    for (size_t i = 0; i < len; i += 16) {
        snprintf(line, sizeof(line), "%08X", (unsigned)(base + i));
        c.serial_port.print(line); c.serial_port.print("  ");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                char b[4]; snprintf(b, sizeof(b), "%02X ", buf[i + j]);
                c.serial_port.print(b);
            } else {
                c.serial_port.print("   ");
            }
        }
        c.serial_port.print(" ");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                char ch = (buf[i + j] >= 32 && buf[i + j] < 127) ? buf[i + j] : '.';
                char a[2] = { ch, 0 };
                c.serial_port.print(a);
            }
        }
        c.serial_port.print("", kCRLF);
    }
}

System::System(SystemController& controller)
      : Module(controller,
               /* module_name         */ "System",
               /* module_description  */ "Stores integral commands and routines",
               /* nvs_key             */ "sys",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ true) {

    // 1) restart
    commands_storage.push_back({
        "restart",
        "Restart the ESP",
        string("Sample Use: $") + lower(module_name) + " restart",
        0,
        [this](string_view) { ESP.restart(); }
    });

    // 2) reboot
    commands_storage.push_back({
        "reboot",
        "Restart the ESP",
        string("Sample Use: $") + lower(module_name) + " reboot",
        0,
        [this](string_view) { ESP.restart(); }
    });

    // 3) info
    commands_storage.push_back({
      "info","Chip and build info",
      string("Sample Use: $")+lower(module_name)+" info",
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

    // 4) uptime
    commands_storage.push_back({
      "uptime","Millis since boot",
      string("Sample Use: $")+lower(module_name)+" uptime",0,
      [this](string_view){
        uint64_t us = esp_timer_get_time();
        uint64_t ms = us / 1000ULL;
        this->controller.serial_port.print(("uptime_ms " + std::to_string((unsigned long long)ms)).c_str(), kCRLF);
      }
    });

    // 5) heap
    commands_storage.push_back({
      "heap","Heap stats",
      string("Sample Use: $")+lower(module_name)+" heap",0,
      [this](string_view){
        size_t free8=heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t min8 =heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        size_t big8 =heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        this->controller.serial_port.print(("8bit free "+std::to_string((unsigned)free8)+" min "+std::to_string((unsigned)min8)+" largest "+std::to_string((unsigned)big8)).c_str(), kCRLF);
#ifdef MALLOC_CAP_SPIRAM
        size_t frees=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t mins =heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        size_t bigs =heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        this->controller.serial_port.print(("psram free "+std::to_string((unsigned)frees)+" min "+std::to_string((unsigned)mins)+" largest "+std::to_string((unsigned)bigs)).c_str(), kCRLF);
#endif
      }
    });

    // 6) tasks
    commands_storage.push_back({
      "tasks","FreeRTOS task list",
      string("Sample Use: $")+lower(module_name)+" tasks",0,
      [this](string_view){
#if (configUSE_TRACE_FACILITY==1) && (configUSE_STATS_FORMATTING_FUNCTIONS==1)
        static char buf[2048];
        vTaskList(buf);
        this->controller.serial_port.print(buf);
#else
        this->controller.serial_port.print("Enable configUSE_TRACE_FACILITY and configUSE_STATS_FORMATTING_FUNCTIONS", kCRLF);
#endif
      }
    });

    // 7) sleep
    commands_storage.push_back({
      "sleep","Deep sleep ms",
      string("Sample Use: $")+lower(module_name)+" sleep 5000",1,
      [this](string_view args){
        uint32_t ms = strtoul(std::string(args).c_str(), nullptr, 10);
        esp_sleep_enable_timer_wakeup((uint64_t)ms*1000ULL);
        this->controller.serial_port.print("Deep sleeping...", kCRLF);
        esp_deep_sleep_start();
      }
    });

    // 8) reset-reason
    commands_storage.push_back({
      "reset-reason","Last reset and wake cause",
      string("Sample Use: $")+lower(module_name)+" reset-reason",0,
      [this](string_view){
        std::string s = "reset " + std::to_string((int)esp_reset_reason()) + "  wake " + std::to_string((int)esp_sleep_get_wakeup_cause());
        this->controller.serial_port.print(s.c_str(), kCRLF);
      }
    });

    // 9) loglevel
    commands_storage.push_back({
      "loglevel","Set ESP log level",
      string("Sample Use: $")+lower(module_name)+" loglevel * info",2,
      [this](string_view args){
        auto s=std::string(args); auto sp=s.find(' ');
        if(sp==std::string::npos){ this->controller.serial_port.print("need TAG LEVEL", kCRLF); return; }
        std::string tag=s.substr(0,sp), lvl=s.substr(sp+1);
        esp_log_level_t l=ESP_LOG_INFO;
        if(lvl=="none") l=ESP_LOG_NONE; else if(lvl=="error") l=ESP_LOG_ERROR;
        else if(lvl=="warn") l=ESP_LOG_WARN; else if(lvl=="debug") l=ESP_LOG_DEBUG;
        else if(lvl=="verbose") l=ESP_LOG_VERBOSE;
        esp_log_level_set(tag=="*" ? "*" : tag.c_str(), l);
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    // 10) time
    commands_storage.push_back({
      "time","Get or set RTC",
      string("Sample Use: $")+lower(module_name)+" time 2025-10-22 12:34:56",0,
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

    // 11) partitions
    commands_storage.push_back({
      "partitions","Partition table summary",
      string("Sample Use: $")+lower(module_name)+" partitions",0,
      [this](string_view){
        const esp_partition_t* boot=esp_ota_get_boot_partition();
        const esp_partition_t* run =esp_ota_get_running_partition();
        std::string hdr = "boot "; hdr += boot?boot->label:"?"; hdr += "  run "; hdr += run?run->label:"?";
        this->controller.serial_port.print(hdr.c_str(), kCRLF);
        esp_partition_iterator_t it=esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while(it){ auto p=esp_partition_get(it);
          char line[96]; snprintf(line, sizeof(line), "APP %-8s addr 0x%08X size %u", p->label, (unsigned)p->address, (unsigned)p->size);
          this->controller.serial_port.print(line, kCRLF);
          it=esp_partition_next(it);
        }
        esp_partition_iterator_release(it);
      }
    });

    // 12) ota
    commands_storage.push_back({
      "ota","OTA status/control",
      string("Sample Use: $")+lower(module_name)+" ota status|mark-valid|rollback",1,
      [this](string_view args){
        std::string a(args);
        const esp_partition_t* run=esp_ota_get_running_partition();
        if(a=="status"){
          esp_ota_img_states_t st; esp_ota_get_state_partition(run,&st);
          std::string s = "running "; s += run?run->label:"?"; s += "  state "; s += std::to_string((int)st);
          this->controller.serial_port.print(s.c_str(), kCRLF);
        } else if(a=="mark-valid"){
          esp_ota_mark_app_valid_cancel_rollback();
          this->controller.serial_port.print("marked valid", kCRLF);
        } else if(a=="rollback"){
          esp_ota_mark_app_invalid_rollback_and_reboot();
        } else {
          this->controller.serial_port.print("status|mark-valid|rollback", kCRLF);
        }
      }
    });

    // 13) cpu-freq
    commands_storage.push_back({
      "cpu-freq","Get CPU frequency (MHz)",
      string("Sample Use: $")+lower(module_name)+" cpu-freq",0,
      [this](string_view){
        this->controller.serial_port.print(std::to_string((unsigned)getCpuFrequencyMhz()).c_str(), kCRLF);
      }
    });

    // 14) set-cpu-freq <mhz>
    commands_storage.push_back({
      "set-cpu-freq","Set CPU frequency MHz (chip-dependent)",
      string("Sample Use: $")+lower(module_name)+" set-cpu-freq 160",1,
      [this](string_view args){
        uint32_t mhz = strtoul(std::string(args).c_str(), nullptr, 10);
        if(mhz==0){ this->controller.serial_port.print("need MHz", kCRLF); return; }
        setCpuFrequencyMhz(mhz);
        this->controller.serial_port.print(("now "+std::to_string((unsigned)getCpuFrequencyMhz())+" MHz").c_str(), kCRLF);
      }
    });

    // 15) random <nbytes>
    commands_storage.push_back({
      "random","Print N random bytes hex",
      string("Sample Use: $")+lower(module_name)+" random 16",1,
      [this](string_view args){
        size_t n = strtoul(std::string(args).c_str(), nullptr, 10);
        if(n==0 || n>1024) n=16;
        std::vector<uint8_t> buf(n);
        esp_fill_random(buf.data(), buf.size());
        auto hex = toHex(buf.data(), buf.size());
        this->controller.serial_port.print(hex.c_str(), kCRLF);
      }
    });

    // 16) crc32 <text>
    commands_storage.push_back({
      "crc32","CRC32 of text",
      string("Sample Use: $")+lower(module_name)+" crc32 hello",1,
      [this](string_view args){
        std::string s(args);
        uint32_t crc = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(s.data()), static_cast<uint32_t>(s.size()));
        char buf[32]; snprintf(buf, sizeof(buf), "crc32 0x%08lX", static_cast<unsigned long>(crc));
        this->controller.serial_port.print(buf, kCRLF);
      }
    });

    // 17) sha256 <text>
    commands_storage.push_back({
      "sha256","SHA256 of text (hex)",
      string("Sample Use: $")+lower(module_name)+" sha256 hello",1,
      [this](string_view args){
        std::string s(args);
        uint8_t dig[32];
        mbedtls_sha256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), dig, 0 /* is224 */);
        auto hex = toHex(dig, sizeof(dig));
        this->controller.serial_port.print(hex.c_str(), kCRLF);
      }
    });


    // 18) base64-enc <text>
    commands_storage.push_back({
      "base64-enc","Base64 encode",
      string("Sample Use: $")+lower(module_name)+" base64-enc hello",1,
      [this](string_view args){
        std::string s(args); size_t olen=0;
        std::vector<unsigned char> out((s.size()*4)/3+8);
        mbedtls_base64_encode(out.data(), out.size(), &olen, (const unsigned char*)s.data(), s.size());
        out.resize(olen);
        std::string enc((char*)out.data(), out.size());
        this->controller.serial_port.print(enc.c_str(), kCRLF);
      }
    });

    // 19) base64-dec <b64>
    commands_storage.push_back({
      "base64-dec","Base64 decode → hex",
      string("Sample Use: $")+lower(module_name)+" base64-dec aGVsbG8=",1,
      [this](string_view args){
        std::string s(args); size_t olen=0;
        std::vector<unsigned char> out((s.size()*3)/4+8);
        int r = mbedtls_base64_decode(out.data(), out.size(), &olen, (const unsigned char*)s.data(), s.size());
        if(r!=0){ this->controller.serial_port.print("decode error", kCRLF); return; }
        auto hex = toHex(out.data(), olen);
        this->controller.serial_port.print(hex.c_str(), kCRLF);
      }
    });

    // 20) gpio-read <pin>
    commands_storage.push_back({
      "gpio-read","Digital read",
      string("Sample Use: $")+lower(module_name)+" gpio-read 5",1,
      [this](string_view args){
        int pin = atoi(std::string(args).c_str());
        pinMode(pin, INPUT);
        this->controller.serial_port.print(std::to_string((int)digitalRead(pin)).c_str(), kCRLF);
      }
    });

    // 21) gpio-write <pin> <0|1>
    commands_storage.push_back({
      "gpio-write","Digital write",
      string("Sample Use: $")+lower(module_name)+" gpio-write 5 1",2,
      [this](string_view args){
        std::string s(args); auto sp=s.find(' ');
        if(sp==std::string::npos){ this->controller.serial_port.print("need PIN LEVEL", kCRLF); return; }
        int pin = atoi(s.substr(0,sp).c_str());
        int lvl = atoi(s.substr(sp+1).c_str());
        pinMode(pin, OUTPUT);
        digitalWrite(pin, lvl?HIGH:LOW);
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    // 22) gpio-mode <pin> <in|in_pullup|in_pulldown|out>
    commands_storage.push_back({
      "gpio-mode","Set pin mode",
      string("Sample Use: $")+lower(module_name)+" gpio-mode 5 out",2,
      [this](string_view args){
        std::string s(args); auto sp=s.find(' ');
        if(sp==std::string::npos){ this->controller.serial_port.print("need PIN MODE", kCRLF); return; }
        int pin = atoi(s.substr(0,sp).c_str()); std::string m=s.substr(sp+1);
        if(m=="out") pinMode(pin, OUTPUT);
        else if(m=="in") pinMode(pin, INPUT);
#ifdef INPUT_PULLDOWN
        else if(m=="in_pulldown") pinMode(pin, INPUT_PULLDOWN);
#endif
        else if(m=="in_pullup") pinMode(pin, INPUT_PULLUP);
        else { this->controller.serial_port.print("modes: in|in_pullup|in_pulldown|out", kCRLF); return; }
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    // 23) adc-read <pin>
    commands_storage.push_back({
      "adc-read","Analog read (raw)",
      string("Sample Use: $")+lower(module_name)+" adc-read 1",1,
      [this](string_view args){
        int pin = atoi(std::string(args).c_str());
        int v = analogRead(pin);
        this->controller.serial_port.print(std::to_string(v).c_str(), kCRLF);
      }
    });

    // 24) i2c-scan
    commands_storage.push_back({
      "i2c-scan","Scan I2C bus on default pins",
      string("Sample Use: $")+lower(module_name)+" i2c-scan",0,
      [this](string_view){
        Wire.begin();
        int found=0;
        for(uint8_t addr=1; addr<0x78; ++addr){
          Wire.beginTransmission(addr);
          uint8_t err = Wire.endTransmission();
          if(err==0){
            char ln[8]; snprintf(ln, sizeof(ln), "0x%02X", addr);
            this->controller.serial_port.print(ln, kCRLF);
            found++;
          }
        }
        if(found==0) this->controller.serial_port.print("none", kCRLF);
      }
    });

    // 25) pwm-setup <ch> <pin> <freq> <bits>
    commands_storage.push_back({
      "pwm-setup","LEDC setup and attach",
      string("Sample Use: $")+lower(module_name)+" pwm-setup 0 5 5000 10",4,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch, pin; double freq; int bits;
        if(!(is >> ch >> pin >> freq >> bits)){ this->controller.serial_port.print("need CH PIN FREQ BITS", kCRLF); return; }
        (void)ch; // channel unused in pin-centric API
        if(!ledcAttach(static_cast<uint8_t>(pin),
                       static_cast<uint32_t>(freq),
                       static_cast<uint8_t>(bits))){
          this->controller.serial_port.print("attach failed", kCRLF); return;
        }
        this->controller.serial_port.print("ok", kCRLF);
      }
    });


//    // 26) pwm-write <ch> <duty>
    // 26) pwm-write <ch> <duty>
    commands_storage.push_back({
      "pwm-write","LEDC write duty (0..2^bits-1)",
      string("Sample Use: $")+lower(module_name)+" pwm-write 0 512",2,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch; int duty;
        if(!(is >> ch >> duty)){ this->controller.serial_port.print("need CH DUTY", kCRLF); return; }
        ledcWrite(static_cast<uint8_t>(ch), static_cast<uint32_t>(duty));
        this->controller.serial_port.print("ok", kCRLF);
      }
    });


    // 27) pwm-stop <ch> [pin]
    commands_storage.push_back({
      "pwm-stop","LEDC detach channel",
      string("Sample Use: $")+lower(module_name)+" pwm-stop 0 5",2,
      [this](string_view args){
        std::istringstream is{std::string(args)}; int ch; int pin = -1;
        if(!(is >> ch)){ this->controller.serial_port.print("need CH [PIN]", kCRLF); return; }
        if(is >> pin){ ledcDetach(static_cast<uint8_t>(pin)); }
        ledcWrite(static_cast<uint8_t>(ch), 0);
        this->controller.serial_port.print("ok", kCRLF);
      }
    });


    // 28) stack
    commands_storage.push_back({
      "stack","Current task stack watermark (words)",
      string("Sample Use: $")+lower(module_name)+" stack",0,
      [this](string_view){
        this->controller.serial_port.print(std::to_string((unsigned)uxTaskGetStackHighWaterMark(nullptr)).c_str(), kCRLF);
      }
    });

    // 29) tasks-rt
    commands_storage.push_back({
      "tasks-rt","FreeRTOS runtime stats",
      string("Sample Use: $")+lower(module_name)+" tasks-rt",0,
      [this](string_view){
#if (configUSE_TRACE_FACILITY==1) && (configGENERATE_RUN_TIME_STATS==1)
        static char buf[2048];
        vTaskGetRunTimeStats(buf);
        this->controller.serial_port.print(buf);
#else
        this->controller.serial_port.print("Enable TRACE and RUNTIME stats", kCRLF);
#endif
      }
    });

    // 30) yield
    commands_storage.push_back({
      "yield","taskYIELD once",
      string("Sample Use: $")+lower(module_name)+" yield",0,
      [this](string_view){ taskYIELD(); }
    });

    // 31) wait <ms>
    commands_storage.push_back({
      "wait","Delay ms",
      string("Sample Use: $")+lower(module_name)+" wait 200",1,
      [this](string_view args){
        vTaskDelay(pdMS_TO_TICKS(strtoul(std::string(args).c_str(), nullptr, 10)));
        this->controller.serial_port.print("ok", kCRLF);
      }
    });

    // 32) reboot-delay <ms>
    commands_storage.push_back({
      "reboot-delay","Delay then restart",
      string("Sample Use: $")+lower(module_name)+" reboot-delay 1000",1,
      [this](string_view args){
        uint32_t ms=strtoul(std::string(args).c_str(),nullptr,10);
        vTaskDelay(pdMS_TO_TICKS(ms)); ESP.restart();
      }
    });

    // 33) mac [sta|ap|bt|eth]
    commands_storage.push_back({
      "mac","Print MAC addresses",
      string("Sample Use: $")+lower(module_name)+" mac",0,
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

    // 34) uid
    commands_storage.push_back({
      "uid","Device UID from eFuse base MAC (and SHA256-64)",
      string("Sample Use: $")+lower(module_name)+" uid",0,
      [this](string_view){
        uint8_t mac[6]; esp_efuse_mac_get_default(mac);
        uint8_t dig[32]; mbedtls_sha256(mac, sizeof(mac), dig, 0 /* is224 */);
        this->controller.serial_port.print(("base_mac "+toHex(mac, sizeof(mac))).c_str(), kCRLF);
        this->controller.serial_port.print(("uid64 "+toHex(dig, 8)).c_str(), kCRLF);
      }
    });


    // 35) flash
    commands_storage.push_back({
      "flash","Flash size/speed/mode",
      string("Sample Use: $")+lower(module_name)+" flash",0,
      [this](string_view){
        size_t sz = ESP.getFlashChipSize();
        uint32_t hz = ESP.getFlashChipSpeed();
        auto mode = ESP.getFlashChipMode();
        const char* m = "UNK";
        switch(mode){
          case FM_QIO:  m="QIO";  break;
          case FM_QOUT: m="QOUT"; break;
          case FM_DIO:  m="DIO";  break;
          case FM_DOUT: m="DOUT"; break;
          default: break;
        }
        std::string s = "size "+std::to_string(static_cast<unsigned>(sz))+
                        "  speed "+std::to_string(static_cast<unsigned>(hz))+
                        "  mode "+m;
        this->controller.serial_port.print(s.c_str(), kCRLF);
      }
    });


    // 36) partitions-data
    commands_storage.push_back({
      "partitions-data","List DATA partitions",
      string("Sample Use: $")+lower(module_name)+" partitions-data",0,
      [this](string_view){
        esp_partition_iterator_t it=esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while(it){ auto p=esp_partition_get(it);
          char line[96]; snprintf(line, sizeof(line), "DATA %-8s subtype 0x%02X addr 0x%08X size %u", p->label, p->subtype, (unsigned)p->address, (unsigned)p->size);
          this->controller.serial_port.print(line, kCRLF);
          it=esp_partition_next(it);
        }
        esp_partition_iterator_release(it);
      }
    });

    // 37) partition-read <label> <offset> <len<=256>
    commands_storage.push_back({
      "partition-read","Hexdump from partition label",
      string("Sample Use: $")+lower(module_name)+" partition-read nvs 0 64",3,
      [this](string_view args){
        std::istringstream is{std::string(args)}; std::string label; uint32_t off=0, len=0;
        if(!(is >> label >> off >> len)){ this->controller.serial_port.print("need LABEL OFFSET LEN", kCRLF); return; }
        if(len==0 || len>256) len=64;
        const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label.c_str());
        if(!p){ this->controller.serial_port.print("label not found", kCRLF); return; }
        std::vector<uint8_t> buf(len);
        if(esp_partition_read(p, off, buf.data(), buf.size()) != ESP_OK){ this->controller.serial_port.print("read error", kCRLF); return; }
        hexdump_lines(this->controller, buf.data(), buf.size(), p->address + off);
      }
    });

    // 38) light-sleep <ms>
    commands_storage.push_back({
      "light-sleep","Light sleep for ms",
      string("Sample Use: $")+lower(module_name)+" light-sleep 200",1,
      [this](string_view args){
        uint32_t ms = strtoul(std::string(args).c_str(), nullptr, 10);
        uint64_t t0=esp_timer_get_time();
        esp_sleep_enable_timer_wakeup((uint64_t)ms*1000ULL);
        esp_light_sleep_start();
        uint64_t dt=esp_timer_get_time()-t0;
        this->controller.serial_port.print(("slept_us "+std::to_string((unsigned long long)dt)).c_str(), kCRLF);
      }
    });

    // 39) heap-frag
    commands_storage.push_back({
      "heap-frag","Heap fragmentation %",
      string("Sample Use: $")+lower(module_name)+" heap-frag",0,
      [this](string_view){
        size_t free8=heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t big8 =heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        int pct = (free8==0)?0 : (int)((100 - (big8*100.0/free8)) + 0.5);
        this->controller.serial_port.print(std::to_string(pct).c_str(), kCRLF);
      }
    });

    // 40) min-free-heap
    commands_storage.push_back({
      "min-free-heap","Minimum free heap since boot",
      string("Sample Use: $")+lower(module_name)+" min-free-heap",0,
      [this](string_view){
        this->controller.serial_port.print(std::to_string((unsigned)ESP.getMinFreeHeap()).c_str(), kCRLF);
      }
    });

    // 41) psram
    commands_storage.push_back({
      "psram","PSRAM size and free",
      string("Sample Use: $")+lower(module_name)+" psram",0,
      [this](string_view){
#if defined(ESP_IDF_VERSION_MAJOR)
        size_t sz = ESP.getPsramSize();
        if(sz==0){ this->controller.serial_port.print("no-psram", kCRLF); return; }
        std::string s = "size "+std::to_string((unsigned)sz)+"  free "+std::to_string((unsigned)ESP.getFreePsram());
        this->controller.serial_port.print(s.c_str(), kCRLF);
#else
        this->controller.serial_port.print("no-psram", kCRLF);
#endif
      }
    });

    // 42) wdt <start ms|feed|stop>
    commands_storage.push_back({
      "wdt","Task WDT control",
      string("Sample Use: $")+lower(module_name)+" wdt start 2000",2,
      [this](string_view args){
        static bool started = false;
        std::istringstream is{std::string(args)}; std::string sub; is >> sub;
        if(sub=="start"){
          uint32_t ms = 0; is >> ms; if(ms < 100) ms = 100;
          esp_task_wdt_deinit();
          esp_task_wdt_config_t cfg{};
          cfg.timeout_ms = ms;
    #if ESP_IDF_VERSION_MAJOR >= 5
          cfg.trigger_panic = true;
          cfg.idle_core_mask = 1; // core 0 on ESP32-C3
    #endif
      if(esp_task_wdt_init(&cfg) == ESP_OK && esp_task_wdt_add(nullptr) == ESP_OK){
        started = true; this->controller.serial_port.print("wdt started", kCRLF);
      } else {
        this->controller.serial_port.print("wdt start error", kCRLF);
      }
    } else if(sub=="feed"){
      if(!started){ this->controller.serial_port.print("not started", kCRLF); return; }
      esp_task_wdt_reset(); this->controller.serial_port.print("ok", kCRLF);
    } else if(sub=="stop"){
      if(started){ esp_task_wdt_delete(nullptr); esp_task_wdt_deinit(); started = false; }
      this->controller.serial_port.print("stopped", kCRLF);
    } else {
      this->controller.serial_port.print("start <ms>|feed|stop", kCRLF);
    }
  }
});

}

void System::begin_routines_required (const ModuleConfig& cfg) {
    this->controller.serial_port.print_header(
        string("XeWe OS") + "\\sep" +
        "Lightweight ESP32 OS" + "\\sep" +
        "https://github.com/maxdokukin/XeWe-OS" + "\\sep" +
        "Version " + TO_STRING(BUILD_VERSION) + "\n" +
        "Build Timestamp " + TO_STRING(BUILD_TIMESTAMP)
    );
}
