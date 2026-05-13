#pragma once

#include <functional>

namespace majo {

struct DepthRenderEntry {
    float sortY = 0.0f;
    std::function<void()> draw;
};

}
