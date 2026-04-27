#pragma once

#include "engine/Math.hpp"

namespace majo {

class Camera {
public:
    void setViewport(int width, int height);
    void follow(Vec2 target, float dt);
    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;
    Vec2 position() const { return position_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    Vec2 position_{};
    int width_ = 1280;
    int height_ = 720;
};

}
