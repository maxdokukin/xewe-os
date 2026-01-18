#include "src/ModuleController/ModuleController.h"


ModuleController * os = nullptr;


void setup() {
    os = new ModuleController();
    os->begin();
}


void loop() {
    os->loop();
}
