/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/XeWe-LED-OS
 *********************************************************************************/
// src/Modules/Software/System/System.h
#pragma once

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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <sys/time.h>


#include "../../Module/Module.h"
#include "../../build_info.h"


struct SystemConfig : public ModuleConfig {};


class System : public Module {
public:
    explicit                    System                      (SystemController& controller);

    void                        begin_routines_required     (const ModuleConfig& cfg)       override;
};
