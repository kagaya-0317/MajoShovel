#include "game/WorldIconRenderer.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace majo {

namespace {
constexpr std::string_view WorldIconDir = "assets/others/";
constexpr std::string_view WorldIconPrefix = "img_";
constexpr std::string_view WorldIconExtension = ".png";
constexpr int MoneyMediumThreshold = 50;
constexpr int MoneyLargeThreshold = 150;
constexpr std::array<int, 6> QuadIndices{{0, 1, 2, 0, 2, 3}};

constexpr std::array<WorldIconDefinition, 40> WorldIconDefinitions{{
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
    {WorldIconId::JunkCrabDebrisCan, "junk_crab_debris_can", "ジャンクラブ ガラクタ缶", 36},
    {WorldIconId::JunkCrabDebrisGear, "junk_crab_debris_gear", "ジャンクラブ ガラクタ歯車", 37},
    {WorldIconId::JunkCrabDebrisBattery, "junk_crab_debris_battery", "ジャンクラブ ガラクタ電池", 38},
    {WorldIconId::JunkCrabDebrisPipe, "junk_crab_debris_pipe", "ジャンクラブ ガラクタパイプ", 39},
    {WorldIconId::MoneyCoin, "money_coin", "お金 吸収コイン", 40},
}};
static_assert(WorldIconDefinitions.size() == static_cast<std::size_t>(WorldIconId::MoneyCoin) + 1);

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

WorldIconDrawOptions effectiveWorldIconDrawOptions(
    const WorldIconDefinition& definition,
    const WorldIconDrawOptions& options)
{
    WorldIconDrawOptions scaledOptions = options;
    if (!scaledOptions.applyScaleOverride ||
        gWorldIconScaleOverrides == nullptr ||
        gWorldIconScaleOverrides->empty() ||
        definition.key.empty()) {
        return scaledOptions;
    }

    const auto it = gWorldIconScaleOverrides->find(std::string(definition.key));
    if (it != gWorldIconScaleOverrides->end()) {
        scaledOptions.scaleMultiplier *= it->second;
    }
    return scaledOptions;
}

bool resolveWorldIconImage(
    Renderer& renderer,
    WorldIconId iconId,
    const WorldIconDefinition& definition,
    TextureFilter filter,
    ImageHandle& outHandle,
    Vec2& outSourceSize)
{
    const std::size_t index = static_cast<std::size_t>(iconId);
    if (index < worldIconPaths().size() &&
        &WorldIconDefinitions[index] == &definition &&
        cachedWorldIconImage(renderer, index, filter, outHandle, outSourceSize)) {
        return true;
    }

    const std::string path = makeWorldIconPathFromNumber(definition.imageNumber);
    outHandle = renderer.acquireImage(path, filter);
    return outHandle.valid() &&
        renderer.getImageSize(outHandle, outSourceSize) &&
        outSourceSize.x > 0.0f &&
        outSourceSize.y > 0.0f;
}

Vec2 rotated(Vec2 value, float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine,
    };
}

std::array<ImageTriangleVertex, 4> projectedIconQuad(
    Vec2 center,
    Vec2 drawSize,
    float faceRatio,
    float imageRotationDegrees,
    float depthAxisDegrees,
    bool flipX,
    bool flipY)
{
    const float axisRadians = depthAxisDegrees * Pi / 180.0f;
    const Vec2 depthAxis = fromAngle(axisRadians);
    const Vec2 tangentAxis{depthAxis.y, -depthAxis.x};
    const float basisRotationDegrees = depthAxisDegrees - 90.0f;
    const float relativeRotationRadians =
        (imageRotationDegrees - basisRotationDegrees) * Pi / 180.0f;
    const Vec2 halfSize = drawSize * 0.5f;
    const std::array<Vec2, 4> localCorners{{
        {-halfSize.x, -halfSize.y},
        {halfSize.x, -halfSize.y},
        {halfSize.x, halfSize.y},
        {-halfSize.x, halfSize.y},
    }};
    std::array<Vec2, 4> textureCoordinates{{
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    }};
    if (flipX) {
        for (Vec2& coordinate : textureCoordinates) {
            coordinate.x = 1.0f - coordinate.x;
        }
    }
    if (flipY) {
        for (Vec2& coordinate : textureCoordinates) {
            coordinate.y = 1.0f - coordinate.y;
        }
    }

    std::array<ImageTriangleVertex, 4> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const Vec2 local = rotated(localCorners[i], relativeRotationRadians);
        vertices[i] = {
            .position = center +
                tangentAxis * local.x +
                depthAxis * (local.y * faceRatio),
            .texCoord = textureCoordinates[i],
        };
    }
    return vertices;
}

std::array<Vec2, 4> extrusionBridgeQuad(
    Vec2 center,
    Vec2 depthAxis,
    float halfDepth,
    float halfLength)
{
    const Vec2 tangentAxis{depthAxis.y, -depthAxis.x};
    return {{
        center - depthAxis * halfDepth - tangentAxis * halfLength,
        center + depthAxis * halfDepth - tangentAxis * halfLength,
        center + depthAxis * halfDepth + tangentAxis * halfLength,
        center - depthAxis * halfDepth + tangentAxis * halfLength,
    }};
}

Color withMultipliedAlpha(Color color, unsigned char alpha)
{
    color.a = static_cast<unsigned char>(
        (static_cast<unsigned int>(color.a) * static_cast<unsigned int>(alpha) + 127U) /
        255U);
    return color;
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

    const WorldIconDrawOptions scaledOptions =
        effectiveWorldIconDrawOptions(*definition, options);
    ImageHandle handle{};
    Vec2 sourceSize{};
    if (!resolveWorldIconImage(
            renderer,
            iconId,
            *definition,
            scaledOptions.filter,
            handle,
            sourceSize)) {
        return false;
    }
    return drawScaledImage(renderer, handle, sourceSize, center, maxSize, scaledOptions);
}

bool drawExtrudedWorldIcon(
    Renderer& renderer,
    WorldIconId iconId,
    Vec2 center,
    Vec2 maxSize,
    const WorldIconDrawOptions& options,
    const ExtrudedWorldIconDrawOptions& extrusion)
{
    const WorldIconDefinition* definition = worldIconDefinition(iconId);
    if (definition == nullptr) {
        return false;
    }

    const WorldIconDrawOptions scaledOptions =
        effectiveWorldIconDrawOptions(*definition, options);
    ImageHandle handle{};
    Vec2 sourceSize{};
    if (!resolveWorldIconImage(
            renderer,
            iconId,
            *definition,
            scaledOptions.filter,
            handle,
            sourceSize)) {
        return false;
    }

    Vec2 drawSize{};
    if (!calculateScaledImageDrawSize(sourceSize, maxSize, scaledOptions, drawSize)) {
        return false;
    }

    const float depthRotationDegrees = std::isfinite(extrusion.depthRotationDegrees)
        ? extrusion.depthRotationDegrees
        : 0.0f;
    const float depthAxisDegrees = std::isfinite(extrusion.depthAxisDegrees)
        ? extrusion.depthAxisDegrees
        : 90.0f;
    const float thicknessPx = std::max(
        0.0f,
        std::isfinite(extrusion.thicknessPx) ? extrusion.thicknessPx : 0.0f);
    const float depthRotationRadians = depthRotationDegrees * Pi / 180.0f;
    const float faceCosine = std::cos(depthRotationRadians);
    const float faceRatio = std::abs(faceCosine);
    const float signedDepth = thicknessPx * std::sin(depthRotationRadians);
    const float visibleDepth = std::abs(signedDepth);
    const Vec2 depthAxis = fromAngle(depthAxisDegrees * Pi / 180.0f);
    const Vec2 frontCenter = center - depthAxis * (signedDepth * 0.5f);
    const Vec2 rearCenter = center + depthAxis * (signedDepth * 0.5f);
    const auto frontVertices = projectedIconQuad(
        frontCenter,
        drawSize,
        faceRatio,
        scaledOptions.rotationDegrees,
        depthAxisDegrees,
        scaledOptions.flipX,
        scaledOptions.flipY);
    const auto rearVertices = projectedIconQuad(
        rearCenter,
        drawSize,
        faceRatio,
        scaledOptions.rotationDegrees,
        depthAxisDegrees,
        scaledOptions.flipX,
        scaledOptions.flipY);
    const bool capVisible =
        faceRatio * std::min(drawSize.x, drawSize.y) >= 0.05f;
    RectF opaqueBounds{0.0f, 0.0f, sourceSize.x, sourceSize.y};
    renderer.getImageOpaqueBounds(handle, opaqueBounds);
    const Vec2 opaqueDrawSize{
        drawSize.x * opaqueBounds.w / sourceSize.x,
        drawSize.y * opaqueBounds.h / sourceSize.y,
    };
    const float bridgeHalfLength =
        std::min(opaqueDrawSize.x, opaqueDrawSize.y) * 0.5f;
    const Color sideColor = withMultipliedAlpha(
        extrusion.sideColor,
        scaledOptions.tint.a);

    bool ok = true;
    bool rendered = false;
    const auto drawCap = [&](const auto& vertices, const ImageTriangleDrawOptions& drawOptions) {
        if (!capVisible) {
            return;
        }
        ok = renderer.drawImageTriangleList(
                 handle,
                 vertices.data(),
                 vertices.size(),
                 QuadIndices.data(),
                 QuadIndices.size(),
                 drawOptions) &&
            ok;
        rendered = true;
    };
    const auto drawBridge = [&](float outlinePx, Color color) {
        if (visibleDepth <= 0.001f || color.a == 0) {
            return;
        }
        const auto vertices = extrusionBridgeQuad(
            center,
            depthAxis,
            visibleDepth * 0.5f + outlinePx,
            bridgeHalfLength + outlinePx);
        renderer.fillPolygon(vertices.data(), vertices.size(), color);
        rendered = true;
    };
    const auto drawOutlineLayer = [&](bool enabled, Color color, int outlinePx) {
        if (!enabled || color.a == 0 || outlinePx <= 0) {
            return;
        }

        ImageTriangleDrawOptions outlineOptions;
        outlineOptions.tint.a = 0;
        outlineOptions.outlineEnabled = true;
        outlineOptions.outlineColor = color;
        outlineOptions.outlinePx = outlinePx;
        drawCap(frontVertices, outlineOptions);
        drawCap(rearVertices, outlineOptions);
        drawBridge(static_cast<float>(outlinePx), color);
    };

    drawOutlineLayer(
        scaledOptions.selectedOutlineEnabled,
        scaledOptions.selectedOutlineColor,
        scaledOptions.selectedOutlinePx);
    drawOutlineLayer(
        scaledOptions.outlineEnabled,
        scaledOptions.outlineColor,
        scaledOptions.outlinePx);

    ImageTriangleDrawOptions imageFillOptions;
    imageFillOptions.tint = scaledOptions.tint;
    imageFillOptions.maskOverlayColor = scaledOptions.maskOverlayColor;
    ImageTriangleDrawOptions farCapFillOptions;
    farCapFillOptions.tint.a = 0;
    farCapFillOptions.maskOverlayColor = sideColor;

    const bool frontIsNear = faceCosine >= 0.0f;
    const auto& nearVertices = frontIsNear ? frontVertices : rearVertices;
    const auto& farVertices = frontIsNear ? rearVertices : frontVertices;
    drawCap(farVertices, farCapFillOptions);
    drawBridge(0.0f, sideColor);
    drawCap(nearVertices, imageFillOptions);
    return rendered && ok;
}

}
