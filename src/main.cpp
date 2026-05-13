#include "engine/App.hpp"

#include <cstring>

int main(int argc, char** argv)
{
    constexpr int DevAutoReloadRebuildRestartExitCode = 85;
    bool testPlayMode = false;
    bool devAutoReloadMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-play") == 0) {
            testPlayMode = true;
        } else if (std::strcmp(argv[i], "--dev-auto-reload") == 0) {
            devAutoReloadMode = true;
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
        if (restart && devAutoReloadMode) {
            return DevAutoReloadRebuildRestartExitCode;
        }
    } while (restart);

    return 0;
}
