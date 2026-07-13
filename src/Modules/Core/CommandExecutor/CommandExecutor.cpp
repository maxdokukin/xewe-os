/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Software/CommandExecutor/CommandExecutor.cpp

#include "CommandExecutor.h"
#include "../../Module/ModuleController.h"

#include <algorithm>
#include <cctype>
#include <span>
#include <string>
#include <vector>


CommandExecutor::CommandExecutor(ModuleController& controller)
      : Module(controller,
               /* id                  */ "cmd",
               /* name                */ "Command_Parser",
               /* description         */ "Allows to parse text from the serial port into module command calls with parameters",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}


void CommandExecutor::loop() {
    if (!controller.serial_port.has_line()) {
        return;
    }

    parse(controller.serial_port.read_line());
}

void CommandExecutor::print_help(std::string_view id) const {
    const std::string id = trim_copy(id);

    if (id.empty()) {
        print_all_commands();
        return;
    }

    Module* module = controller.get_module(id);

    if (module == nullptr) {
        controller.serial_port.printf(
            "Error: Unknown module '%s'\r\n",
            id.c_str()
        );
        return;
    }

    const auto commands = module->get_commands();

    if (commands.empty()) {
        controller.serial_port.printf(
            "Module '%s' has no CLI commands\r\n",
            id.c_str()
        );
        return;
    }

    std::vector<std::vector<std::string_view>> table_data;
    table_data.push_back({"Command", "Args", "Description", "Sample Usage"});

    std::vector<std::string> arg_counts;
    arg_counts.reserve(commands.size());

    for (const Command& command : commands) {
        if (command.name.empty() || !command.function) {
            continue;
        }

        arg_counts.push_back(std::to_string(command.arg_count));

        table_data.push_back({
            command.name,
            arg_counts.back(),
            command.description,
            command.sample_usage
        });
    }

    const std::string header =
        std::string(module->get_name()) + " Commands [" + std::string(module->get_id()) + "]";

    controller.serial_port.print_table(table_data, header);
}

void CommandExecutor::print_all_commands() const {
    controller.serial_port.print(
        "Usage:\r\n"
        "  $<id> <command> [args...]\r\n"
        "  $<id>\r\n"
        "  $<id> help\r\n"
        "  $help <id>\r\n\r\n"
        "Global command listing is not available without a ModuleController module iterator.\r\n",
        xewe::str::kCRLF
    );
}

void CommandExecutor::parse(std::string_view input_line) const {
    std::string local = trim_copy(input_line);

    if (local.empty()) {
        return;
    }

    if (local[0] != '$') {
        controller.serial_port.print("Error: commands must start with '$'; type $help", kCRLF);
        return;
    }

    local.erase(0, 1);
    local = trim_copy(local);

    if (local.empty()) {
        print_all_commands();
        return;
    }

    std::vector<std::string> tokens;

    if (!tokenize(local, tokens)) {
        return;
    }

    if (tokens.empty()) {
        print_all_commands();
        return;
    }

    if (lower_copy(tokens[0]) == "help") {
        if (tokens.size() == 1) {
            print_all_commands();
            return;
        }

        if (tokens.size() != 2) {
            controller.serial_port.print("Usage: $help <id>", kCRLF);
            return;
        }

        print_help(tokens[1]);
        return;
    }

    Module* module = controller.get_module(tokens[0]);

    if (module == nullptr) {
        controller.serial_port.printf(
            "Error: Unknown module '%s'\r\n",
            tokens[0].c_str()
        );
        return;
    }

    if (tokens.size() == 1) {
        print_help(tokens[0]);
        return;
    }

    if (lower_copy(tokens[1]) == "help") {
        print_help(tokens[0]);
        return;
    }

    std::vector<std::string> recipients;
    recipients.push_back(tokens[0]);

    std::vector<std::string> args;
    args.reserve(tokens.size() - 2);

    for (std::size_t i = 2; i < tokens.size(); ++i) {
        args.push_back(tokens[i]);
    }

    controller.send_command(
        const_cast<CommandExecutor*>(this),
        std::span<const std::string>(recipients.data(), recipients.size()),
        tokens[1],
        std::span<const std::string>(args.data(), args.size())
    );
}

std::string CommandExecutor::trim_copy(std::string_view value) {
    const auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    std::size_t begin = 0;
    std::size_t end = value.size();

    while (begin < end && is_space(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    while (end > begin && is_space(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string CommandExecutor::lower_copy(std::string_view value) {
    std::string out(value.begin(), value.end());

    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return out;
}

bool CommandExecutor::tokenize(std::string_view input,
                               std::vector<std::string>& out) const {
    out.clear();

    std::size_t pos = 0;

    while (pos < input.size()) {
        while (pos < input.size() &&
               std::isspace(static_cast<unsigned char>(input[pos])) != 0) {
            ++pos;
        }

        if (pos >= input.size()) {
            break;
        }

        std::string token;

        if (input[pos] == '"') {
            ++pos;

            bool closed = false;
            bool escape = false;

            while (pos < input.size()) {
                const char c = input[pos++];

                if (escape) {
                    token.push_back(c);
                    escape = false;
                    continue;
                }

                if (c == '\\') {
                    escape = true;
                    continue;
                }

                if (c == '"') {
                    closed = true;
                    break;
                }

                token.push_back(c);
            }

            if (escape) {
                token.push_back('\\');
            }

            if (!closed) {
                controller.serial_port.print("Error: Unterminated quote in command.", kCRLF);
                return false;
            }
        } else {
            while (pos < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[pos])) == 0) {
                token.push_back(input[pos++]);
            }
        }

        out.push_back(token);
    }

    return true;
}