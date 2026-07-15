// flexdata.h  -  Option A (static field registry) implementation.
// Kept in a header so the Arduino .ino auto-prototype generator leaves the
// templates alone.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <nvs.h>
#include <nvs_flash.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>


// ----------------------------------------------------------------------------
// std::vector<T> <-> JSON (written once, shared by every struct).
// ----------------------------------------------------------------------------
namespace ArduinoJson {
template <typename T>
struct Converter<std::vector<T>> {
    static void toJson(const std::vector<T>& src, JsonVariant dst) {
        JsonArray a = dst.to<JsonArray>();
        for (const T& x : src) a.add(x);
    }
    static std::vector<T> fromJson(JsonVariantConst src) {
        std::vector<T> dst;
        for (JsonVariantConst x : src.as<JsonArrayConst>()) dst.push_back(x.as<T>());
        return dst;
    }
    static bool checkJson(JsonVariantConst src) { return src.is<JsonArrayConst>(); }
};
}  // namespace ArduinoJson


// ----------------------------------------------------------------------------
// Binary codec: little-endian, length-prefixed strings/arrays. ESP32 is LE, so
// arithmetic types are stored as their native bytes.
// ----------------------------------------------------------------------------
struct BlobWriter {
    std::vector<uint8_t> bytes;
    void raw(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        bytes.insert(bytes.end(), b, b + n);
    }
};

struct BlobReader {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;
    bool take(void* out, size_t n) {
        if (!ok || static_cast<size_t>(end - p) < n) { ok = false; return false; }
        std::memcpy(out, p, n);
        p += n;
        return true;
    }
};

// -- writers --
template <typename T>
void blob_write(BlobWriter& w, const T& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        w.raw(&v, sizeof(T));
    } else {
        static_assert(sizeof(T) == 0, "no blob_write overload for this type");
    }
}
inline void blob_write(BlobWriter& w, const std::string& s) {
    uint32_t n = static_cast<uint32_t>(s.size());
    w.raw(&n, sizeof(n));
    w.raw(s.data(), n);
}
template <typename T>
void blob_write(BlobWriter& w, const std::vector<T>& v) {
    uint32_t n = static_cast<uint32_t>(v.size());
    w.raw(&n, sizeof(n));
    for (const T& x : v) blob_write(w, x);
}

// -- readers --
template <typename T>
void blob_read(BlobReader& r, T& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        r.take(&v, sizeof(T));
    } else {
        static_assert(sizeof(T) == 0, "no blob_read overload for this type");
    }
}
inline void blob_read(BlobReader& r, std::string& s) {
    uint32_t n = 0;
    if (!r.take(&n, sizeof(n))) return;
    if (static_cast<size_t>(r.end - r.p) < n) { r.ok = false; return; }
    s.assign(reinterpret_cast<const char*>(r.p), n);
    r.p += n;
}
template <typename T>
void blob_read(BlobReader& r, std::vector<T>& v) {
    uint32_t n = 0;
    if (!r.take(&n, sizeof(n))) return;
    v.clear();
    v.reserve(n);
    for (uint32_t i = 0; i < n && r.ok; ++i) {
        T x{};
        blob_read(r, x);
        v.push_back(std::move(x));
    }
}


// ----------------------------------------------------------------------------
// Field registry helpers.
// ----------------------------------------------------------------------------
template <typename C, typename M>
struct Field { const char* name; M C::* ptr; };
template <typename C, typename M>
constexpr Field<C, M> fld(const char* n, M C::* p) { return {n, p}; }


// ----------------------------------------------------------------------------
// FlexData<Derived>: all generic methods, written once. Derived supplies a
// static constexpr fields() tuple of fld("name", &Derived::member) entries.
// ----------------------------------------------------------------------------
template <typename Derived>
struct FlexData {
    static constexpr uint8_t kBlobVersion = 1;

    // object -> JsonDocument (in-memory tree, for further manipulation)
    JsonDocument as_json_doc() const {
        JsonDocument doc;
        visit(self(), [&](const char* n, const auto& v) { doc[n] = v; });
        return doc;
    }

    // object -> JSON text
    std::string as_json_str() const {
        std::string out;
        serializeJson(as_json_doc(), out);
        return out;
    }

    // JSON text -> object (partial merge: only keys present are overwritten)
    void update(std::string_view json) {
        JsonDocument doc;
        if (deserializeJson(doc, json)) return;
        visit(self(), [&](const char* n, auto& ref) {
            using M = std::decay_t<decltype(ref)>;
            if (!doc[n].isNull()) ref = doc[n].template as<M>();
        });
    }

    // construct a fresh instance from JSON (starts from defaults, then merges)
    static Derived from_json(std::string_view json) {
        Derived d;
        d.update(json);
        return d;
    }

    // set one field by name; value is type-checked against the field's type
    template <typename V>
    bool set_field(std::string_view name, const V& value) {
        JsonDocument d;
        d.set(value);
        bool done = false;
        visit(self(), [&](const char* n, auto& ref) {
            using M = std::decay_t<decltype(ref)>;
            if (!done && name == n) { ref = d.template as<M>(); done = true; }
        });
        return done;
    }

    // read one field by name, encoded as JSON text
    std::string get_field(std::string_view name) const {
        std::string out = "null";
        visit(self(), [&](const char* n, const auto& ref) {
            if (name == n) { JsonDocument d; d.set(ref); out.clear(); serializeJson(d, out); }
        });
        return out;
    }

    // object -> compact binary blob (version byte + fields in declared order)
    std::vector<uint8_t> to_blob() const {
        BlobWriter w;
        uint8_t ver = kBlobVersion;
        w.raw(&ver, 1);
        visit(self(), [&](const char*, const auto& ref) { blob_write(w, ref); });
        return std::move(w.bytes);
    }

    // binary blob -> object; false on truncation or version mismatch
    bool from_blob(const std::vector<uint8_t>& bytes) {
        BlobReader r{bytes.data(), bytes.data() + bytes.size()};
        uint8_t ver = 0;
        if (!r.take(&ver, 1) || ver != kBlobVersion) return false;
        visit(self(), [&](const char*, auto& ref) { blob_read(r, ref); });
        return r.ok;
    }

private:
    Derived&       self()       { return static_cast<Derived&>(*this); }
    const Derived& self() const { return static_cast<const Derived&>(*this); }

    template <typename Self, typename Fn>
    static void visit(Self&& s, Fn&& fn) {
        std::apply([&](auto... f) { (fn(f.name, s.*(f.ptr)), ...); }, Derived::fields());
    }
};


// ----------------------------------------------------------------------------
// Two example structs (from the spec). Real typed members + one fields() line.
// ----------------------------------------------------------------------------
struct DataStruct1 : FlexData<DataStruct1> {
    int32_t                  var1 = 1;
    std::string              var2 = "1";
    std::vector<int32_t>     var3 = {1, 2, 3};
    std::vector<std::string> var4 = {"a", "b", "c"};

    static constexpr auto fields() {
        return std::make_tuple(
            fld("var1", &DataStruct1::var1),
            fld("var2", &DataStruct1::var2),
            fld("var3", &DataStruct1::var3),
            fld("var4", &DataStruct1::var4));
    }
};

struct DataStruct2 : FlexData<DataStruct2> {
    int32_t                  var1 = 2;
    std::string              var2 = "2";
    std::vector<int32_t>     var3 = {4, 5, 6};
    std::vector<std::string> var4 = {"d", "e", "f"};
    std::string              var5 = "slug";

    static constexpr auto fields() {
        return std::make_tuple(
            fld("var1", &DataStruct2::var1),
            fld("var2", &DataStruct2::var2),
            fld("var3", &DataStruct2::var3),
            fld("var4", &DataStruct2::var4),
            fld("var5", &DataStruct2::var5));
    }
};


// ----------------------------------------------------------------------------
// Minimal NVS blob helpers (namespace "flexdemo").
// ----------------------------------------------------------------------------
inline bool g_nvs_ready = false;

inline void nvs_begin() {
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    g_nvs_ready = (e == ESP_OK);
}

inline bool nvs_save_blob(const char* key, const std::vector<uint8_t>& data) {
    nvs_handle_t h;
    if (nvs_open("flexdemo", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_blob(h, key, data.data(), data.size());
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return e == ESP_OK;
}

inline std::vector<uint8_t> nvs_load_blob(const char* key) {
    std::vector<uint8_t> out;
    nvs_handle_t h;
    if (nvs_open("flexdemo", NVS_READONLY, &h) != ESP_OK) return out;
    size_t n = 0;
    if (nvs_get_blob(h, key, nullptr, &n) == ESP_OK && n > 0) {
        out.resize(n);
        if (nvs_get_blob(h, key, out.data(), &n) != ESP_OK) out.clear();
    }
    nvs_close(h);
    return out;
}

template <typename T>
bool nvs_save(const char* key, const T& obj) {
    return nvs_save_blob(key, obj.to_blob());
}

template <typename T>
T nvs_load(const char* key) {
    T obj;
    std::vector<uint8_t> b = nvs_load_blob(key);
    if (!b.empty()) obj.from_blob(b);
    return obj;
}
