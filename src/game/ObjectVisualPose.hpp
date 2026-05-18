#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "game/ObjectImageRenderer.hpp"

namespace majo {

[[nodiscard]] ObjectImageDrawOptions objectGroundImageOptions(
    const ObjectDefinition& object,
    const ObjectImageDrawOptions& base = {});

[[nodiscard]] ObjectImageDrawOptions objectRingImageOptions(
    const ObjectDefinition& object,
    Vec2 outward,
    Vec2 forward,
    float totalSeconds,
    const ObjectImageDrawOptions& base = {});

}
