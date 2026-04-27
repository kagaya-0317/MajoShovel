#include "engine/App.hpp"

int main(int, char**)
{
    majo::App app;
    if (!app.initialize("Majo Shovel", 1280, 720)) {
        return 1;
    }
    app.run();
    return 0;
}
