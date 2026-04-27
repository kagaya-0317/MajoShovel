#include "engine/Camera.hpp"

namespace majo {

void Camera::setViewport(int width, int height)
{
    width_ = width;
    height_ = height;
}

void Camera::follow(Vec2 target, float dt)
{
    position_ = lerp(position_, target, 1.0f - std::exp(-10.0f * dt));
}

Vec2 Camera::worldToScreen(Vec2 world) const
{
    return {world.x - position_.x + width_ * 0.5f, world.y - position_.y + height_ * 0.5f};
}

Vec2 Camera::screenToWorld(Vec2 screen) const
{
    return {screen.x + position_.x - width_ * 0.5f, screen.y + position_.y - height_ * 0.5f};
}

}
