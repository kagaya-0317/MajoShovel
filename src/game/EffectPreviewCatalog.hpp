#pragma once

#include "engine/Math.hpp"
#include "game/EffectSystem.hpp"
#include "game/MagicAudio.hpp"
#include "game/MagicFxSystem.hpp"

#include <span>
#include <string_view>

namespace majo {

enum class EffectPreviewSource {
    EffectSystem,
    MagicFx,
    StatusVisual,
};

enum class EffectPreviewTarget {
    Player,
    EnemySlime,
    WallTile,
};

enum class EffectPreviewPlayback {
    BurstEvery20Frames,
    PersistentEmitter,
    StatusLoop,
};

enum class EffectPreviewAction {
    ParticlePreset,
    DigHit,
    TileBreak,
    CrateBreak,
    EnemyHit,
    EnemyDeath,
    EnemyTransform,
    CaptureSuccess,
    DropPickup,
    ItemBreakGeneric,
    ItemBreakWood,
    ItemBreakCeramic,
    ItemBreakGlass,
    MaterialFloat,
    TorchFlicker,
    ForegroundTorchFlicker,
    SpecialItemGlimmer,
    ForegroundSpecialItemGlimmer,
    WarpCircle,
    BossCircle,
    AreaPulse,
    Explosion,
    MagicCast,
};

struct EffectPreviewEntry {
    std::string_view id;
    std::string_view label;
    std::string_view group;
    EffectPreviewSource source = EffectPreviewSource::EffectSystem;
    EffectPreviewTarget target = EffectPreviewTarget::Player;
    EffectPreviewPlayback playback = EffectPreviewPlayback::BurstEvery20Frames;
    EffectPreviewAction action = EffectPreviewAction::ParticlePreset;
    ParticleEffectId particleId = ParticleEffectId::DigDust;
    std::string_view argument;
    Vec2 offset{};
    Vec2 direction{1.0f, 0.0f};
    float radius = 32.0f;
    float scale = 1.0f;
    std::string_view previewSoundCueId;
    std::string_view layeredPreviewSoundCueId;
};

[[nodiscard]] std::span<const EffectPreviewEntry> effectSystemPreviewEntries();
[[nodiscard]] std::span<const EffectPreviewEntry> magicFxPreviewEntries();
[[nodiscard]] std::span<const EffectPreviewEntry> entityStatusPreviewEntries();

void playEffectSystemPreview(
    EffectSystem& effects,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction,
    TileType wallTileType,
    Color wallTileColor);

[[nodiscard]] MagicFxEmitterHandle startMagicFxPreview(
    MagicFxSystem& magicFx,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction);

void playMagicFxPreview(
    MagicFxSystem& magicFx,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction);

} // namespace majo
