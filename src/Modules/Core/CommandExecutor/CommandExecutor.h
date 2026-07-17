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

#include "../../Module/Module.h"

#include <string>
#include <string_view>
#include <vector>

struct CommandExecutorConfig : public ModuleConfig {};

class CommandExecutor : public Module {
public:
    explicit                    CommandExecutor             (ModuleController& controller);

    void                        loop                        ()                              override;

    void                        print_help                  (std::string_view id)    const;
    void                        print_all_commands          ()                              const;
    void                        parse                       (std::string_view input_line)   const;

private:
    static std::string          trim_copy                   (std::string_view value);
    static std::string          lower_copy                  (std::string_view value);

    bool                        tokenize                    (std::string_view input,
                                                             std::vector<std::string>& out) const;
};