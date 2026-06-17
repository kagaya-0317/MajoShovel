#include "game/ItemImageRenderer.hpp"

#include "game/Enemy.hpp"

namespace majo {

namespace {

Color multiplyRgb(Color color, Color multiplier)
{
    color.r = static_cast<unsigned char>(static_cast<int>(color.r) * static_cast<int>(multiplier.r) / 255);
    color.g = static_cast<unsigned char>(static_cast<int>(color.g) * static_cast<int>(multiplier.g) / 255);
    color.b = static_cast<unsigned char>(static_cast<int>(color.b) * static_cast<int>(multiplier.b) / 255);
    return color;
}

EnemyImageDrawOptions applyEnemyVariantVisual(
    const ItemVisualRef& visual,
    EnemyImageDrawOptions options)
{
    if (visual.enemyVariantLevelBonus >= EnemyAbyssVariantLevelBonus) {
        options.tint = multiplyRgb(options.tint, {118, 118, 142, 255});
    } else if (visual.enemyVariantLevelBonus >= EnemyDeepVariantLevelBonus) {
        options.tint = multiplyRgb(options.tint, {170, 170, 192, 255});
    }
    return options;
}

}

ItemImageDrawOptions itemImageOptionsFromObjectOptions(const ObjectImageDrawOptions& options)
{
    ItemImageDrawOptions itemOptions;
    itemOptions.object = options;
    itemOptions.enemy.anchor = options.anchor;
    itemOptions.enemy.tint = options.tint;
    itemOptions.enemy.filter = options.filter;
    itemOptions.enemy.allowUpscale = options.allowUpscale;
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
        EnemyImageDrawOptions enemyOptions = applyEnemyVariantVisual(visual, options.enemy);
        return drawEnemyImageIcon(
            renderer,
            visual.imageNumber,
            center,
            maxSize,
            options.enemyAnimationTimeSeconds,
            enemyOptions);
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
