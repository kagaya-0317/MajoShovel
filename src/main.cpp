#include "engine/App.hpp"

#include <cstring>

int main(int argc, char** argv)
{
    bool testPlayMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-play") == 0) {
            testPlayMode = true;
        }
    }

    bool restart = false;
    do {
        majo::App app;
        if (!app.initialize("Majo Shovel", 1280, 720, testPlayMode)) {
            return 1;
        }
        app.run();
        restart = app.restartRequested();
    } while (restart);

    return 0;
}
