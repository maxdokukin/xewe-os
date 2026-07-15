# DataFabric

DataFabric persists typed C++ objects as compact binary blobs in NVS and serves
them as JSON on demand. Any struct that derives from `FlexData<Derived>` gets
object⇄JSON and object⇄blob conversion for free — including nested structs and
vectors of structs — so a module can keep one plain C++ object in RAM and let
DataFabric handle both flash storage and the API view.

## Data model

The C++ object in RAM is the single source of truth. JSON is produced on demand
for the API; the blob is produced on demand for storage. Neither is kept in sync
continuously — they are derived views.

```
                       to_json() │ from_json()
        ┌──────────────┐  (on demand)  ┌────────────────────┐   save()  ┌───────────────┐
        │              │ ─────JSON────► │                    │ ──blob──► │               │
        │  user / API  │                │   object in RAM    │           │  NVS record   │
        │              │ ◄────JSON───── │ (source of truth)  │ ◄─blob─── │  (flash)      │
        └──────────────┘                └────────────────────┘   load()  └───────────────┘
                       from_json()│ to_json()
```

* **JSON** (text) is the wire format for the API. It is generated only when
  asked (`to_json`) and parsed back with a partial merge (`from_json` /
  `update` — only keys present in the JSON are overwritten).
* **Blob** (binary) is the storage format: little-endian, length-prefixed, with
  a one-byte version at the top level. Much smaller than JSON and cheap to
  read/write.
* **NVS** stores exactly one blob per record (`ns` + `key`), reusing the `Nvs`
  core module rather than duplicating flash code.

## Components

| File             | Role                                                                                     |
|------------------|------------------------------------------------------------------------------------------|
| `FlexData.h`     | Pure, controller-free CRTP base. The blob codec + ArduinoJson converters live here.      |
| `DataFabric.h`   | The `Module`. Public template API (`save`/`load`/`to_json`/`from_json`) + `remove`.      |
| `DataFabric.tpp` | Template bodies — forward to `Nvs::write_blob`/`read_blob` and the `FlexData` methods.    |
| `DataFabric.cpp` | Ctor + `nvs_ref()` accessor (dereferences `controller.nvs`, needs a complete controller). |

`FlexData.h` has no dependency on `Nvs`, `SerialPort`, or `ModuleController`, so
data-defining modules can include it just to declare their structs. Only
`DataFabric` touches the controller.

> The `.tpp`/`.cpp` split exists because `ModuleController` is an incomplete type
> when `DataFabric.h` is parsed (the controller header includes this one), so the
> `controller.nvs` dereference cannot live in the header. This mirrors the sibling
> `Nvs` module.

## Authoring a data struct

Derive from `FlexData<Derived>`, give it real typed members, and register them in
one `static constexpr fields()` tuple. Transient members simply stay out of
`fields()` — they are never serialized or persisted.

```cpp
#include "../Core/DataFabric/FlexData.h"

struct LedCfg : FlexData<LedCfg> {
    uint8_t     brightness = 128;
    std::string name       = "strip";

    static constexpr auto fields() {
        return std::make_tuple(
            fld("brightness", &LedCfg::brightness),
            fld("name",       &LedCfg::name));
    }
};
```

That is all the struct needs. It now supports the full API below.

## The three flows

```
        ┌──────────────────────────── object in RAM (LedCfg) ────────────────────────────┐
        │                                                                                 │
  JSON  │  fabric.to_json(cfg)        ──►  {"brightness":128,"name":"strip"}              │
        │  fabric.from_json(json,cfg) ◄──  partial merge (only present keys overwritten)  │
        │                                                                                 │
  BLOB  │  fabric.save(ns,key,cfg)    ──►  [ver][u8][len|bytes...]  ──►  NVS record       │
        │  fabric.load(ns,key,cfg)    ◄──  NVS record  ──►  [ver][u8][len|bytes...]       │
        │                                                                                 │
  FIELD │  cfg.set_field("name","x")  ──►  one field by name                              │
        │  cfg.get_field("name")      ◄──  one field as JSON text                         │
        └─────────────────────────────────────────────────────────────────────────────────┘
```

### JSON (API in / out)

```cpp
std::string json = fabric.to_json(cfg);      // object -> JSON text
fabric.from_json(R"({"brightness":255})", cfg); // JSON -> object (partial merge)
```

### Blob + NVS (persistence)

```cpp
fabric.save("led", "cfg", cfg);   // object -> blob -> NVS record
LedCfg restored;
if (fabric.load("led", "cfg", restored)) { /* ... */ }  // NVS -> blob -> object
fabric.remove("led", "cfg");      // delete the record
```

`load` returns `false` (and leaves the target untouched) on a missing key or a
corrupt / version-mismatched blob.

### One field by name

```cpp
cfg.set_field("name", "kitchen");          // returns false if no such field
std::string v = cfg.get_field("name");     // "\"kitchen\""  (JSON-encoded)
```

## Nested structs

A field may itself be a `FlexData` struct, or a `std::vector` of them. The codec
and the JSON converters recurse automatically, so a parent record persists and
serializes its whole tree in one blob.

```cpp
struct ChildCfg : FlexData<ChildCfg> {
    uint8_t     pin     = 0;
    std::string command = "";
    static constexpr auto fields() {
        return std::make_tuple(fld("pin", &ChildCfg::pin),
                               fld("command", &ChildCfg::command));
    }
};

struct ParentCfg : FlexData<ParentCfg> {
    uint32_t              revision = 0;
    std::vector<ChildCfg> children;          // vector of nested records
    static constexpr auto fields() {
        return std::make_tuple(fld("revision", &ParentCfg::revision),
                               fld("children", &ParentCfg::children));
    }
};

fabric.save("app", "parent", parent);        // whole tree -> one blob
```

## Blob format

```
 top level ── to_blob()
 ┌──────┬────────────────────────── fields, in fields() order ──────────────────────────┐
 │ ver  │  arithmetic: raw LE bytes                                                       │
 │ (u8) │  std::string: [u32 length][raw bytes]                                           │
 │      │  std::vector<T>: [u32 count][element][element]...                               │
 │      │  nested FlexData: its fields written in order (no per-element version byte)      │
 └──────┴─────────────────────────────────────────────────────────────────────────────┘
```

Only the top-level record carries the version byte; nested structs are written
field-by-field. Bump `FlexData::kBlobVersion` when a struct's on-disk layout
changes — `from_blob` rejects a mismatched version rather than misreading old
data.

## Reusing DataFabric

Because `DataFabric` is a registered core module (`controller.data_fabric`), any
other module can persist its config in a couple of lines:

```cpp
LedCfg cfg;
if (!controller.data_fabric.load("led", "cfg", cfg)) {
    // first boot: defaults already in cfg, save them
    controller.data_fabric.save("led", "cfg", cfg);
}
```

See `src/Tests/DataFabric/DataFabricTester.cpp` for a full round-trip test suite,
including the nested-vector path.
