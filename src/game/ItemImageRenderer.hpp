#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/ItemModel.hpp"
#include "game/ObjectImageRenderer.hpp"

namespace majo {

struct ItemImageDrawOptions {
    ObjectImageDrawOptions object;
    EnemyImageDrawOptions enemy;
    float enemyAnimationTimeSeconds = 0.0f;
};

[[nodiscard]] ItemImageDrawOptions itemImageOptionsFromObjectOptions(const ObjectImageDrawOptions& options);
[[nodiscard]] bool drawItemVisual(
    Renderer& renderer,
    const ItemVisualRef& visual,
    Vec2 center,
    Vec2 maxSize,
    const ItemImageDrawOptions& options = {});
[[nodiscard]] bool drawItemImage(
    Renderer& renderer,
    const ItemData& item,
    Vec2 center,
    Vec2 maxSize,
    const ItemImageDrawOptions& options = {});
[[nodiscard]] bool drawItemImage(
    Renderer& renderer,
    const ItemData& item,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options);

}
