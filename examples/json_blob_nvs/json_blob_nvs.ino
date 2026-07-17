/*
 * json_blob_nvs.ino  -  standalone ESP32 sketch (Option A: static field registry)
 *
 * FlexData<Derived> gives every subclass, for free:
 *   - as_json_str / as_json_doc   object  -> JSON text / tree  (ArduinoJson)
 *   - update(json) / from_json     JSON    -> object            (partial merge / construct)
 *   - set_field / get_field        access one field BY NAME
 *   - to_blob() / from_blob()      object <-> compact binary blob (hand-rolled)
 *   - nvs_save / nvs_load          blob   <-> NVS flash
 *
 * The object in RAM is the source of truth. JSON is produced on demand.
 * Storage is the object as a binary blob (never JSON text).
 *
 * Implementation lives in flexdata.h (same folder) so the Arduino .ino
 * auto-prototype generator does not mangle the templates.
 *
 * Requires the ArduinoJson library (v7). Board: ESP32-S3.
 * Open Serial Monitor at 115200 baud after upload.
 */

#include "flexdata.h"


static void line() { Serial.println("----------------------------------------"); }

static void check(const char* label, bool ok) {
    Serial.printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(400);
    nvs_begin();
}

// Runs every few seconds so the serial monitor catches a full cycle whenever
// it attaches (the native USB-CDC port does not reset the board on open).
void loop() {
    Serial.println();
    Serial.println("=== FlexData (Option A) + NVS blob demo ===");

    check("nvs ready", g_nvs_ready);
    line();

    // 1) object -> JSON
    DataStruct1 a;
    Serial.printf("as_json_str:        %s\n", a.as_json_str().c_str());
    Serial.printf("blob size (bytes):  %u\n", (unsigned)a.to_blob().size());
    line();

    // 2) object -> blob -> NVS -> blob -> object
    check("nvs_save ds1", nvs_save("ds1", a));
    DataStruct1 b = nvs_load<DataStruct1>("ds1");
    Serial.printf("loaded:             %s\n", b.as_json_str().c_str());
    check("blob round-trip equal", a.as_json_str() == b.as_json_str());
    line();

    // 3) partial JSON merge (only listed keys change)
    b.update(R"({"var1":9,"var4":["x","y"]})");
    Serial.printf("after update:       %s\n", b.as_json_str().c_str());
    check("update var1", b.var1 == 9);                 // typed member access
    check("update var4 size", b.var4.size() == 2);
    line();

    // 4) set / get one field by name
    bool set_ok = b.set_field("var2", "hello");
    Serial.printf("get_field(var2):    %s\n", b.get_field("var2").c_str());
    check("set_field by name", set_ok && b.var2 == "hello");
    line();

    // 5) construct a different struct straight from JSON, then persist it
    DataStruct2 c = DataStruct2::from_json(R"({"var1":42,"var5":"night"})");
    Serial.printf("ds2 from_json:      %s\n", c.as_json_str().c_str());
    check("nvs_save ds2", nvs_save("ds2", c));
    DataStruct2 d = nvs_load<DataStruct2>("ds2");
    check("ds2 round-trip equal", c.as_json_str() == d.as_json_str());
    line();

    Serial.println("done.");
    delay(3000);
}
