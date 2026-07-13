```cpp
// Nvs supported types
// Pattern: write<T>(namespace, key, value) then read<T>(namespace, key, default_value).
// Notes:
// - std::string_view and const char* are write input types only; read them back as std::string.
// - String is available only when Arduino.h is available.
// - Integral support is width-based: signed/unsigned 1, 2, 4, and 8 byte integer types.
// - Floating-point values are stored as blobs using sizeof(T).

#include <cstdint>
#include <string>
#include <string_view>

#if __has_include(<Arduino.h>)
#include <Arduino.h>
#endif

void nvs_supported_type_examples(Nvs& nvs) {
    // bool
    nvs.write<bool>("demo", "bool", true);
    bool bool_value = nvs.read<bool>("demo", "bool", false);

    // int8_t
    nvs.write<int8_t>("demo", "int8", static_cast<int8_t>(-8));
    int8_t int8_value = nvs.read<int8_t>("demo", "int8", 0);

    // int16_t
    nvs.write<int16_t>("demo", "int16", static_cast<int16_t>(-16));
    int16_t int16_value = nvs.read<int16_t>("demo", "int16", 0);

    // int32_t
    nvs.write<int32_t>("demo", "int32", static_cast<int32_t>(-32));
    int32_t int32_value = nvs.read<int32_t>("demo", "int32", 0);

    // int64_t
    nvs.write<int64_t>("demo", "int64", static_cast<int64_t>(-64));
    int64_t int64_value = nvs.read<int64_t>("demo", "int64", 0);

    // uint8_t
    nvs.write<uint8_t>("demo", "uint8", static_cast<uint8_t>(8));
    uint8_t uint8_value = nvs.read<uint8_t>("demo", "uint8", 0);

    // uint16_t
    nvs.write<uint16_t>("demo", "uint16", static_cast<uint16_t>(16));
    uint16_t uint16_value = nvs.read<uint16_t>("demo", "uint16", 0);

    // uint32_t
    nvs.write<uint32_t>("demo", "uint32", static_cast<uint32_t>(32));
    uint32_t uint32_value = nvs.read<uint32_t>("demo", "uint32", 0);

    // uint64_t
    nvs.write<uint64_t>("demo", "uint64", static_cast<uint64_t>(64));
    uint64_t uint64_value = nvs.read<uint64_t>("demo", "uint64", 0);

    // float
    nvs.write<float>("demo", "float", 3.14f);
    float float_value = nvs.read<float>("demo", "float", 0.0f);

    // double
    nvs.write<double>("demo", "double", 6.28);
    double double_value = nvs.read<double>("demo", "double", 0.0);

    // long double
    nvs.write<long double>("demo", "long_double", static_cast<long double>(9.42));
    long double long_double_value = nvs.read<long double>("demo", "long_double", 0.0L);

    // std::string
    nvs.write<std::string>("demo", "std_string", std::string("hello"));
    std::string std_string_value = nvs.read<std::string>("demo", "std_string", "");

    // String
    nvs.write<String>("demo", "string", String("hello"));
    String string_value = nvs.read<String>("demo", "string", String(""));

    // std::string_view write, std::string read
    nvs.write<std::string_view>("demo", "string_view", std::string_view("hello"));
    std::string string_view_value = nvs.read<std::string>("demo", "string_view", "");

    // const char* write, std::string read
    nvs.write<const char*>("demo", "c_string", "hello");
    std::string c_string_value = nvs.read<std::string>("demo", "c_string", "");

    // Keep example variables used.
    (void)bool_value;
    (void)int8_value;
    (void)int16_value;
    (void)int32_value;
    (void)int64_value;
    (void)uint8_value;
    (void)uint16_value;
    (void)uint32_value;
    (void)uint64_value;
    (void)float_value;
    (void)double_value;
    (void)long_double_value;
    (void)std_string_value;
#if __has_include(<Arduino.h>)
    (void)string_value;
#endif
    (void)string_view_value;
    (void)c_string_value;
}
```
