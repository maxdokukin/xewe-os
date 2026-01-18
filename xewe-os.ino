#include "src/SystemController/SystemController.h"


SystemController * os = nullptr;


void setup() {
    os = new SystemController();
    os->begin();
}


void loop() {
    os->loop();
}
