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
// src/Tests/DataFabric/DataFabricTester.cpp
#include "DataFabricTester.h"

#include "../../Modules/Module/ModuleController.h"

#include <string>
#include <vector>


// ----------------------------------------------------------------------------
// Test structs. Authored the way a real module would author them, in a .h/.cpp
// that includes FlexData.h (pulled in transitively via ModuleController.h).
// ----------------------------------------------------------------------------
namespace {

struct FlatCfg : FlexData<FlatCfg> {
    int32_t                  var1 = 1;
    std::string              var2 = "one";
    std::vector<int32_t>     var3 = {1, 2, 3};
    std::vector<std::string> var4 = {"a", "b", "c"};

    static constexpr auto fields() {
        return std::make_tuple(
            fld("var1", &FlatCfg::var1),
            fld("var2", &FlatCfg::var2),
            fld("var3", &FlatCfg::var3),
            fld("var4", &FlatCfg::var4));
    }
};

struct ChildCfg : FlexData<ChildCfg> {
    uint8_t     pin     = 0;
    std::string command = "";

    static constexpr auto fields() {
        return std::make_tuple(
            fld("pin",     &ChildCfg::pin),
            fld("command", &ChildCfg::command));
    }
};

struct ParentCfg : FlexData<ParentCfg> {
    uint32_t               revision = 0;
    std::vector<ChildCfg>  children;

    static constexpr auto fields() {
        return std::make_tuple(
            fld("revision", &ParentCfg::revision),
            fld("children", &ParentCfg::children));
    }
};

}  // namespace


DataFabricTester::DataFabricTester(ModuleController& controller)
      : Module(controller,
               /* id                  */ "df_test",
               /* name                */ "DataFabricTester",
               /* description         */ "Round-trip tests for the DataFabric module",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ true) {

    commands_storage.push_back(Command{
        "run",
        "Run the full DataFabric test suite",
        std::string("$") + id + " run",
        0,
        [this](std::span<const std::string>) { run_tests(); }
    });

    commands_storage.push_back(Command{
        "clean",
        "Wipe the DataFabric test namespace",
        std::string("$") + id + " clean",
        0,
        [this](std::span<const std::string>) {
            clean();
            this->controller.serial_port.print("Test namespace cleared");
        }
    });
}


void DataFabricTester::report(const char* label, bool ok) {
    if (ok) {
        m_pass++;
        controller.serial_port.printf("  [PASS] %s", label);
    } else {
        m_fail++;
        controller.serial_port.printf("  [FAIL] %s", label);
    }
}


void DataFabricTester::clean() {
    controller.nvs.reset_ns(kNs);
}


void DataFabricTester::run_tests() {
    m_pass = 0;
    m_fail = 0;

    controller.serial_port.print_header("DataFabric Test Suite");
    clean();

    DataFabric& fabric = controller.data_fabric;

    // ---- object -> JSON ---------------------------------------------------
    {
        FlatCfg a;
        const std::string json = fabric.to_json(a);
        report("to_json non-empty", json.size() > 2 && json.front() == '{');
    }

    // ---- to_blob / from_blob round-trip (flat) ----------------------------
    {
        FlatCfg a;
        a.var1 = 42;
        a.var2 = "hello world";
        a.var3 = {9, 8, 7, 6};
        a.var4 = {"x", "y"};

        FlatCfg b;
        const bool ok = b.from_blob(a.to_blob());
        report("blob round-trip (flat)", ok && a.as_json_str() == b.as_json_str());
    }

    // ---- save -> load via DataFabric + Nvs (flat) -------------------------
    {
        FlatCfg a;
        a.var1 = 123;
        a.var2 = "persisted";
        a.var3 = {5};
        a.var4 = {"p", "q", "r"};

        const bool saved = fabric.save(kNs, "flat", a);

        FlatCfg b;
        const bool loaded = fabric.load(kNs, "flat", b);
        report("save+load round-trip (flat)",
               saved && loaded && a.as_json_str() == b.as_json_str());
    }

    // ---- load on a missing key returns false ------------------------------
    {
        FlatCfg b;
        const bool loaded = fabric.load(kNs, "no_such_key", b);
        report("load missing key -> false", !loaded);
    }

    // ---- partial JSON merge ----------------------------------------------
    {
        FlatCfg a;                          // var1=1, var2="one"
        a.update(R"({"var1":99,"var4":["z"]})");
        report("partial update",
               a.var1 == 99 && a.var2 == "one" && a.var4.size() == 1 && a.var4[0] == "z");
    }

    // ---- set_field / get_field by name -----------------------------------
    {
        FlatCfg a;
        const bool set_ok = a.set_field("var2", "renamed");
        const std::string got = a.get_field("var2");
        report("set_field / get_field",
               set_ok && a.var2 == "renamed" && got == "\"renamed\"");
    }

    // ---- construct from JSON ---------------------------------------------
    {
        FlatCfg a = FlatCfg::from_json(R"({"var1":7,"var2":"built"})");
        report("from_json construct", a.var1 == 7 && a.var2 == "built");
    }

    // ---- NESTED: save -> load a vector of FlexData ------------------------
    {
        ParentCfg p;
        p.revision = 3;
        p.children.push_back(ChildCfg{});           // defaults
        p.children.back().pin = 9;
        p.children.back().command = "$system reboot";
        p.children.push_back(ChildCfg{});
        p.children.back().pin = 10;
        p.children.back().command = "$led toggle";

        const bool saved = fabric.save(kNs, "nested", p);

        ParentCfg q;
        const bool loaded = fabric.load(kNs, "nested", q);

        const bool structural =
            loaded &&
            q.revision == 3 &&
            q.children.size() == 2 &&
            q.children[0].pin == 9 &&
            q.children[1].command == "$led toggle";

        report("save+load round-trip (nested vector)",
               saved && structural && p.as_json_str() == q.as_json_str());
    }

    // Leave no test data behind.
    clean();

    controller.serial_port.printf("DataFabric tests done: %d passed, %d failed",
                                  m_pass, m_fail);
}
