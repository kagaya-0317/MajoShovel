#include "game/WorldIconRenderer.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string>

namespace majo {

namespace {
constexpr std::string_view WorldIconDir = "assets/others/";
constexpr std::string_view WorldIconPrefix = "img_";
constexpr std::string_view WorldIconExtension = ".png";
constexpr int MoneyMediumThreshold = 50;
constexpr int MoneyLargeThreshold = 150;

constexpr std::array<WorldIconDefinition, 35> WorldIconDefinitions{{
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
    {WorldIconId::WarpGuidePedestal, "warp_guide_pedestal", "ワープ案内図 台座", 16},
    {WorldIconId::WarpGuideMap, "warp_guide_map", "ワープ案内図 地図", 17},
    {WorldIconId::NestHole, "nest_hole", "巣穴", 18},
    {WorldIconId::GlowingRock, "glowing_rock", "光る岩", 19},
    {WorldIconId::ElectricReceiver, "electric_receiver", "受電石", 20},
    {WorldIconId::ElectricReceiverPowered, "electric_receiver_powered", "受電石 通電中", 24},
    {WorldIconId::LostBaggage, "lost_baggage", "荷物", 21},
    {WorldIconId::Campfire, "campfire", "焚き火", 22},
    {WorldIconId::HeavyRock, "heavy_rock", "重い岩", 23},
    {WorldIconId::UiScreenSettings, "ui_screen_settings", "画面設定", 25},
    {WorldIconId::UiVolume, "ui_volume", "音量", 26},
    {WorldIconId::UiGamepad, "ui_gamepad", "操作", 27},
    {WorldIconId::UiStatus, "ui_status", "ステータス", 28},
    {WorldIconId::UiBackpack, "ui_backpack", "リュック", 29},
    {WorldIconId::UiOptions, "ui_options", "オプション", 30},
    {WorldIconId::UiQuitGame, "ui_quit_game", "ゲーム終了", 31},
    {WorldIconId::UiStorageChest, "ui_storage_chest", "収納箱", 32},
    {WorldIconId::UiRing0, "ui_ring_0", "リング0", 33},
    {WorldIconId::UiRing8, "ui_ring_8", "リング8", 34},
    {WorldIconId::UiRingC, "ui_ring_c", "リングC", 35},
}};
static_assert(WorldIconDefinitions.size() == static_cast<std::size_t>(WorldIconId::UiRingC) + 1);

const std::unordered_map<std::string, float>* gWorldIconScaleOverrides = nullptr;
constexpr std::size_t WorldIconFilterCount = 2;

struct CachedWorldIconImage {
    const Renderer* renderer = nullptr;
    std::uint64_t generation = 0;
    std::array<ImageHandle, WorldIconFilterCount> handles{};
    std::array<Vec2, WorldIconFilterCount> sourceSizes{};
    std::array<bool, WorldIconFilterCount> sourceSizeReady{};
};

std::array<CachedWorldIconImage, WorldIconDefinitions.size()> gWorldIconImageCache{};

std::size_t textureFilterIndex(TextureFilter filter)
{
    return filter == TextureFilter::Linear ? 1U : 0U;
}

std::string makeWorldIconPathFromNumber(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(WorldIconDir) +
        std::string(WorldIconPrefix) +
        std::to_string(imageNumber) +
        std::string(WorldIconExtension);
}

std::array<std::string, WorldIconDefinitions.size()> makeWorldIconPaths()
{
    std::array<std::string, WorldIconDefinitions.size()> paths{};
    for (std::size_t i = 0; i < WorldIconDefinitions.size(); ++i) {
        paths[i] = makeWorldIconPathFromNumber(WorldIconDefinitions[i].imageNumber);
    }
    return paths;
}

const std::array<std::string, WorldIconDefinitions.size()>& worldIconPaths()
{
    static const std::array<std::string, WorldIconDefinitions.size()> paths = makeWorldIconPaths();
    return paths;
}

const WorldIconDefinition* worldIconDefinitionFast(WorldIconId iconId)
{
    const std::size_t index = static_cast<std::size_t>(iconId);
    if (index < WorldIconDefinitions.size() && WorldIconDefinitions[index].iconId == iconId) {
        return &WorldIconDefinitions[index];
    }

    const auto it = std::find_if(WorldIconDefinitions.begin(), WorldIconDefinitions.end(), [iconId](const WorldIconDefinition& definition) {
        return definition.iconId == iconId;
    });
    return it == WorldIconDefinitions.end() ? nullptr : &*it;
}

bool cachedWorldIconImage(
    Renderer& renderer,
    std::size_t iconIndex,
    TextureFilter filter,
    ImageHandle& outHandle,
    Vec2& outSourceSize)
{
    if (iconIndex >= WorldIconDefinitions.size()) {
        return false;
    }

    CachedWorldIconImage& cache = gWorldIconImageCache[iconIndex];
    const std::uint64_t generation = renderer.imageCacheGeneration();
    if (cache.renderer != &renderer || cache.generation != generation) {
        cache = {};
        cache.renderer = &renderer;
        cache.generation = generation;
    }

    const std::size_t filterIndex = textureFilterIndex(filter);
    if (!cache.handles[filterIndex].valid() || !cache.sourceSizeReady[filterIndex]) {
        const ImageHandle handle = renderer.acquireImage(worldIconPaths()[iconIndex], filter);
        Vec2 sourceSize{};
        if (!handle.valid() || !renderer.getImageSize(handle, sourceSize) || sourceSize.x <= 0.0f || sourceSize.y <= 0.0f) {
            cache.handles[filterIndex] = handle;
            cache.sourceSizeReady[filterIndex] = false;
            return false;
        }
        cache.handles[filterIndex] = handle;
        cache.sourceSizes[filterIndex] = sourceSize;
        cache.sourceSizeReady[filterIndex] = true;
    }

    outHandle = cache.handles[filterIndex];
    outSourceSize = cache.sourceSizes[filterIndex];
    return outHandle.valid();
}
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
    return worldIconDefinitionFast(iconId);
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
    return makeWorldIconPathFromNumber(imageNumber);
}

std::string worldIconPath(WorldIconId iconId)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    if (definition == nullptr) {
        return {};
    }

    const std::size_t index = static_cast<std::size_t>(iconId);
    if (index < worldIconPaths().size() && &WorldIconDefinitions[index] == definition) {
        return worldIconPaths()[index];
    }
    return worldIconPathFromNumber(definition->imageNumber);
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
    if (scaledOptions.applyScaleOverride &&
        gWorldIconScaleOverrides != nullptr &&
        !gWorldIconScaleOverrides->empty() &&
        !definition->key.empty()) {
        const auto it = gWorldIconScaleOverrides->find(std::string(definition->key));
        if (it != gWorldIconScaleOverrides->end()) {
            scaledOptions.scaleMultiplier *= it->second;
        }
    }

    std::string fallbackPath;
    std::string_view path;
    const std::size_t index = static_cast<std::size_t>(iconId);
    if (index < worldIconPaths().size() && &WorldIconDefinitions[index] == definition) {
        path = worldIconPaths()[index];
    } else {
        fallbackPath = worldIconPathFromNumber(definition->imageNumber);
        path = fallbackPath;
    }

    ImageHandle handle{};
    Vec2 sourceSize{};
    if (index < worldIconPaths().size() &&
        &WorldIconDefinitions[index] == definition &&
        cachedWorldIconImage(renderer, index, scaledOptions.filter, handle, sourceSize)) {
        return drawScaledImage(renderer, handle, sourceSize, center, maxSize, scaledOptions);
    }
    return drawScaledImage(renderer, path, center, maxSize, scaledOptions);
}

}
