# SerialPort Module

Line-oriented serial I/O with raw writes, framed prints, word-wrapping, and typed input.
The module echoes incoming bytes, assembles complete lines, and provides simple UI helpers.

---

## Overview

### Printing (framed output)
```cpp
// Word-wrap by default ('w'). Use 'c' for fixed character wrap.
// Alignment applies only when message_width > 0.
void print(std::string_view message = {},
           std::string_view end = kCRLF,
           std::string_view edge_character = {},
           const char       text_align = 'l',     // 'l' | 'c' | 'r'
           const char       wrap_mode  = 'w',     // 'w' word | 'c' char
           const uint16_t   message_width = 0,    // 0 = no wrap, no pad
           const uint16_t   margin_l = 0,
           const uint16_t   margin_r = 0);
```

```cpp
// printf with framing
void printf_fmt(std::string_view edge_character,
                std::string_view end,
                const char       text_align,
                const char       wrap_mode,       // 'w' or 'c'
                const uint16_t   message_width,
                const uint16_t   margin_l,
                const uint16_t   margin_r,
                const char*      fmt, ...);

// Convenience printf that uses print() defaults.
void printf(const char* fmt, ...);
```

Behavior:
- `wrap_mode='w'` keeps whole words on a line when possible. Words longer than `message_width` are hard-split.
- `wrap_mode='c'` uses strict fixed-width chunks.
- `message_width==0` disables wrapping and padding. Content is placed as-is between edges and margins.

### Other framing helpers
```cpp
void print_separator(uint16_t total_width=50,
                     std::string_view fill="-",
                     std::string_view edge_character="+");

void print_spacer(uint16_t total_width=50,
                  std::string_view edge_character="|");

void print_header(std::string_view message,
                  uint16_t total_width=50,
                  std::string_view edge_character="|",
                  std::string_view cross_edge_character="+",
                  std::string_view sep_fill="-");
```

### Typed input getters
```cpp
std::string get_string(...);
int         get_int(...);
uint8_t     get_uint8(...);
uint16_t    get_uint16(...);
uint32_t    get_uint32(...);
float       get_float(...);
bool        get_yn(...);
```
All getters: prompt → read line (optional timeout) → parse/validate → retry up to `retry_count` (`0` = infinite) → default on failure.

---

## Quick start

```cpp
SerialPort sp(controller);
SerialPortConfig cfg;
cfg.baud_rate = 115200;
sp.begin_routines_required(cfg);

// In your main loop:
sp.loop();
```

---

## Examples

### Basic framed prints
```cpp
sp.print_header("Serial Port\\sepReady", 40);
sp.print_separator(40, "=", "+");

sp.print("left",   kCRLF, "|", 'l', 'w', 12, 1, 1);
sp.print("center", kCRLF, "|", 'c', 'w', 12, 0, 0);
sp.print("right",  kCRLF, "|", 'r', 'w', 12, 2, 0);

sp.print_spacer(40, "|");
sp.print_separator(40, "-", "+");
```

### Word-wrap vs char-wrap
```cpp
// Word-wrap keeps words together where possible.
sp.print("this is a pretty long centered text. i am curious if wrapping is working well",
         kCRLF, "|", 'c', 'w', 12, 0, 0);

// Char-wrap splits at exact width.
sp.print("this is a pretty long centered text. i am curious if wrapping is working well",
         kCRLF, "|", 'c', 'c', 12, 0, 0);
```

### printf with framing
```cpp
sp.printf_fmt("|", kCRLF, 'l', 'w', 10, 0, 0, "fmt %d %s", 7, "seven");

// Or use defaults via printf()
sp.printf("value=%d name=%s", 42, "ok");
```

### Integer input with bounds
```cpp
bool ok = false;
int n = sp.get_int("Pick a number [0..10]",
                   0, 10,
                   /*retry_count*/ 3,
                   /*timeout_ms*/ 5000,
                   /*default*/ 5,
                   std::ref(ok));
if (!ok) sp.println_raw("Fell back to default = 5.");
```

### Yes/No
```cpp
bool ok = false;
bool proceed = sp.get_yn("Continue?", 1, 3000, false, std::ref(ok));
if (!ok || !proceed) sp.println_raw("Stopped.");
```

### Read a raw line with a deadline
```cpp
std::string line;
if (sp.read_line_with_timeout(line, 2000)) {
    sp.printf_raw("you said: %s\r\n", line.c_str());
} else {
    sp.println_raw("no line within 2s");
}
```

---

## API reference

### Lifecycle
- **`SerialPort(SystemController& controller)`** — Registers the module and CLI command.
- **`void begin_routines_required(const ModuleConfig& cfg)`** — Sets TX/RX sizes, starts `Serial`, small delay.
- **`void loop()`** — Echoes bytes; `'\r'` ignored; on `'\n'` or buffer end, terminates and marks a line ready.
- **`void reset(bool verbose=false, bool do_restart=true)`** — Clears input state and calls base `Module::reset`.

### Output — raw
- **`void print_raw(std::string_view message)`** — Writes bytes as-is.
- **`void println_raw(std::string_view message)`** — Writes bytes then `CRLF`.
- **`void printf_raw(const char* fmt, ...)`** — `vsnprintf`, writes buffer. If no format specs, writes verbatim.

### Output — boxed
- **`void print(std::string_view message = {}, std::string_view end = kCRLF, std::string_view edge_character = {}, char align='l', char wrap='w', uint16_t width=0, uint16_t ml=0, uint16_t mr=0)`**  
  Splits on `'\n'`. If `width>0` then wrap by word (`wrap='w'`) or by character (`wrap='c'`). Alignment applies only when `width>0`. Writes `end` after the last fragment and `CRLF` between fragments.
- **`void printf_fmt(std::string_view edge, std::string_view end, char align, char wrap, uint16_t width, uint16_t ml, uint16_t mr, const char* fmt, ...)`**  
  Formats then delegates to `print`.
- **`void printf(const char* fmt, ...)`**  
  Formats then calls `print(msg)` using all defaults.
- **`void print_separator(uint16_t total_width=50, std::string_view fill="-", std::string_view edge="+")`**  
  Prints `edge + fill*(total_width-2*edge.size) + edge` when space allows.
- **`void print_spacer(uint16_t total_width=50, std::string_view edge="|")`**  
  Prints an empty framed line of `total_width` with `edge` characters.
- **`void print_header(std::string_view message, uint16_t total_width=50, std::string_view edge="|", std::string_view cross_edge="+", std::string_view sep_fill="-")`**  
  Prints a separator, then each `\\sep`-separated part centered within the inner width, each followed by the same separator.

### Input — lines
- **`bool has_line() const`** — True if a full line is ready.
- **`std::string read_line()`** — Returns the line and clears readiness; empty string if none.
- **`void flush_input()`** — Drains device and clears state.
- **`bool read_line_with_timeout(std::string& out, uint32_t timeout_ms)`** — Calls `loop()` until a line arrives or the timeout elapses (`0` = no timeout).
- **`void write_line_crlf(std::string_view s)`** — Writes `s` and `CRLF`.

### Input — typed getters
Shared behavior: prompt → iterate with optional timeout → validate → default on failure → optional success flag.

- **Core**
  - `template <typename Ret, typename CheckFn> Ret get_core(...)` — common loop.
  - `template <typename T> T get_integral(...)` — integer parsing and range enforcement.

- **Concrete**
  - `std::string get_string(prompt, min_length, max_length, retry_count, timeout_ms, default_value, success_sink)` — Accepts length in `[min_length..max_length]`. If `max_length==0`, uses `INPUT_BUFFER_SIZE-1`.
  - `int get_int(...)`, `uint8_t get_uint8(...)`, `uint16_t get_uint16(...)`, `uint32_t get_uint32(...)` — Base-10 parsing. Enforce `[min..max]`.
  - `float get_float(prompt, min_value, max_value, retry_count, timeout_ms, default_value, success_sink)` — Parses with `strtod`. Rejects NaN and trailing junk. Enforces range.
  - `bool get_yn(prompt, retry_count, timeout_ms, default_value, success_sink)` — Accepts `y/yes/1/true` or `n/no/0/false` (case-insensitive).

---

## Notes and limits
- Input buffer: **255 bytes**. On overflow the line is force-ended.
- `'\r'` ignored. `'\n'` commits the line.
- Wrapping counts **bytes**, not glyphs. Non-ASCII or multi-byte UTF-8 may not align visually.
- Word-wrap uses ASCII `isspace` semantics.
- `message_width==0` disables both wrapping and padding; alignment is bypassed.

---

## Changelog
- **Changed**: `print` parameter order is now `(message, end, edge, align, wrap, width, ml, mr)`.
- **Added**: `printf_fmt(edge, end, align, wrap, width, ml, mr, fmt, ...)` for framed printf.
- **Added**: `printf(fmt, ...)` convenience that uses `print()` defaults.
- **Default behavior**: When `message_width>0`, word-wrap is used unless `'c'` is specified.

---

## License
PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0. See `LICENSE` and `LICENSE-NO-AI.md` for terms.
