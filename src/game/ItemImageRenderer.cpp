#include "game/ItemImageRenderer.hpp"

namespace majo {

ItemImageDrawOptions itemImageOptionsFromObjectOptions(const ObjectImageDrawOptions& options)
{
    ItemImageDrawOptions itemOptions;
    itemOptions.object = options;
    itemOptions.enemy.anchor = options.anchor;
    itemOptions.enemy.tint = options.tint;
    itemOptions.enemy.filter = options.filter;
    itemOptions.enemy.allowUpscale = options.allowUpscale;
    itemOptions.enemy.outlineEnabled = options.outlineEnabled;
    itemOptions.enemy.outlineColor = options.outlineColor;
    itemOptions.enemy.outlinePx = options.outlinePx;
    itemOptions.enemy.selectedOutlineEnabled = options.selectedOutlineEnabled;
    itemOptions.enemy.selectedOutlineColor = options.selectedOutlineColor;
    itemOptions.enemy.selectedOutlinePx = options.selectedOutlinePx;
    itemOptions.enemy.maskOverlayColor = options.maskOverlayColor;
    itemOptions.enemy.scaleMultiplier = options.scaleMultiplier;
    itemOptions.enemy.rotationDegrees = options.rotationDegrees;
    itemOptions.enemy.flipX = options.flipX;
    itemOptions.enemy.flipY = options.flipY;
    return itemOptions;
}

bool drawItemVisual(
    Renderer& renderer,
    const ItemVisualRef& visual,
    Vec2 center,
    Vec2 maxSize,
    const ItemImageDrawOptions& options)
{
    if (visual.source == ItemVisualSource::Enemy) {
        return drawEnemyImageIcon(
            renderer,
            visual.imageNumber,
            center,
            maxSize,
            options.enemyAnimationTimeSeconds,
            options.enemy);
    }

    if (visual.imageNumber <= 0) {
        return false;
    }

    ObjectDefinition object;
    object.id = visual.sourceId;
    object.imageNumber = visual.imageNumber;
    return drawObjectImage(renderer, object, center, maxSize, options.object);
}

bool drawItemImage(
    Renderer& renderer,
    const ItemData& item,
    Vec2 center,
    Vec2 maxSize,
    const ItemImageDrawOptions& options)
{
    const ItemVisualRef visual = effectiveItemVisualRef(item);
    return drawItemVisual(renderer, visual, center, maxSize, options);
}

bool drawItemImage(
    Renderer& renderer,
    const ItemData& item,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options)
{
    return drawItemImage(renderer, item, center, maxSize, itemImageOptionsFromObjectOptions(options));
}

}
