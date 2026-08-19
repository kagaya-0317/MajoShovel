#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"
#include "game/EntityStatusVisuals.hpp"
#include "game/TileMap.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class EffectType {
    Ring,
    LevelUpPulseRing,
    Particle
};

enum class ParticleVisual {
    Circle,
    RockShard,
    Sparkle,
    LevelUpTwinkle,
    ImpactSpark,
    ImpactBurst,
    PoisonBubble,
    WaterDrop,
    ThunderArc
};

enum class EffectLayer {
    World,
    Foreground,
    Ground
};

inline constexpr float WarpPointPulseCycleSeconds = 2.4f;

enum class DamagePopupStyle {
    Enemy,
    Player,
    Heal,
    Guard,
    Critical,
    WeakPoint
};

enum class ParticleEffectId {
    DigDust,
    DirtBreak,
    RockBreak,
    OreBreak,
    EnemyHit,
    EnemyPoisonHit,
    EnemyBleedHit,
    EnemySleepHit,
    EnemyConfuseHit,
    EnemyBlindHit,
    EnemyFireHit,
    EnemyWaterHit,
    EnemyIceHit,
    EnemyThunderHit,
    EnemyWindHit,
    EnemyEarthHit,
    EnemyDeathSoul,
    CaptureSuccess,
    DropPickup,
    TorchFlicker,
    MagicFire,
    MagicIce,
    MagicThunder,
    MagicWind,
    MagicEarth,
    MagicDefault,
    PoisonAura,
    SlowAura,
    BleedAura,
    FrozenSparkle,
    SpecialItemGlimmer,
    WarpCircle,
    BossCircle,
    ItemBreak,
    WoodBreak,
    CeramicBreak,
    GlassBreak,
};

enum class ItemBreakVisual {
    Generic,
    Wood,
    Ceramic,
    Glass,
};

struct Effect {
    bool active = false;
    EffectType type = EffectType::Particle;
    EffectLayer layer = EffectLayer::World;
    Vec2 position{};
    Vec2 velocity{};
    Vec2 acceleration{};
    Color color{};
    ParticleVisual visual = ParticleVisual::Circle;
    float age = 0.0f;
    float duration = 0.35f;
    float startRadius = 2.0f;
    float endRadius = 10.0f;
    float ringWidth = 5.0f;
    float drag = 3.5f;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float shardAspect = 1.0f;
    int shardVariant = 0;
    bool physicsShard = false;
    float altitude = 0.0f;
    float verticalVelocity = 0.0f;
    float gravity = 0.0f;
    float bounceRestitution = 0.0f;
    float groundFriction = 0.0f;
    float shadowVisualSize = 0.0f;
    int bouncesRemaining = 0;
};

struct DamagePopup {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float age = 0.0f;
    float duration = 0.72f;
    int amount = 0;
    DamagePopupStyle style = DamagePopupStyle::Enemy;
};

struct StatusTextPopup {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float age = 0.0f;
    float duration = 0.9f;
    std::string text;
    Color color{255, 255, 255, 255};
    StatusPopupTarget target = StatusPopupTarget::Enemy;
};

struct EffectSoundEvent {
    std::string cueId;
    Vec2 position{};
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
};

struct LevelUpTextPopup {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float age = 0.0f;
    float duration = 1.12f;
};

struct SmokeBurstOptions {
    int count = 10;
    float size = 22.0f;
    float sizeJitter = 0.35f;
    float spreadRadius = 11.0f;
    float speed = 28.0f;
    float riseSpeed = 18.0f;
    float duration = 0.64f;
    float durationJitter = 0.10f;
    Color colorA{226, 226, 224, 178};
    Color colorB{142, 146, 150, 172};
    EffectLayer layer = EffectLayer::Foreground;
};

struct PresentationPulseRingOptions {
    float duration = 1.25f;
    float interval = 0.25f;
    float startRadius = 8.0f;
    float endRadius = 72.0f;
    float ringWidth = 5.0f;
    EffectLayer layer = EffectLayer::Foreground;
};

struct SmokePuff {
    bool active = false;
    EffectLayer layer = EffectLayer::Foreground;
    Vec2 position{};
    Vec2 velocity{};
    Color color{};
    float age = 0.0f;
    float duration = 0.64f;
    float radius = 18.0f;
    float growEnd = 0.22f;
    float shrinkStart = 0.58f;
    float peakScale = 1.20f;
    float lobeSpread = 0.35f;
    float phase = 0.0f;
};

class EffectSystem {
public:
    void setLightweightMode(bool enabled) { lightweightMode_ = enabled; }
    [[nodiscard]] bool lightweightMode() const { return lightweightMode_; }

    void update(float dt);
    void renderGround(Renderer& renderer);
    void render(Renderer& renderer);
    void renderShadows(Renderer& renderer);
    void appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer);
    void renderForeground(Renderer& renderer);
    void renderDamagePopups(Renderer& renderer);
    std::vector<EffectSoundEvent> consumeSoundEvents();

    void spawn(
        ParticleEffectId id,
        Vec2 position,
        Vec2 direction = {1.0f, 0.0f},
        float scale = 1.0f,
        EffectLayer layer = EffectLayer::World,
        Color colorOverride = {0, 0, 0, 0},
        int countMultiplier = 1);
    void spawnDamagePopup(Vec2 position, int amount, DamagePopupStyle style = DamagePopupStyle::Enemy);
    void spawnStatusPopup(Vec2 position, std::string_view stateId, StatusPopupTarget target);
    void spawnLevelUpPopup(Vec2 position);
    void spawnPresentationPulseRings(
        Vec2 position,
        Color color,
        int pulseCount,
        PresentationPulseRingOptions options = {});
    void spawnLevelUpEffects(Vec2 position);
    void spawnDigHit(Vec2 position, Vec2 direction = {1.0f, 0.0f}, Color colorOverride = {0, 0, 0, 0}, bool playSound = true);
    void spawnTileBreak(
        Vec2 position,
        TileType tileType = TileType::Dirt,
        Color colorOverride = {0, 0, 0, 0},
        bool playSound = true,
        float scale = 1.0f,
        int debrisCountMultiplier = 1);
    void spawnCrateBreak(Vec2 position, Color colorOverride = {0, 0, 0, 0}, bool playSound = true);
    void spawnSmokeBurst(Vec2 position, SmokeBurstOptions options = {});
    void spawnAttackImpactBurst(Vec2 position, SmokeBurstOptions options = {}, bool playSound = true);
    void spawnEnemyHit(Vec2 position, std::string_view effect = {}, bool playSound = true);
    void spawnEnemyDeath(Vec2 position, bool playSound = true);
    void spawnEnemyTransform(Vec2 position, bool playSound = true);
    void spawnThrowStart(Vec2 position, Vec2 direction);
    void spawnReturn(Vec2 position);
    void spawnRingTrail(Vec2 position, Vec2 direction);
    void spawnForegroundRingTrail(Vec2 position, Vec2 direction);
    void spawnCaptureSuccess(Vec2 position, Vec2 direction, bool playSound = true);
    void spawnDropPickup(Vec2 position, Vec2 direction, bool playSound = true);
    void spawnItemBreak(Vec2 position, ItemBreakVisual visual = ItemBreakVisual::Generic, float scale = 1.0f, bool playSound = true);
    void spawnBrokenItemSmoke(Vec2 position, float scale = 1.0f);
    void spawnMaterialFloat(Vec2 position, Color color);
    void spawnTorchFlicker(Vec2 position);
    void spawnForegroundTorchFlicker(Vec2 position);
    void spawnStatusAura(Vec2 position, std::string_view stateId);
    void spawnSpecialItemGlimmer(Vec2 position);
    void spawnForegroundSpecialItemGlimmer(Vec2 position);
    void spawnWarpCircle(Vec2 position, bool boss);
    void spawnAreaPulse(Vec2 position, float radius, Color color);
    void spawnExplosion(Vec2 position, float radius, bool playSound = true);
    void spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power);

private:
    void queueSound(std::string_view cueId, Vec2 position, float volumeScale = 1.0f, float pitchScale = 1.0f);
    void renderLayer(Renderer& renderer, EffectLayer layer);
    void renderSmokeLayer(Renderer& renderer, EffectLayer layer);
    void spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration, EffectLayer layer = EffectLayer::World);
    Effect* spawnParticle(
        Vec2 position,
        Vec2 velocity,
        float radius,
        Color color,
        float duration,
        Vec2 acceleration = {},
        float drag = 3.5f,
        EffectLayer layer = EffectLayer::World,
        ParticleVisual visual = ParticleVisual::Circle,
        int shardVariant = 0,
        float rotation = 0.0f,
        float angularVelocity = 0.0f,
        float shardAspect = 1.0f);
    void spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration, EffectLayer layer = EffectLayer::World);

    ObjectPool<Effect, balance::MaxEffects> effects_;
    ObjectPool<SmokePuff, 192> smokePuffs_;
    ObjectPool<DamagePopup, 128> damagePopups_;
    ObjectPool<StatusTextPopup, 96> statusTextPopups_;
    ObjectPool<LevelUpTextPopup, 8> levelUpTextPopups_;
    std::vector<EffectSoundEvent> soundEvents_;
    bool lightweightMode_ = false;
};

}
