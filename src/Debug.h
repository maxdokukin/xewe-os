/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// Debug.h
#pragma once


#define DEBUG_SystemController  0
#define DEBUG_Module            1
#define DEBUG_SerialPort        0
#define DEBUG_Nvs               1
#define DEBUG_System            0
#define DEBUG_CommandParser     0
#define DEBUG_Wifi              1
#define DEBUG_WebInterface      0


#define DBG_ENABLED(cls)      (DEBUG_##cls)

#define DBG_PRINTLN(cls, msg)                                    \
    do { if (DBG_ENABLED(cls)) {                                 \
            Serial.print("[DBG] [");                             \
            Serial.print(#cls); /* <--- Changed here */          \
            Serial.print("]: ");                                 \
            Serial.println(msg);                                 \
        } } while(0)

#define DBG_PRINTF(cls, fmt, ...)                                \
    do { if (DBG_ENABLED(cls)) {                                 \
            Serial.print("[DBG] [");                             \
            Serial.print(#cls); /* <--- Changed here */          \
            Serial.print("]: ");                                 \
            Serial.printf((fmt), ##__VA_ARGS__);                 \
        } } while(0)
