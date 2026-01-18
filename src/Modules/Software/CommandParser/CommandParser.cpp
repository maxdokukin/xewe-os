/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/



// src/Modules/Software/CommandParser/CommandParser.cpp

#include "CommandParser.h"
#include "../../../SystemController/SystemController.h"

CommandParser::CommandParser(SystemController& controller)
      : Module(controller,
               /* module_name         */ "Command_Parser",
               /* module_description  */ "Allows to parse text from the serial port in the action function calls with parameters",
               /* nvs_key             */ "cmd",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}

void CommandParser::begin_routines_required(const ModuleConfig& cfg) {
    const auto& config = static_cast<const CommandParserConfig&>(cfg);

    command_groups.clear();

    if (config.modules && config.module_count > 0) {
        for (size_t i = 0; i < config.module_count; ++i) {
            Module* module = config.modules[i];
            if (module) {
                auto grp = module->get_commands_group();
                if (!grp.commands.empty()) {
                    command_groups.push_back(grp);
                }
            }
        }
    }
}

void CommandParser::print_help(const string& group_name) const {
    string target = group_name;
    transform(target.begin(), target.end(), target.begin(), ::tolower);

    for (size_t i = 0; i < command_groups.size(); ++i) {
        const auto& grp = command_groups[i];
        string name = grp.name;
        string group = grp.group;
        if (target == group) {
            Serial.println("----------------------------------------");
            Serial.print(grp.name.c_str());
            Serial.println(" commands:");
            for (size_t j = 0; j < grp.commands.size(); ++j) {
                const auto& cmd = grp.commands[j];
                Serial.print("    $");
                Serial.print(grp.group.c_str());
                Serial.print(" ");
                Serial.print(cmd.name.c_str());
                int pad = 20 - int(grp.group.size() + cmd.name.size());
                for (int k = 0; k < pad; ++k) Serial.print(" ");
                Serial.print("- ");
                Serial.print(cmd.description.c_str());
                Serial.print(" (args: ");
                Serial.print(cmd.arg_count);
                Serial.println(")");
                Serial.print("                          - ");
                Serial.println(cmd.sample_usage.c_str());
            }
            Serial.println("----------------------------------------");
            return;
        }
    }
    Serial.print("Error: Command group '");
    Serial.print(group_name.c_str());
    Serial.println("' not found.");
}

void CommandParser::print_all_commands() const {
    Serial.println("\n===== All Available Commands =====");
    for (size_t i = 0; i < command_groups.size(); ++i) {
        if (!command_groups[i].name.empty()) {
            print_help(command_groups[i].name);
        }
    }
    Serial.println("==================================");
}

void CommandParser::parse(string_view input_line) const {
    // Copy into mutable string
    string local(input_line.begin(), input_line.end());
    auto is_space = [](char c){ return isspace(static_cast<unsigned char>(c)); };

    // Trim whitespace
    size_t b = local.find_first_not_of(" \t\r\n"),
           e = local.find_last_not_of(" \t\r\n");
    if (b == string::npos) return;
    local = local.substr(b, e - b + 1);

    // Must start with $
    if (local.empty() || local[0] != '$') {
        Serial.println("Error: commands must start with '$'; type $help");
        return;
    }

    // Drop '$' and trim again
    local.erase(0,1);
    b = local.find_first_not_of(" \t\r\n");
    e = local.find_last_not_of(" \t\r\n");
    if (b == string::npos) local.clear();
    else                        local = local.substr(b, e - b + 1);

    // Split off group name
    size_t sp = local.find(' ');
    string group = (sp == string::npos) ? local : local.substr(0, sp);

    // Handle $help specially
    string gl = group;
    transform(gl.begin(), gl.end(), gl.begin(), ::tolower);
    if (gl == "help") {
        print_all_commands();
        return;
    }

    // Extract rest of line
    string rest = (sp == string::npos)
                       ? string()
                       : local.substr(sp+1);
    b = rest.find_first_not_of(" \t\r\n");
    e = rest.find_last_not_of(" \t\r\n");
    if (b == string::npos) rest.clear();
    else                        rest = rest.substr(b, e - b + 1);

    // Tokenize (supports quoted)
    struct Token { string value; bool quoted; };
    vector<Token> toks;
    size_t pos = 0;
    while (pos < rest.size()) {
        while (pos < rest.size() && is_space(rest[pos])) ++pos;
        if (pos >= rest.size()) break;

        bool quoted = false;
        string tok;
        if (rest[pos] == '"') {
            quoted = true;
            size_t q = rest.find('"', pos+1);
            if (q == string::npos) {
                Serial.println("Error: Unterminated quote in command.");
                return;
            }
            tok = rest.substr(pos+1, q-pos-1);
            pos = q+1;
        } else {
            size_t q = rest.find(' ', pos);
            if (q == string::npos) {
                tok = rest.substr(pos);
                pos = rest.size();
            } else {
                tok = rest.substr(pos, q-pos);
                pos = q;
            }
        }
        toks.push_back({tok, quoted});
    }

    // Separate cmd name and arguments
    string cmd;
    vector<Token> args;
    if (!toks.empty()) {
        cmd  = toks[0].value;
        args.assign(toks.begin()+1, toks.end());
    }

    // Lookup group
    for (size_t gi = 0; gi < command_groups.size(); ++gi) {
        const auto& grp = command_groups[gi];
        string name = grp.name;
        transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (gl == name) {
            // If no subcommand provided, show help for this group
            if (cmd.empty()) {
                print_help(grp.name);
                return;
            }
            // Find matching command
            string cl = cmd;
            transform(cl.begin(), cl.end(), cl.begin(), ::tolower);
            for (const auto& c : grp.commands) {
                string cn = c.name;
                transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
                if (cl == cn) {
                    if (c.arg_count != args.size()) {
                        Serial.printf(
                          "Error: '%s' expects %u args, but got %u\n",
                           c.name.c_str(),
                           unsigned(c.arg_count),
                           unsigned(args.size())
                        );
                        return;
                    }
                    // Rebuild args string
                    string rebuilt;
                    for (size_t ai = 0; ai < args.size(); ++ai) {
                        auto& tk = args[ai];
                        if (tk.quoted && tk.value.find(' ') != string::npos) {
                            rebuilt += '"'; rebuilt += tk.value; rebuilt += '"';
                        } else {
                            rebuilt += tk.value;
                        }
                        if (ai + 1 < args.size()) rebuilt += ' ';
                    }
                    // pass a string, not a string_view
                    c.function(rebuilt);
                    return;
                }
            }
            Serial.printf("Error: Unknown command '%s'; type $%s to see available commands\n",
                          cmd.c_str(), group.c_str());
            return;
        }
    }

    Serial.printf("Error: Unknown command group '%s'; type $help\n", group.c_str());
}
