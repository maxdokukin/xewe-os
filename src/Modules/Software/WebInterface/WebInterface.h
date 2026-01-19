/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Software/WebInterface/WebInterface.h
#pragma once

#include "../../Module/Module.h"

#include <WebServer.h>
#include <string>
#include <sstream>
#include <iomanip>

struct WebInterfaceConfig : public ModuleConfig {};


class WebInterface : public Module {
public:
    explicit                    WebInterface                (SystemController& controller);

    void                        begin_routines_required     (const ModuleConfig& cfg)       override;

    void                        loop                        ()                              override;
    string                      status                      (const bool verbose=false)      const override;

    WebServer&                  get_server                  ()                              { return http_server; }
private:
    WebServer                   http_server                  {80};

    void                        serve_main_page               ();
    void                        handle_command_request        ();

    static const char           INDEX_HTML                  [] PROGMEM;
};
