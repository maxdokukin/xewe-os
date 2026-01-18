/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// <filepath from project root>
#pragma once

#include "../../Module/Module.h"

#include <WebServer.h>
#include <string>
#include <sstream>
#include <iomanip>

struct WebConfig : public ModuleConfig {};


class WebInterface : public Module {
public:
    explicit                    WebInterface                (SystemController& controller);

    // optional functions, can be overridden; def is Module.cpp
    void                        begin_routines_required     (const ModuleConfig& cfg)       override;
    void                        begin_routines_regular      (const ModuleConfig& cfg)       override;
    void                        begin_routines_common       (const ModuleConfig& cfg)       override;

    void                        loop                        ()                              override;
                                                             const bool do_restart=true)    override;
    void                        reset                       (const bool verbose=false,
                                                             const bool do_restart=true)    override;

    string                      status                      (const bool verbose=false)      const override;

    // custom functions template
    WebServer&                  get_server                  ()                              { return httpServer; }
private:
    WebServer                   httpServer                  {80};

    void                        serveMainPage               ();
    void                        handleCommandRequest        ();

    static const char           INDEX_HTML                  [] PROGMEM;
};
