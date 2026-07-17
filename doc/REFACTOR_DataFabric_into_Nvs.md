# Plan: Merge `DataFabric` into `Nvs`

## Context

`DataFabric` was crystallized as a Core module wrapping the FlexData model
(`user (API) <-- JSON --> object (RAM) <-- blob --> NVS`). In review it turned
out to be almost entirely passthrough: of its five methods, `to_json`/`from_json`
just forward to `FlexData::as_json_str()`/`update()`, `remove` forwards to
`Nvs::remove()`, and only `save`/`load` carry real logic — and that logic is a
two-line bridge from `obj.to_blob()`/`from_blob()` to `Nvs::write_blob`/
`read_blob`. So `DataFabric` is a module that mostly forwards into `Nvs`.

The fix is to fold that bridge into `Nvs` itself as `save<T>`/`load<T>` templates
and delete `DataFabric`. Objects still persist as one blob per record; JSON is
still produced on demand — but via the `FlexData` methods callers already have,
with no wrapper module in between.

**Invariant (user requirement):** the *authoring* API for a data struct is
unchanged — include one header, derive from `FlexData<Derived>`, declare a
`static constexpr fields()` tuple. Only the include path moves (FlexData.h now
lives under `Core/Nvs/`) and the persistence call becomes `nvs.save`/`nvs.load`.

## Call-site API: before → after

```cpp
// before                                   // after
data_fabric.save(ns, key, obj);            nvs.save(ns, key, obj);
data_fabric.load(ns, key, out);            nvs.load(ns, key, out);
data_fabric.to_json(obj);                  obj.as_json_str();        // FlexData
data_fabric.from_json(json, out);          out.update(json);         // FlexData
data_fabric.remove(ns, key);               nvs.remove(ns, key);      // already existed
```

## 1. Add `save`/`load` templates to `Nvs`

`src/Modules/Core/Nvs/Nvs.h` — declare next to `write_blob`/`read_blob` (no new
include; the header stays free of the ArduinoJson pull-in):

```cpp
// Typed object persistence: T must derive from FlexData<T>. save serializes the
// object to its blob and stores it as one record; load reads the record back.
// load returns false (leaving out untouched) on a missing key or a
// corrupt/version-mismatched blob.
template <typename T> bool save (std::string_view ns, std::string_view key, const T& obj);
template <typename T> bool load (std::string_view ns, std::string_view key, T& out);
```

`src/Modules/Core/Nvs/Nvs.tpp` — add `#include "FlexData.h"` at the top, then the
bodies (lifted from `DataFabric.tpp`, `nvs_ref().` → direct call):

```cpp
template <typename T>
bool Nvs::save(std::string_view ns, std::string_view key, const T& obj) {
    static_assert(std::is_base_of_v<FlexData<T>, T>, "Nvs::save<T>() requires T : FlexData<T>.");
    return write_blob(ns, key, obj.to_blob());
}
template <typename T>
bool Nvs::load(std::string_view ns, std::string_view key, T& out) {
    static_assert(std::is_base_of_v<FlexData<T>, T>, "Nvs::load<T>() requires T : FlexData<T>.");
    const std::vector<uint8_t> bytes = read_blob(ns, key);
    if (bytes.empty()) return false;
    return out.from_blob(bytes);
}
```

No cycle: `FlexData.h` depends on nothing in `Nvs`; `Nvs.tpp` is included at the
bottom of `Nvs.h`, so `FlexData.h` (ArduinoJson) lands only in TUs that use the
templates.

## 2. Relocate `FlexData.h`

Move `src/Modules/Core/DataFabric/FlexData.h` → `src/Modules/Core/Nvs/FlexData.h`
(content unchanged except the `// src/...` path comment and the header note that
currently says persistence lives in "the DataFabric module (DataFabric.h)" →
reword to "the Nvs module (`nvs.save`/`nvs.load`)"). Data-defining modules include
it as `"../Core/Nvs/FlexData.h"`; `Nvs.tpp` includes it as `"FlexData.h"`.

## 3. Delete the `DataFabric` module

Remove the whole `src/Modules/Core/DataFabric/` folder: `DataFabric.h`,
`DataFabric.tpp`, `DataFabric.cpp`, `README.md` (moves — see step 6), and the old
copy of `FlexData.h` (moved — step 2).

## 4. Unwire from `ModuleController`

`src/Modules/Module/ModuleController.h`:
- drop `#include "../Core/DataFabric/DataFabric.h"`
- drop member `DataFabric data_fabric;`
- repoint tester include (step 5)

`src/Modules/Module/ModuleController.cpp`:
- drop `, data_fabric(*this)` from the ctor init list
- drop `register_module(data_fabric);`
- drop `data_fabric.begin(DataFabricConfig{});`
- repoint the tester `owned_modules.push_back(...)` (step 5)

## 5. Repoint the tester (keep coverage)

Rename `src/Tests/DataFabric/` → `src/Tests/NvsFlex/`, class `DataFabricTester`
→ `NvsFlexTester`, config struct likewise, CLI id `df_test` → `flex_test`
(commands become `$flex_test run` / `$flex_test clean`). In the `.cpp`:
- delete the `DataFabric& fabric = controller.data_fabric;` line
- `fabric.save(...)`/`fabric.load(...)`  → `controller.nvs.save(...)`/`.load(...)`
- `fabric.to_json(a)`                    → `a.as_json_str()`
- the `update`/`set_field`/`from_blob`/`from_json` construct tests already call
  FlexData methods directly and stay as-is

The test structs (`FlatCfg`/`ChildCfg`/`ParentCfg`) and all assertions —
including the nested-vector round-trip — are unchanged.

## 6. Move + update the README

Move `Core/DataFabric/README.md` → `src/Modules/Core/Nvs/README.md` and rewrite
it around the Nvs-owned API: the flow diagram keeps `object (RAM) <-> blob <->
NVS record`, but the arrows are labeled `nvs.save`/`nvs.load` for storage and
`obj.as_json_str()`/`obj.update()` for JSON; the components table drops the
DataFabric module and lists `FlexData.h` (codec + converters) plus the `Nvs`
`save`/`load`/`write_blob`/`read_blob` methods.

## Files touched

| Action | Path |
|--------|------|
| edit   | `src/Modules/Core/Nvs/Nvs.h` (declare save/load) |
| edit   | `src/Modules/Core/Nvs/Nvs.tpp` (include FlexData.h + bodies) |
| move   | `Core/DataFabric/FlexData.h` → `Core/Nvs/FlexData.h` (+ comment reword) |
| move   | `Core/DataFabric/README.md` → `Core/Nvs/README.md` (+ rewrite) |
| delete | `src/Modules/Core/DataFabric/` (DataFabric.{h,tpp,cpp}) |
| edit   | `src/Modules/Module/ModuleController.h` |
| edit   | `src/Modules/Module/ModuleController.cpp` |
| move   | `src/Tests/DataFabric/` → `src/Tests/NvsFlex/` (rename class/id + repoint calls) |

Examples under `examples/` are standalone and left untouched.

## Verification

Standing rule: **no build without your go-ahead.** After approval:
- Compile the main sketch with the project's real FQBN
  (`...,PartitionScheme=no_ota,...`) to confirm the templates instantiate and
  `ModuleController` links without `data_fabric`.
- Upload to COM8, open serial, run `$flex_test run` — expect all PASS incl. the
  nested-vector round-trip — then `$flex_test clean` to wipe the test namespace.
