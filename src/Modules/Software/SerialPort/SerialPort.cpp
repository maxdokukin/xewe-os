/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// src/Modules/SerialPort/SerialPort.cpp
#include "SerialPort.h"
#include "../../../SystemController/SystemController.h"
#include <cstring>  // strlen

SerialPort::SerialPort(SystemController& controller)
    : Module(controller,
             /* module_name         */ "Serial_Port",
             /* module_description  */ "Allows to send and receive text messages over the USB wire",
             /* nvs_key             */ "ser",
             /* requires_init_setup */ false,
             /* can_be_disabled     */ false,
             /* has_cli_cmds        */ false) {
    commands_storage.push_back({
        "test",
        "test available functions",
        string("Sample Use: $") + lower(module_name) + " test",
        0,
        [this](string_view) { test(); }
    });
}

void SerialPort::begin_routines_required(const ModuleConfig& cfg) {
    const auto& config = static_cast<const SerialPortConfig&>(cfg);
    Serial.setTxBufferSize(2048);
    Serial.setRxBufferSize(1024);
    Serial.begin(config.baud_rate);
    delay(1000);
}

void SerialPort::loop() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        yield();

        Serial.write(static_cast<uint8_t>(c));

        if (c == '\r') continue;
        if (c == '\n' || input_buffer_pos >= INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_buffer_pos] = '\0';
            line_length = input_buffer_pos;
            input_buffer_pos = 0;
            line_ready = true;
        } else {
            input_buffer[input_buffer_pos++] = c;
        }
    }
}

void SerialPort::reset (const bool verbose, const bool do_restart) {
    flush_input();
    input_buffer_pos = 0;
    line_length      = 0;
    line_ready       = false;
    Module::reset(verbose, do_restart);
}

// printers
void SerialPort::print(std::string_view message,
                       std::string_view end,
                       std::string_view edge_character,
                       const char text_align,
                       const char wrap_mode,
                       const uint16_t message_width,
                       const uint16_t margin_l,
                       const uint16_t margin_r) {
    auto lines_sv = split_lines_sv(message, '\n');
    const bool use_wrap = (message_width > 0);

    for (size_t i = 0; i < lines_sv.size(); ++i) {
        std::string base_line(lines_sv[i]);
        rtrim_cr(base_line);

        std::vector<std::string> chunks = use_wrap
            ? ((wrap_mode == 'c' || wrap_mode == 'C')
                  ? wrap_fixed(base_line, message_width)
                  : wrap_words(base_line, message_width))
            : std::vector<std::string>{base_line};

        for (size_t j = 0; j < chunks.size(); ++j) {
            const bool is_last = (i + 1 == lines_sv.size()) && (j + 1 == chunks.size());
            std::string out = compose_box_line(chunks[j], edge_character,
                                               message_width, margin_l, margin_r, text_align);
            Serial.write(reinterpret_cast<const uint8_t*>(out.data()), out.size());
            if (is_last) {
                if (!end.empty())
                    Serial.write(reinterpret_cast<const uint8_t*>(end.data()), end.size());
            } else {
                Serial.write(reinterpret_cast<const uint8_t*>(kCRLF), 2);
            }
        }
    }
}

void SerialPort::printf_fmt(std::string_view end,
                        std::string_view edge_character,
                        const char text_align,
                        const char wrap_mode,
                        const uint16_t message_width,
                        const uint16_t margin_l,
                        const uint16_t margin_r,
                        const char* fmt, ...) {
    if (!fmt) {
        print("", end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    std::string msg;
    if (needed > 0) {
        std::vector<char> buf(static_cast<size_t>(needed) + 1u);
        vsnprintf(buf.data(), buf.size(), fmt, ap2);
        msg.assign(buf.data(), static_cast<size_t>(needed));
    }
    va_end(ap2);

    print(msg, end, edge_character, text_align, wrap_mode, message_width, margin_l, margin_r);
}

void SerialPort::printf(const char* fmt, ...) {
    if (!fmt) { print(); return; }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    std::string msg;
    if (needed > 0) {
        std::vector<char> buf(static_cast<size_t>(needed) + 1u);
        vsnprintf(buf.data(), buf.size(), fmt, ap2);
        msg.assign(buf.data(), static_cast<size_t>(needed));
    }
    va_end(ap2);

    print(msg);
}


void SerialPort::print_separator(const uint16_t total_width,
                                 std::string_view fill,
                                 std::string_view edge_character) {
    std::string line;
    if (total_width == 0) {
        line.clear();
    } else if (edge_character.empty()) {
        // Full-width fill pattern
        line = repeat_pattern(fill, total_width);
    } else {
        const size_t e = edge_character.size();
        if (total_width <= e) {
            line.assign(edge_character.substr(0, total_width));
        } else if (total_width <= 2 * e) {
            // Not enough room for interior
            line.assign(edge_character.substr(0, total_width));
        } else {
            const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
            line.reserve(total_width);
            line.append(edge_character);
            line += repeat_pattern(fill, inner);
            line.append(edge_character);
        }
    }
    write_line_crlf(line);
}

void SerialPort::print_spacer(const uint16_t total_width,
                              std::string_view edge_character) {
    std::string line;
    if (total_width == 0) {
        line.clear();
    } else if (edge_character.empty()) {
        line.assign(static_cast<size_t>(total_width), ' ');
    } else {
        const size_t e = edge_character.size();
        if (total_width <= e) {
            line.assign(edge_character.substr(0, total_width));
        } else if (total_width <= 2 * e) {
            line.assign(edge_character.substr(0, total_width));
        } else {
            const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
            line.reserve(total_width);
            line.append(edge_character);
            line.append(inner, ' ');
            line.append(edge_character);
        }
    }
    write_line_crlf(line);
}

void SerialPort::print_header(std::string_view message,
                              const uint16_t total_width,
                              std::string_view edge_character,
                              std::string_view cross_edge_character,
                              std::string_view sep_fill) {
    print_separator(total_width, sep_fill, cross_edge_character);

    auto parts = split_by_token(message, "\\sep");
    const uint16_t edge_w = static_cast<uint16_t>(edge_character.size() * 2);
    const uint16_t content_width =
        (!edge_character.empty() && total_width > edge_w)
            ? static_cast<uint16_t>(total_width - edge_w)
            : total_width;

    for (auto& p : parts) {
        print(p, kCRLF, edge_character, 'c', 'w', content_width, 0, 0);
        print_separator(total_width, sep_fill, cross_edge_character);
    }
}

void SerialPort::print_table(const vector<vector<string_view>>& table,
                             string_view header_content,
                             const uint16_t max_col_width,
                             string_view edge_character,
                             string_view cross_edge_character,
                             string_view sep_fill) {
    if (table.empty()) return;

    // 1. Calculate Column Widths
    size_t num_cols = 0;
    for (const auto& row : table) num_cols = max(num_cols, row.size());

    vector<uint16_t> col_widths(num_cols, 0);

    for (const auto& row : table) {
        for (size_t c = 0; c < row.size(); ++c) {
            // Width = Length + 1 space left + 1 space right
            size_t req_width = row[c].length() + 2;
            if (req_width > max_col_width) req_width = max_col_width;

            if (req_width > col_widths[c]) {
                col_widths[c] = static_cast<uint16_t>(req_width);
            }
        }
    }

    // 2. Calculate Total Table Width
    // Formula: Edge + (Col1 + Edge) + (Col2 + Edge) ...
    size_t total_table_width = edge_character.size();
    for (auto w : col_widths) {
        total_table_width += w + edge_character.size();
    }

    // Helper: Print a complex divider (e.g., +-----+-----+)
    auto print_complex_divider = [&]() {
        string line;
        line.reserve(total_table_width);
        line.append(cross_edge_character);
        for (size_t c = 0; c < num_cols; ++c) {
            for (size_t k = 0; k < col_widths[c]; ++k) {
                if (!sep_fill.empty())
                    line += sep_fill[k % sep_fill.size()];
                else
                    line += '-';
            }
            line.append(cross_edge_character);
        }
        write_line_crlf(line);
    };

    // Helper: Wrap text (Uses existing word wrapping logic)
    auto get_wrapped_lines = [&](string_view text, uint16_t width) -> vector<string> {
        if (width <= 2) return { string(text) };
        return wrap_words(string(text), width - 2);
    };

    // 3. Print Header (if exists)
    if (!header_content.empty()) {
        // A. Print Top Solid Line
        print_separator(static_cast<uint16_t>(total_table_width), sep_fill, cross_edge_character);

        // B. Print Centered Header Text
        // We reuse the existing 'print' function to handle centering, wrapping, and boxing automatically.
        // Content width = Total - 2 * Edge
        uint16_t header_content_width = static_cast<uint16_t>(total_table_width - (edge_character.size() * 2));
        print(header_content, kCRLF, edge_character, 'c', 'w', header_content_width, 0, 0);
    }

    // 4. Print Table Body
    // If we had a header, this divider separates Header from Data.
    // If no header, this is the top of the table.
    print_complex_divider();

    for (const auto& row : table) {
        // Pre-calculate wrapped blocks
        vector<vector<string>> row_blocks;
        size_t max_row_height = 0;

        for (size_t c = 0; c < num_cols; ++c) {
            string_view entry = (c < row.size()) ? row[c] : "";
            vector<string> wrapped = get_wrapped_lines(entry, col_widths[c]);
            if (wrapped.empty()) wrapped.push_back("");
            max_row_height = max(max_row_height, wrapped.size());
            row_blocks.push_back(move(wrapped));
        }

        // Print physical lines
        for (size_t h = 0; h < max_row_height; ++h) {
            string line_out;
            line_out.reserve(total_table_width);
            line_out.append(edge_character);

            for (size_t c = 0; c < num_cols; ++c) {
                string segment = (h < row_blocks[c].size()) ? row_blocks[c][h] : "";

                // Left Margin
                line_out += ' ';
                line_out += segment;

                // Right Padding
                size_t current_len = segment.length();
                size_t target_len  = static_cast<size_t>(col_widths[c]) - 2;

                if (target_len > current_len) {
                    line_out.append(target_len - current_len, ' ');
                }

                line_out += ' '; // Right Margin
                line_out += edge_character;
            }
            write_line_crlf(line_out);
        }

        // Separator between rows
        print_complex_divider();
    }
}

// getters
string SerialPort::get_string(string_view prompt,
                                   const uint16_t min_length,
                                   const uint16_t max_length,
                                   const uint16_t retry_count,
                                   const uint32_t timeout_ms,
                                   string_view default_value,
                                   optional<reference_wrapper<bool>> success_sink) {
    const size_t min_len = static_cast<size_t>(min_length);
    const size_t max_len = (max_length == 0) ? (INPUT_BUFFER_SIZE - 1)
                                             : static_cast<size_t>(max_length);

    auto checker = [&](const string& line, string& out, const char*& err)->bool {
        if (line.size() < min_len || line.size() > max_len) {
            printf_raw("! Length must be in [%u..%u] chars.\r\n",
                       static_cast<unsigned>(min_len),
                       static_cast<unsigned>(max_len));
            err = nullptr;
            return false;
        }
        out = line;
        return true;
    };

    return get_core<string>(prompt, retry_count, timeout_ms, string(default_value),
                               success_sink, "> ", /*crlf*/false, checker);
}

int SerialPort::get_int(string_view prompt,
                        const int min_value,
                        const int max_value,
                        const uint16_t retry_count,
                        const uint32_t timeout_ms,
                        const int default_value,
                        optional<reference_wrapper<bool>> success_sink) {
    return get_integral<int>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint8_t SerialPort::get_uint8(string_view prompt,
                              const uint8_t min_value,
                              const uint8_t max_value,
                              const uint16_t retry_count,
                              const uint32_t timeout_ms,
                              const uint8_t default_value,
                              optional<reference_wrapper<bool>> success_sink) {
    return get_integral<uint8_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint16_t SerialPort::get_uint16(string_view prompt,
                                const uint16_t min_value,
                                const uint16_t max_value,
                                const uint16_t retry_count,
                                const uint32_t timeout_ms,
                                const uint16_t default_value,
                                optional<reference_wrapper<bool>> success_sink) {
    return get_integral<uint16_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

uint32_t SerialPort::get_uint32(string_view prompt,
                                const uint32_t min_value,
                                const uint32_t max_value,
                                const uint16_t retry_count,
                                const uint32_t timeout_ms,
                                const uint32_t default_value,
                                optional<reference_wrapper<bool>> success_sink) {
    return get_integral<uint32_t>(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink);
}

float SerialPort::get_float(string_view prompt,
                            const float min_value,
                            const float max_value,
                            const uint16_t retry_count,
                            const uint32_t timeout_ms,
                            const float default_value,
                            optional<reference_wrapper<bool>> success_sink) {
    float minv = min_value, maxv = max_value;
    if (minv > maxv) std::swap(minv, maxv);

    auto checker = [&](const string& line, float& out, const char*& err)->bool {
        const char* s = line.c_str();
        char* end = nullptr;
        double dv = strtod(s, &end);
        while (end && *end == ' ') ++end;
        if (s == end || (end && *end != '\0')) {
            err = "! Invalid number. Please enter a decimal value.";
            return false;
        }
        if (dv != dv) { err = "! Invalid number."; return false; } // NaN
        float v = static_cast<float>(dv);
        if (v < minv || v > maxv) {
            printf_raw("! Out of range [%g..%g].\r\n",
                       static_cast<double>(minv),
                       static_cast<double>(maxv));
            err = nullptr;
            return false;
        }
        out = v;
        return true;
    };

    return get_core<float>(prompt, retry_count, timeout_ms, default_value,
                              success_sink, "> ", /*crlf*/false, checker);
}

bool SerialPort::get_yn(string_view prompt,
                        const uint16_t retry_count,
                        const uint32_t timeout_ms,
                        const bool default_value,
                        optional<reference_wrapper<bool>> success_sink) {
    auto checker = [&](const string& line, bool& out, const char*& err)->bool {
        string low = to_lower(line);
        if (low == "y" || low == "yes" || low == "1" || low == "true")  { out = true;  return true; }
        if (low == "n" || low == "no"  || low == "0" || low == "false") { out = false; return true; }
        err = "! Please answer 'y' or 'n'.";
        return false;
    };

    return get_core<bool>(prompt, retry_count, timeout_ms, default_value,
                             success_sink, "(y/n) > ", /*crlf*/false, checker);
}

bool SerialPort::has_line() const { return line_ready; }

string SerialPort::read_line() {
    if (!line_ready) return {};
    string out(input_buffer, line_length);
    line_ready       = false;
    line_length      = 0;
    input_buffer_pos = 0;
    return out;
}

// private
void SerialPort::flush_input() {
    while (Serial.available()) {
        (void)Serial.read();
        yield();
    }
    input_buffer_pos = 0;
    line_length      = 0;
    line_ready       = false;
}

void SerialPort::print_raw(string_view message) {
    Serial.write(reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

void SerialPort::println_raw(string_view message) {
    Serial.write(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    Serial.write(reinterpret_cast<const uint8_t*>(kCRLF), 2);
}

void SerialPort::printf_raw(const char* fmt, ...) {
    if (!fmt) return;

    bool has_spec = false;
    for (const char* p = fmt; *p; ++p) {
        if (*p == '%') {
            if (*(p + 1) == '%') { ++p; continue; }
            has_spec = true;
            break;
        }
    }
    if (!has_spec) {
        size_t n = strlen(fmt);
        if (n) Serial.write(reinterpret_cast<const uint8_t*>(fmt), n);
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    if (needed <= 0) { va_end(ap2); return; }

    std::vector<char> buf(static_cast<size_t>(needed) + 1u);
    vsnprintf(buf.data(), buf.size(), fmt, ap2);
    va_end(ap2);

    Serial.write(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<size_t>(needed));
}

bool SerialPort::read_line_with_timeout(string& out,
                                        const uint32_t timeout_ms) {
    uint32_t start = millis();
    for (;;) {
        loop();
        if (has_line()) {
            out = read_line();
            return true;
        }
        if (timeout_ms != 0 && (millis() - start >= timeout_ms)) {
            return false;
        }
        yield();
    }
}

void SerialPort::write_line_crlf(string_view s) {
    Serial.write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    Serial.write(reinterpret_cast<const uint8_t*>(kCRLF), 2);
}

template <typename T>
T SerialPort::get_integral(string_view prompt,
                           const T min_value,
                           const T max_value,
                           const uint16_t retry_count,
                           const uint32_t timeout_ms,
                           const T default_value,
                           optional<reference_wrapper<bool>> success_sink) {
    T minv = min_value, maxv = max_value;
    if (minv > maxv) std::swap(minv, maxv);

    auto checker = [&](const string& line, T& out, const char*& err)->bool {
        T v{};
        if (!parse_int<T>(line, v)) {
            err = "! Invalid number. Please enter a base-10 integer.";
            return false;
        }
        if (v < minv || v > maxv) {
            printf_raw("! Out of range [%lld..%lld].\r\n",
                       static_cast<long long>(minv),
                       static_cast<long long>(maxv));
            err = nullptr;
            return false;
        }
        out = v;
        return true;
    };

    return get_core<T>(prompt, retry_count, timeout_ms, default_value, success_sink, "> ", false, checker);
}

void SerialPort::test() {
    auto banner = [&](string_view fn) {
        printf_raw("[TEST] ------------------------------------------------\r\n");
        printf_raw("[TEST] %.*s BEGIN\r\n", int(fn.size()), fn.data());
    };
    auto done = [&](string_view fn) {
        printf_raw("[TEST] %.*s END\r\n", int(fn.size()), fn.data());
        printf_raw("[TEST] ------------------------------------------------\r\n");
    };

    // RAW OUTPUT
    banner("print_raw");
    printf_raw("[TEST] in : \"raw\"\r\n");
    print_raw("raw");
    printf_raw("[TEST] out: printed\r\n");
    done("print_raw");

    banner("println_raw");
    printf_raw("[TEST] in : \"line\"\r\n");
    println_raw("line");
    printf_raw("[TEST] out: printed with CRLF\r\n");
    done("println_raw");

    banner("printf_raw");
    printf_raw("[TEST] in : fmt=\"num=%%d str=%%s\", 42, \"ok\"\r\n");
    printf_raw("num=%d str=%s\r\n", 42, "ok");
    printf_raw("[TEST] out: printed\r\n");
    done("printf_raw");

    // BOXED PRINT API
    banner("print_separator");
    printf_raw("[TEST] in : total_width=20, fill='-', edge='+'\r\n");
    print_separator(20, "-", "+");
    printf_raw("[TEST] out: printed\r\n");
    done("print_separator");

    banner("print_spacer");
    printf_raw("[TEST] in : total_width=20, edge='|'\r\n");
    print_spacer(20, "|");
    printf_raw("[TEST] out: printed\r\n");
    done("print_spacer");

    banner("print_header");
    printf_raw("[TEST] in : message=\"Header\\sepSub\", total_width=20, edge='|', sep_edge='+', sep_fill='-'\r\n");
    print_header("Header\\sepSub", 20, "|", "+", "-");
    printf_raw("[TEST] out: printed\r\n");
    done("print_header");

    banner("print");
    printf_raw("[TEST] in : message=\"left\", edge='|', align='l', wrap='w', width=10, ml=1, mr=1, end=CRLF\r\n");
    print("left",  kCRLF, "|", 'l', 'w', 10, 1, 1);
    printf_raw("[TEST] out: printed\r\n");

    banner("print()");
    printf_raw("[TEST] in : message=\"left\", edge='|', align='l', wrap='w', width=10, ml=1, mr=1, end=CRLF\r\n");
    print("bare print call");
    printf_raw("[TEST] out: printed\r\n");

    printf_raw("[TEST] in : message=\"center\", edge='|', align='c', wrap='w', width=12, ml=0, mr=0, end=CRLF\r\n");
    print("center", kCRLF, "|", 'c', 'w', 12, 0, 0);
    printf_raw("[TEST] out: printed\r\n");

    printf_raw("[TEST] in : message=\"right\", edge='|', align='r', wrap='w', width=12, ml=2, mr=0, end=CRLF\r\n");
    print("right", kCRLF, "|", 'r', 'w', 12, 2, 0);
    printf_raw("[TEST] out: printed\r\n");

    print("this is a pretty long centered text. i am curious if wrapping is working well", kCRLF,
          "|", 'c', 'w', 12, 0, 0);
    print("this is a pretty long left text. i am curious if wrapping is working well", kCRLF,
          "|", 'l', 'w', 12, 0, 0);
    print("this is a pretty long right text. i am curious if wrapping is working well", kCRLF,
          "|", 'r', 'w', 12, 0, 0);
    done("print");

    banner("printf (boxed)");
    printf_raw("[TEST] in : edge='|', align='l', width=10, ml=0, mr=0, end=CRLF, fmt=\"fmt %%d %%s\", 7, \"seven\"\r\n");
    printf_fmt("|", kCRLF, 'l', 'w', 10, 0, 0, "fmt %d %s", 7, "seven");
    printf_raw("[TEST] out: printed\r\n");
    done("printf (boxed)");

    // INPUT AND LINE UTILITIES — no synthetic input
    banner("has_line/read_line");
    printf_raw("[TEST] in : none; expect no line\r\n");
    flush_input();
    bool hl = has_line();
    printf_raw("[TEST] out: has_line=%s\r\n", hl ? "true" : "false");
    string got = read_line();
    printf_raw("[TEST] out: read_line=\"%s\"\r\n", got.c_str());
    printf_raw("[TEST] out: post: has_line=%s\r\n", has_line() ? "true" : "false");
    done("has_line/read_line");

    banner("flush_input");
    printf_raw("[TEST] in : call flush_input()\r\n");
    flush_input();
    printf_raw("[TEST] out: cleared\r\n");
    done("flush_input");

    banner("read_line_with_timeout");
    printf_raw("[TEST] in : timeout_ms=10; expect timeout\r\n");
    string outLine;
    bool ok = read_line_with_timeout(outLine, 10);
    printf_raw("[TEST] out: ok=%s, line=\"%s\"\r\n", ok ? "true" : "false", outLine.c_str());
    done("read_line_with_timeout");

    banner("write_line_crlf");
    printf_raw("[TEST] in : \"EOL test\"\r\n");
    write_line_crlf("EOL test");
    printf_raw("[TEST] out: printed\r\n");
    done("write_line_crlf");

    // GETTERS — default paths only, short timeouts
    banner("get_int");
    printf_raw("[TEST] in : prompt=\"int?\", range=[0..100], retries=1, timeout=0, default=5\r\n");
    flush_input();
    bool succ = false;
    int v = get_int("int?", 0, 100, 1, 0, 5, ref(succ));
    printf_raw("[TEST] out: value=%d, success=%s\r\n", v, succ ? "true" : "false");
    done("get_int");

    banner("get_uint8");
    printf_raw("[TEST] in : prompt=\"u8?\", range=[0..255], retries=1, timeout=0, default=9\r\n");
    flush_input();
    succ = false;
    uint8_t v8 = get_uint8("u8?", 0, 255, 1, 0, 9, ref(succ));
    printf_raw("[TEST] out: value=%u, success=%s\r\n", static_cast<unsigned>(v8), succ ? "true" : "false");
    done("get_uint8");

    banner("get_uint16");
    printf_raw("[TEST] in : prompt=\"u16?\", range=[0..10000], retries=1, timeout=0, default=1\r\n");
    flush_input();
    succ = false;
    uint16_t v16 = get_uint16("u16?", 0, 10000, 1, 0, 1, ref(succ));
    printf_raw("[TEST] out: value=%u, success=%s\r\n", static_cast<unsigned>(v16), succ ? "true" : "false");
    done("get_uint16");

    banner("get_uint32");
    printf_raw("[TEST] in : prompt=\"u32?\", range=[0..1000000], retries=1, timeout=0, default=2\r\n");
    flush_input();
    succ = false;
    uint32_t v32 = get_uint32("u32?", 0u, 1000000u, 1, 0, 2u, ref(succ));
    printf_raw("[TEST] out: value=%lu, success=%s\r\n", static_cast<unsigned long>(v32), succ ? "true" : "false");
    done("get_uint32");

    banner("get_float");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    float vf = get_float("float?", -10.5f, 10.5f, 1, 0, 3.14f, ref(succ));
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    banner("get_string");
    printf_raw("[TEST] in : prompt=\"str?\", len=[3..10], retries=1, timeout=0, default=\"xx\"\r\n");
    flush_input();
    succ = false;
    string s = get_string("str?", 3, 10, 1, 0, "xx", ref(succ));
    printf_raw("[TEST] out: value=\"%s\", success=%s\r\n", s.c_str(), succ ? "true" : "false");
    done("get_string");

    banner("get_yn");
    printf_raw("[TEST] in : prompt=\"yn?\", retries=1, timeout=0, default=false\r\n");
    flush_input();
    succ = false;
    bool b = get_yn("yn?", 1, 0, false, ref(succ));
    printf_raw("[TEST] out: value=%s, success=%s\r\n", b ? "true" : "false", succ ? "true" : "false");
    done("get_yn");

    banner("get_float (5 sec timeout)");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    vf = get_float("float?", -10.5f, 10.5f, 1, 5999, 3.14f, ref(succ));
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    banner("get_float (inf retries)");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    vf = get_float("float?", -1.5f, 1.5f);
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    // SUMMARY
    banner("summary");
    printf_raw("[TEST] in : none\r\n");
    print_separator(16, "=", "+");
    print("done", kCRLF, "|", 'c', 'w', 10, 0, 0);
    print_separator(16, "=", "+");
    printf_raw("[TEST] out: printed\r\n");
    done("summary");
}
