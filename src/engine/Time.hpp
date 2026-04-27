#pragma once

namespace majo {

class Time {
public:
    void reset();
    void tick();

    float deltaSeconds() const { return deltaSeconds_; }
    float totalSeconds() const { return totalSeconds_; }
    float fps() const { return fps_; }

private:
    double lastCounter_ = 0.0;
    float deltaSeconds_ = 1.0f / 60.0f;
    float totalSeconds_ = 0.0f;
    float fps_ = 60.0f;
};

}
