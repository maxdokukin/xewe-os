/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/Software/CommandExecutor/CommandExecutor.h
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>
#include <span>

#include "../../Module/Module.h"


struct CommandExecutorConfig : public ModuleConfig {};


class CommandExecutor : public Module {
public:
    explicit                    CommandExecutor             (ModuleController& controller);

    void                        loop                        ()                              override;

    void                        parse                       (std::string_view input_line)   const;
    void                        print_help                  (std::string_view id)           const;
    void                        print_all_commands          ()                              const;

private:
    static std::string          trim_copy                   (std::string_view value);
    static std::string          lower_copy                  (std::string_view value);

    bool                        tokenize                    (std::string_view input,
                                                             std::vector<std::string>& out) const;
};