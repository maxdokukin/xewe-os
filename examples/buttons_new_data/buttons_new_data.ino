/*
 * buttons_new_data.ino  -  Buttons config rewritten on the FlexData model.
 *
 * The OLD Buttons module sharded its config across many NVS string keys:
 *   btn_count (u8) + btn_cfg_0 .. btn_cfg_N   ("9 \"$system reboot\" pullup on_press 50")
 * Every add/remove was an O(N) read-modify-rewrite of individual keys, each
 * config re-parsed from text, and the 15-char NVS key limit loomed.
 *
 * The NEW model keeps the whole table as ONE object in RAM (the source of
 * truth) and persists it as ONE compact binary blob under a single key:
 *
 *   user (API) <-- JSON on demand --> ButtonsData (RAM) <-- blob --> NVS["bdata"]
 *
 *   ButtonsData { std::vector<ButtonCfg> buttons; }   // nested FlexData!
 *
 * add/remove/clear mutate the vector in RAM, then one save() writes one blob.
 * JSON is produced only for the API (as_json), never stored.
 *
 * Implementation of FlexData lives in flexdata.h (same folder) so the Arduino
 * .ino auto-prototype generator does not mangle the templates.
 *
 * Requires ArduinoJson (v7). Board: ESP32-S3 (FQBN esp32:esp32:esp32s3:CDCOnBoot=cdc).
 * Open Serial Monitor at 115200 baud after upload.
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "flexdata.h"


// ----------------------------------------------------------------------------
// Enum codes. Stored as small integers in the blob (compact + stable), mapped
// to/from the human words used in config strings and JSON.
// ----------------------------------------------------------------------------
enum InputMode   : uint8_t { MODE_PULLUP = 0, MODE_PULLDOWN = 1 };
enum TriggerEvent: uint8_t { EV_ON_PRESS = 0, EV_ON_RELEASE = 1, EV_ON_CHANGE = 2 };

static const char* mode_to_str(uint8_t m) {
    return m == MODE_PULLDOWN ? "pulldown" : "pullup";
}
static const char* event_to_str(uint8_t e) {
    switch (e) {
        case EV_ON_RELEASE: return "on_release";
        case EV_ON_CHANGE:  return "on_change";
        default:            return "on_press";
    }
}


// ----------------------------------------------------------------------------
// One button's config. Persisted fields are listed in fields(); the transient
// debounce state below is intentionally NOT in fields(), so it never touches
// JSON or the blob.
// ----------------------------------------------------------------------------
struct ButtonCfg : FlexData<ButtonCfg> {
    // -- persisted --
    uint32_t    b_id        = 0;
    uint8_t     pin         = 0;
    std::string command     = "";
    uint32_t    debounce_ms = 50;
    uint8_t     mode        = MODE_PULLUP;
    uint8_t     event       = EV_ON_PRESS;

    // -- transient (runtime only; not serialized) --
    uint32_t    last_debounce_time = 0;
    int         last_steady_state  = HIGH;
    int         last_flicker_state = HIGH;

    static constexpr auto fields() {
        return std::make_tuple(
            fld("b_id",        &ButtonCfg::b_id),
            fld("pin",         &ButtonCfg::pin),
            fld("command",     &ButtonCfg::command),
            fld("debounce_ms", &ButtonCfg::debounce_ms),
            fld("mode",        &ButtonCfg::mode),
            fld("event",       &ButtonCfg::event));
    }
};

// The whole table: one FlexData holding a vector of nested FlexData. This is
// what earns JSON + blob for the entire config for free.
struct ButtonsData : FlexData<ButtonsData> {
    std::vector<ButtonCfg> buttons;

    static constexpr auto fields() {
        return std::make_tuple(
            fld("buttons", &ButtonsData::buttons));
    }
};


// ----------------------------------------------------------------------------
// Small exception-free string helpers (esp32-arduino may build -fno-exceptions,
// so std::stoi/std::stoul are off the table).
// ----------------------------------------------------------------------------
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static bool parse_u32(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    char* endp = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &endp, 10);
    if (endp == s.c_str() || *endp != '\0') return false;
    out = static_cast<uint32_t>(v);
    return true;
}


// ----------------------------------------------------------------------------
// Buttons: the rewritten module. Owns one ButtonsData, persisted as one blob.
// ----------------------------------------------------------------------------
class Buttons {
public:
    // ---- persistence ----
    void load()  { data = nvs_load<ButtonsData>(kKey); rewire_hardware(); }
    bool save()  { return nvs_save(kKey, data); }

    // ---- API in: parse a config string, add/replace, return the new b_id ----
    // Format: <pin> "<command>" <pullup|pulldown> <on_press|on_release|on_change> [debounce_ms]
    // A pin already present is replaced (its b_id is kept).
    bool add_from_config(const std::string& cfg, uint32_t* out_id = nullptr) {
        ButtonCfg b;
        if (!parse_config(cfg, b)) return false;

        // replace-by-pin, else append with a fresh b_id
        ButtonCfg* existing = find_by_pin(b.pin);
        if (existing) {
            b.b_id = existing->b_id;                 // keep identity
            *existing = b;
        } else {
            b.b_id = next_id();
            data.buttons.push_back(b);
        }
        std::sort(data.buttons.begin(), data.buttons.end(),
                  [](const ButtonCfg& x, const ButtonCfg& y) { return x.b_id < y.b_id; });
        configure_pin(*find_by_pin(b.pin));
        if (out_id) *out_id = b.b_id;
        return true;
    }

    bool remove_by_pin(uint8_t pin) {
        size_t before = data.buttons.size();
        data.buttons.erase(
            std::remove_if(data.buttons.begin(), data.buttons.end(),
                           [pin](const ButtonCfg& b) { return b.pin == pin; }),
            data.buttons.end());
        return data.buttons.size() != before;
    }

    void clear_all() { data.buttons.clear(); }

    // ---- API out ----
    std::string as_json() const { return data.as_json_str(); }
    size_t      count()   const { return data.buttons.size(); }
    size_t      blob_size() const { return data.to_blob().size(); }

    void status() const {
        Serial.printf("Buttons: %u configured\n", (unsigned)data.buttons.size());
        for (const ButtonCfg& b : data.buttons) {
            Serial.printf("  #%u pin=%u %-8s %-10s deb=%ums cmd=\"%s\"\n",
                          (unsigned)b.b_id, (unsigned)b.pin,
                          mode_to_str(b.mode), event_to_str(b.event),
                          (unsigned)b.debounce_ms, b.command.c_str());
        }
    }

    // ---- runtime: debounce poll (edge -> would run b.command) ----
    void poll() {
        uint32_t now = millis();
        for (ButtonCfg& b : data.buttons) {
            int reading = digitalRead(b.pin);
            if (reading != b.last_flicker_state) {
                b.last_debounce_time = now;
                b.last_flicker_state = reading;
            }
            if (now - b.last_debounce_time >= b.debounce_ms &&
                reading != b.last_steady_state) {
                int prev = b.last_steady_state;
                b.last_steady_state = reading;
                if (edge_matches(b, prev, reading)) {
                    // real module: controller.command_executor.parse(b.command);
                    Serial.printf("[trigger] pin=%u cmd=\"%s\"\n",
                                  (unsigned)b.pin, b.command.c_str());
                }
            }
        }
    }

private:
    static constexpr const char* kKey = "bdata";
    ButtonsData data;

    ButtonCfg* find_by_pin(uint8_t pin) {
        for (ButtonCfg& b : data.buttons) if (b.pin == pin) return &b;
        return nullptr;
    }

    uint32_t next_id() const {
        uint32_t m = 0;
        for (const ButtonCfg& b : data.buttons) m = std::max(m, b.b_id);
        return m + 1;
    }

    static bool edge_matches(const ButtonCfg& b, int prev, int now) {
        bool pressed  = (b.mode == MODE_PULLUP) ? (now == LOW)  : (now == HIGH);
        bool released = (b.mode == MODE_PULLUP) ? (now == HIGH) : (now == LOW);
        switch (b.event) {
            case EV_ON_PRESS:   return pressed;
            case EV_ON_RELEASE: return released;
            case EV_ON_CHANGE:  return prev != now;
        }
        return false;
    }

    void configure_pin(ButtonCfg& b) {
        pinMode(b.pin, b.mode == MODE_PULLUP ? INPUT_PULLUP : INPUT_PULLDOWN);
        int s = digitalRead(b.pin);
        b.last_steady_state = b.last_flicker_state = s;
        b.last_debounce_time = millis();
    }

    void rewire_hardware() {
        for (ButtonCfg& b : data.buttons) configure_pin(b);
    }

    // <pin> "<command>" <mode> <event> [debounce_ms]
    static bool parse_config(const std::string& raw, ButtonCfg& out) {
        std::string s = trim(raw);
        if (s.empty()) return false;

        // pin
        size_t sp = s.find(' ');
        if (sp == std::string::npos) return false;
        uint32_t pin32;
        if (!parse_u32(trim(s.substr(0, sp)), pin32) || pin32 > 255) return false;
        out.pin = static_cast<uint8_t>(pin32);
        s = trim(s.substr(sp));

        // "command" (quoted)
        if (s.empty() || s.front() != '"') return false;
        size_t close = s.find('"', 1);
        if (close == std::string::npos) return false;
        out.command = s.substr(1, close - 1);
        s = trim(s.substr(close + 1));

        // mode
        sp = s.find(' ');
        std::string mode = trim(sp == std::string::npos ? s : s.substr(0, sp));
        if      (mode == "pullup")   out.mode = MODE_PULLUP;
        else if (mode == "pulldown") out.mode = MODE_PULLDOWN;
        else return false;
        s = (sp == std::string::npos) ? "" : trim(s.substr(sp));

        // event
        sp = s.find(' ');
        std::string ev = trim(sp == std::string::npos ? s : s.substr(0, sp));
        if      (ev == "on_press")   out.event = EV_ON_PRESS;
        else if (ev == "on_release") out.event = EV_ON_RELEASE;
        else if (ev == "on_change")  out.event = EV_ON_CHANGE;
        else return false;
        s = (sp == std::string::npos) ? "" : trim(s.substr(sp));

        // optional debounce_ms
        if (!s.empty()) {
            uint32_t d;
            if (!parse_u32(trim(s), d)) return false;
            out.debounce_ms = d;
        }
        return true;
    }
};


// ----------------------------------------------------------------------------
// Demo
// ----------------------------------------------------------------------------
static void line() { Serial.println("----------------------------------------"); }
static void check(const char* label, bool ok) {
    Serial.printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(400);
    nvs_begin();
}

void loop() {
    Serial.println();
    Serial.println("=== Buttons (FlexData) + NVS blob demo ===");
    check("nvs ready", g_nvs_ready);
    line();

    Buttons btns;

    // 1) load whatever persisted (empty on a fresh chip)
    btns.load();
    Serial.printf("loaded persisted count: %u\n", (unsigned)btns.count());
    line();

    // 2) rebuild deterministically so the demo is repeatable
    btns.clear_all();
    check("add pin 9",  btns.add_from_config(R"(9 "$system reboot" pullup on_press 50)"));
    check("add pin 10", btns.add_from_config(R"(10 "$led toggle" pulldown on_release)"));
    check("replace pin 10 (same pin)",
          btns.add_from_config(R"(10 "$led on" pulldown on_change 75)"));
    check("count == 2 (pin 10 replaced, not added)", btns.count() == 2);
    line();

    // 3) object -> JSON (API view) + blob size
    Serial.printf("as_json:   %s\n", btns.as_json().c_str());
    Serial.printf("blob size: %u bytes\n", (unsigned)btns.blob_size());
    btns.status();
    line();

    // 4) object -> blob -> NVS -> blob -> object round-trip
    check("save", btns.save());
    Buttons reloaded;
    reloaded.load();
    check("reload count == 2", reloaded.count() == 2);
    check("reload json equals saved json", reloaded.as_json() == btns.as_json());
    line();

    // 5) remove one, persist, reload
    check("remove_by_pin(10)", btns.remove_by_pin(10));
    check("save after remove", btns.save());
    Buttons afterRemove;
    afterRemove.load();
    check("reload count == 1", afterRemove.count() == 1);
    Serial.printf("remaining: %s\n", afterRemove.as_json().c_str());
    line();

    Serial.println("done.");
    delay(3000);
}
