#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/ItemModel.hpp"
#include "game/ScaledImageRenderer.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace majo {

enum class WorldIconId {
    MoneySmall,
    MoneyMedium,
    MoneyLarge,
    Chest,
    ChestOpen,
    RareChest,
    RareChestOpen,
    SuperRareChest,
    SuperRareChestOpen,
    Crate,
    OldWoodBuildingMaterial,
    EnhancementOre,
    MoonFragment,
    ManaDrop,
    WarpPoint,
};

struct WorldIconDefinition {
    WorldIconId iconId = WorldIconId::MoneySmall;
    std::string_view key;
    std::string_view displayName;
    int imageNumber = 0;
};

struct WorldIconDrawOptions : ScaledImageDrawOptions {
    bool applyScaleOverride = true;
};

void setWorldIconScaleOverrides(const std::unordered_map<std::string, float>* scaleByIconKey);

[[nodiscard]] std::span<const WorldIconDefinition> worldIconDefinitions();
[[nodiscard]] const WorldIconDefinition* worldIconDefinition(WorldIconId iconId);
[[nodiscard]] const WorldIconDefinition* worldIconDefinitionByKey(std::string_view key);
[[nodiscard]] std::string_view worldIconKey(WorldIconId iconId);
[[nodiscard]] std::string_view worldIconDisplayName(WorldIconId iconId);
[[nodiscard]] std::string worldIconPathFromNumber(int imageNumber);
[[nodiscard]] std::string worldIconPath(WorldIconId iconId);
[[nodiscard]] WorldIconId moneyWorldIconForAmount(int amount);
[[nodiscard]] WorldIconId materialWorldIcon(MaterialType type);
[[nodiscard]] WorldIconId chestWorldIcon(LootChestKind kind, bool opened);
[[nodiscard]] bool drawWorldIcon(
    Renderer& renderer,
    WorldIconId iconId,
    Vec2 center,
    Vec2 maxSize,
    const WorldIconDrawOptions& options = {});

}
