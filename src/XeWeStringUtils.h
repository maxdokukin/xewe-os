/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <cstdarg>

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

namespace xewe::str {

inline std::string lower(std::string s) {
    std::transform(
        s.begin(), s.end(),
        s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );
    return s;
}

inline std::string capitalize(std::string s) {
    bool new_word = true;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalnum(c)) {
            s[i] = static_cast<char>(new_word ? std::toupper(c) : std::tolower(c));
            new_word = false;
        } else {
            new_word = true; // next alnum starts a new word
        }
    }
    return s;
}

inline std::string to_hex(const uint8_t* b, size_t n) {
    static const char* k = "0123456789ABCDEF";
    std::string s; s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s.push_back(k[b[i] >> 4]); s.push_back(k[b[i] & 0x0F]); }
    return s;
}
// --------------------------------------------------------------------------------------
// Small, header-only string utilities intended for embedded targets.
// Keep allocations modest and avoid exceptions.
// --------------------------------------------------------------------------------------

inline constexpr char kCRLF[] = "\r\n";

// Repeat a character N times into a std::string.
inline std::string repeat(char ch, size_t count) {
    return std::string(count, ch);
}

// Trim a single trailing '\r' (typical from CRLF when splitting on '\n').
inline void rtrim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

// Return lower-cased copy (ASCII only).
inline std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Split by a single character without allocating substrings (views).
inline std::vector<std::string_view> split_lines_sv(std::string_view text, char delim = '\n') {
    std::vector<std::string_view> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t pos = text.find(delim, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(text.substr(start));
            break;
        } else {
            out.emplace_back(text.substr(start, pos - start));
            start = pos + 1;
        }
    }
    return out;
}

// Split by a literal token (e.g. "\\sep").
inline std::vector<std::string> split_by_token(std::string_view s, std::string_view token) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(token, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        } else {
            out.emplace_back(s.substr(start, pos - start));
            start = pos + token.size();
        }
    }
    return out;
}

// Break a string into fixed-width chunks (character-based wrap).
inline std::vector<std::string> wrap_fixed(std::string_view s, size_t width) {
    std::vector<std::string> out;
    if (width == 0) {
        out.emplace_back(s);
        return out;
    }
    for (size_t i = 0; i < s.size(); i += width) {
        out.emplace_back(s.substr(i, std::min(width, s.size() - i)));
    }
    return out;
}

inline std::vector<std::string> wrap_words(std::string_view s, size_t width) {
    std::vector<std::string> out;
    if (width == 0) {
        out.emplace_back(s);
        return out;
    }
    auto is_space = [](unsigned char c){ return std::isspace(c) != 0; };

    std::string line;
    line.reserve(width);

    size_t i = 0, n = s.size();
    while (i < n) {
        // skip leading spaces between words
        while (i < n && is_space(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= n) break;

        // take next word [i, j)
        size_t j = i;
        while (j < n && !is_space(static_cast<unsigned char>(s[j]))) ++j;
        std::string_view word = s.substr(i, j - i);
        i = j;

        // place word
        if (line.empty()) {
            if (word.size() <= width) {
                line.assign(word);
            } else {
                // hard-split long word
                size_t k = 0;
                while (k < word.size()) {
                    size_t take = std::min(width, word.size() - k);
                    out.emplace_back(word.substr(k, take));
                    k += take;
                }
            }
        } else {
            if (line.size() + 1 + word.size() <= width) {
                line.push_back(' ');
                line.append(word);
            } else {
                out.emplace_back(line);
                line.clear();
                if (word.size() <= width) {
                    line.assign(word);
                } else {
                    size_t k = 0;
                    while (k < word.size()) {
                        size_t take = std::min(width, word.size() - k);
                        // first chunk goes on a fresh line; others flush immediately
                        if (line.empty()) {
                            line.assign(word.substr(k, take));
                        } else {
                            out.emplace_back(line);
                            line.assign(word.substr(k, take));
                        }
                        k += take;
                    }
                }
            }
        }
    }
    if (!line.empty()) out.emplace_back(std::move(line));
    if (out.empty()) out.emplace_back(std::string{}); // preserve empty input semantics
    return out;
}


// Align a short string within a field of "width" using 'l', 'r', or 'c'.
// If width == 0, alignment pads are zero.
inline std::string align_into(std::string_view s, size_t width, char align) {
    if (width == 0 || s.size() >= width) return std::string(s);
    size_t pad = width - s.size();
    switch (align) {
        case 'r': return repeat(' ', pad) + std::string(s);
        case 'c': {
            size_t left = pad / 2;
            size_t right = pad - left;
            return repeat(' ', left) + std::string(s) + repeat(' ', right);
        }
        default:  // 'l'
            return std::string(s) + repeat(' ', pad);
    }
}

inline std::string repeat_pattern(std::string_view pat, size_t count) {
    if (count == 0) return {};
    if (pat.empty()) return std::string(count, ' ');
    std::string out;
    out.resize(count);
    size_t p = 0;
    for (size_t i = 0; i < count; ++i) {
        out[i] = pat[p];
        p = (p + 1) % pat.size();
    }
    return out;
}

// Build a spacer/box line like: "|" + spaces + "|"
inline std::string make_spacer_line(uint16_t total_width, std::string_view edge = "|") {
    if (total_width == 0) return {};
    if (edge.empty()) return std::string(total_width, ' ');
    const size_t e = edge.size();
    if (total_width <= e) return std::string(edge.substr(0, total_width));
    if (total_width <= 2 * e) return std::string(edge.substr(0, total_width));
    const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
    std::string out;
    out.reserve(total_width);
    out.append(edge);
    out.append(inner, ' ');
    out.append(edge);
    return out;
}

// Build a rule line like: "+" + "-----" + "+"
inline std::string make_rule_line(uint16_t total_width, std::string_view fill = "-", std::string_view edge = "+") {
    if (total_width == 0) return {};
    if (edge.empty()) return repeat_pattern(fill, total_width);
    const size_t e = edge.size();
    if (total_width <= e) return std::string(edge.substr(0, total_width));
    if (total_width <= 2 * e) return std::string(edge.substr(0, total_width));
    const uint16_t inner = static_cast<uint16_t>(total_width - 2 * e);
    std::string out;
    out.reserve(total_width);
    out.append(edge);
    out += repeat_pattern(fill, inner);
    out.append(edge);
    return out;
}

// Compose a single "boxed" content line with edges and margins.
// message_width == 0 means: do NOT align/stretch; just place content between margins and edges.
inline std::string compose_box_line(std::string_view content,
                                    std::string_view edge,
                                    size_t message_width,
                                    size_t margin_l,
                                    size_t margin_r,
                                    char align) {
    const size_t edge_len = edge.size();
    const size_t field    = (message_width == 0) ? content.size() : message_width;

    std::string line;
    line.reserve(edge_len * 2 + margin_l + field + margin_r);

    if (edge_len) line.append(edge);

    if (message_width == 0) {
        line += repeat(' ', margin_l);
        line.append(content);
        line += repeat(' ', margin_r);
    } else {
        std::string payload = align_into(content, message_width, align);
        line += repeat(' ', margin_l);
        line += payload;
        line += repeat(' ', margin_r);
    }

    if (edge_len) line.append(edge);
    return line;
}

// Format a printf-style string to std::string (safe two-pass).
inline std::string vformat(const char* fmt, va_list ap) {
    if (!fmt) return {};
#if defined(__GNUC__)
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(nullptr, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed <= 0) return {};
    std::string out;
    out.resize(static_cast<size_t>(needed));
    vsnprintf(out.data(), out.size() + 1, fmt, ap);
    return out;
#else
    // Fallback: fixed buffer (avoid on embedded, but kept for completeness)
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    return std::string(buf);
#endif
}

// Numeric parsing helpers (base-10). Return false if parse fails or out-of-range.
template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
inline bool parse_int(std::string_view s, T& out) {
    // Trim spaces.
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    if (start >= end) return false;

    // Copy to buffer and ensure NUL-termination.
    std::string tmp(s.substr(start, end - start));
    char* pEnd = nullptr;

    if constexpr (std::is_signed<T>::value) {
        long long v = strtoll(tmp.c_str(), &pEnd, 10);
        if (pEnd == tmp.c_str() || *pEnd != '\0') return false;
        if (v < static_cast<long long>(std::numeric_limits<T>::min()) ||
            v > static_cast<long long>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(v);
        return true;
    } else {
        unsigned long long v = strtoull(tmp.c_str(), &pEnd, 10);
        if (pEnd == tmp.c_str() || *pEnd != '\0') return false;
        if (v > static_cast<unsigned long long>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(v);
        return true;
    }
}

} // namespace xewe::str
