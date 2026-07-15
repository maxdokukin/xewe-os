/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/

// src/Modules/Core/DataFabric/DataFabric.h
//
// DataFabric wraps the FlexData model into a Core module. It reuses the Nvs
// module for flash (one blob per record) and exposes the on-demand JSON view for
// the API. The C++ object in RAM is the source of truth:
//
//   user (API) <-- JSON on demand --> object (RAM) <-- blob --> Nvs record
//
// Any struct passed here must derive from FlexData<Derived> (see FlexData.h).
//
// Template bodies live in DataFabric.tpp and the controller-dereferencing
// accessors in DataFabric.cpp, because ModuleController is incomplete at the
// point this header is parsed (ModuleController.h includes this header to declare
// the member). Same reason NvsTester keeps its bodies in a .cpp.
#pragma once

#include "../../Module/Module.h"
#include "FlexData.h"

#include <string>
#include <string_view>

class Nvs;


struct DataFabricConfig : public ModuleConfig {};


class DataFabric : public Module {
public:
    explicit                    DataFabric                  (ModuleController& controller);

    // object -> blob -> Nvs record. Returns false on a failed write.
    template <typename T>
    bool                        save                        (std::string_view ns,
                                                             std::string_view key,
                                                             const T& obj);

    // Nvs record -> blob -> object. Returns false (and leaves out untouched) on a
    // missing key or a corrupt/version-mismatched blob.
    template <typename T>
    bool                        load                        (std::string_view ns,
                                                             std::string_view key,
                                                             T& out);

    // object -> JSON text (API out).
    template <typename T>
    std::string                 to_json                     (const T& obj) const;

    // JSON text -> object via partial merge (API in). Only keys present in the
    // JSON are overwritten; unknown keys are ignored.
    template <typename T>
    bool                        from_json                   (std::string_view json,
                                                             T& out) const;

    void                        remove                      (std::string_view ns,
                                                             std::string_view key);

private:
    // Defined in the .cpp, where ModuleController is complete.
    Nvs&                        nvs_ref                     () const;
};

#include "DataFabric.tpp"
