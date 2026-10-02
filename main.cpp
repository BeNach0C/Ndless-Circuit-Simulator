#include "CircuitGui.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    CircuitGui gui;
    if (!gui.init()) {
        return 1;
    }

    gui.run();

    return 0;
}