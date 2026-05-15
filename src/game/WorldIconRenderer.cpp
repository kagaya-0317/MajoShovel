#include "game/WorldIconRenderer.hpp"

#include <array>
#include <algorithm>

namespace majo {

namespace {
constexpr std::string_view WorldIconDir = "assets/others/";
constexpr std::string_view WorldIconPrefix = "img_";
constexpr std::string_view WorldIconExtension = ".png";
constexpr int MoneyMediumThreshold = 50;
constexpr int MoneyLargeThreshold = 150;

constexpr std::array<WorldIconDefinition, 15> WorldIconDefinitions{{
    {WorldIconId::MoneySmall, "money_small", "お金 小額", 1},
    {WorldIconId::MoneyMedium, "money_medium", "お金 中額", 2},
    {WorldIconId::MoneyLarge, "money_large", "お金 高額", 3},
    {WorldIconId::Chest, "chest", "宝箱", 4},
    {WorldIconId::ChestOpen, "chest_open", "宝箱 開封", 5},
    {WorldIconId::RareChest, "rare_chest", "レア宝箱", 6},
    {WorldIconId::RareChestOpen, "rare_chest_open", "レア宝箱 開封", 7},
    {WorldIconId::SuperRareChest, "super_rare_chest", "超レア宝箱", 8},
    {WorldIconId::SuperRareChestOpen, "super_rare_chest_open", "超レア宝箱 開封", 9},
    {WorldIconId::Crate, "crate", "木箱", 10},
    {WorldIconId::OldWoodBuildingMaterial, "old_wood_building_material", "古木の建材", 11},
    {WorldIconId::EnhancementOre, "enhancement_ore", "強化鉱石", 12},
    {WorldIconId::MoonFragment, "moon_fragment", "月のカケラ", 13},
    {WorldIconId::ManaDrop, "mana_drop", "魔力のしずく", 14},
    {WorldIconId::WarpPoint, "warp_point", "ワープポイント", 15},
}};

const std::unordered_map<std::string, float>* gWorldIconScaleOverrides = nullptr;
}

void setWorldIconScaleOverrides(const std::unordered_map<std::string, float>* scaleByIconKey)
{
    gWorldIconScaleOverrides = scaleByIconKey;
}

std::span<const WorldIconDefinition> worldIconDefinitions()
{
    return WorldIconDefinitions;
}

const WorldIconDefinition* worldIconDefinition(WorldIconId iconId)
{
    const auto it = std::find_if(WorldIconDefinitions.begin(), WorldIconDefinitions.end(), [iconId](const WorldIconDefinition& definition) {
        return definition.iconId == iconId;
    });
    return it == WorldIconDefinitions.end() ? nullptr : &*it;
}

const WorldIconDefinition* worldIconDefinitionByKey(std::string_view key)
{
    const auto it = std::find_if(WorldIconDefinitions.begin(), WorldIconDefinitions.end(), [key](const WorldIconDefinition& definition) {
        return definition.key == key;
    });
    return it == WorldIconDefinitions.end() ? nullptr : &*it;
}

std::string_view worldIconKey(WorldIconId iconId)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    return definition == nullptr ? std::string_view{} : definition->key;
}

std::string_view worldIconDisplayName(WorldIconId iconId)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    return definition == nullptr ? std::string_view{} : definition->displayName;
}

std::string worldIconPathFromNumber(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(WorldIconDir) +
        std::string(WorldIconPrefix) +
        std::to_string(imageNumber) +
        std::string(WorldIconExtension);
}

std::string worldIconPath(WorldIconId iconId)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    return definition == nullptr ? std::string{} : worldIconPathFromNumber(definition->imageNumber);
}

WorldIconId moneyWorldIconForAmount(int amount)
{
    if (amount >= MoneyLargeThreshold) {
        return WorldIconId::MoneyLarge;
    }
    if (amount >= MoneyMediumThreshold) {
        return WorldIconId::MoneyMedium;
    }
    return WorldIconId::MoneySmall;
}

WorldIconId materialWorldIcon(MaterialType type)
{
    switch (type) {
    case MaterialType::OldWoodBuildingMaterial:
        return WorldIconId::OldWoodBuildingMaterial;
    case MaterialType::EnhancementOre:
        return WorldIconId::EnhancementOre;
    case MaterialType::MoonFragment:
        return WorldIconId::MoonFragment;
    case MaterialType::ManaDrop:
        return WorldIconId::ManaDrop;
    case MaterialType::Count:
        break;
    }
    return WorldIconId::OldWoodBuildingMaterial;
}

WorldIconId chestWorldIcon(LootChestKind kind, bool opened)
{
    switch (kind) {
    case LootChestKind::Common:
        return opened ? WorldIconId::ChestOpen : WorldIconId::Chest;
    case LootChestKind::Rare:
        return opened ? WorldIconId::RareChestOpen : WorldIconId::RareChest;
    case LootChestKind::SuperRare:
        return opened ? WorldIconId::SuperRareChestOpen : WorldIconId::SuperRareChest;
    }
    return opened ? WorldIconId::ChestOpen : WorldIconId::Chest;
}

bool drawWorldIcon(
    Renderer& renderer,
    WorldIconId iconId,
    Vec2 center,
    Vec2 maxSize,
    const WorldIconDrawOptions& options)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    if (definition == nullptr) {
        return false;
    }

    WorldIconDrawOptions scaledOptions = options;
    if (scaledOptions.applyScaleOverride && gWorldIconScaleOverrides != nullptr && !definition->key.empty()) {
        const auto it = gWorldIconScaleOverrides->find(std::string(definition->key));
        if (it != gWorldIconScaleOverrides->end()) {
            scaledOptions.scaleMultiplier *= it->second;
        }
    }
    return drawScaledImage(renderer, worldIconPathFromNumber(definition->imageNumber), center, maxSize, scaledOptions);
}

}
