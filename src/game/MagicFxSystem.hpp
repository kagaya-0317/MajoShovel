#pragma once

#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace majo {

class Renderer;
struct DepthRenderEntry;

enum class MagicFxParticleShape {
    SoftCircle,
    Circle,
    Ring,
    SparkLine,
    Shard,
    Crystal,
    WindCrescent,
    WindSparkle,
};

enum class MagicFxSpawnShape {
    Point,
    Circle,
    Ring,
    Line,
    Cone,
};

struct MagicFxRange {
    float min = 0.0f;
    float max = 0.0f;
};

struct MagicFxEmitterHandle {
    std::uint32_t id = 0;

    [[nodiscard]] bool valid() const { return id != 0; }
};

struct MagicFxEmitterConfig {
    Vec2 position{};
    Vec2 direction{1.0f, 0.0f};
    Vec2 baseVelocity{};
    Vec2 acceleration{};
    MagicFxParticleShape particleShape = MagicFxParticleShape::SoftCircle;
    MagicFxSpawnShape spawnShape = MagicFxSpawnShape::Point;
    Color startColor{255, 255, 255, 220};
    Color endColor{255, 255, 255, 0};
    MagicFxRange speed{0.0f, 0.0f};
    MagicFxRange lifetime{0.35f, 0.55f};
    MagicFxRange startSize{4.0f, 6.0f};
    MagicFxRange endSize{0.0f, 2.0f};
    MagicFxRange rotation{0.0f, 0.0f};
    MagicFxRange angularVelocity{0.0f, 0.0f};
    MagicFxRange height{0.0f, 0.0f};
    MagicFxRange verticalVelocity{0.0f, 0.0f};
    float gravity = 0.0f;
    float drag = 0.0f;
    float spawnRadius = 0.0f;
    float spawnLineLength = 0.0f;
    float spreadRadians = 0.0f;
    float stretch = 1.0f;
    float fadeInFraction = 0.0f;
    float fadeOutFraction = 0.45f;
    float emissionRate = 0.0f;
    float duration = 0.0f;
    int burstCount = 0;
    bool loop = false;
    bool depthSorted = true;
};

class MagicFxSystem {
public:
    struct Particle {
        bool active = false;
        MagicFxParticleShape shape = MagicFxParticleShape::SoftCircle;
        Vec2 position{};
        Vec2 velocity{};
        Vec2 acceleration{};
        Color startColor{255, 255, 255, 220};
        Color endColor{255, 255, 255, 0};
        float age = 0.0f;
        float lifetime = 0.5f;
        float startSize = 4.0f;
        float endSize = 0.0f;
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        float height = 0.0f;
        float verticalVelocity = 0.0f;
        float gravity = 0.0f;
        float drag = 0.0f;
        float stretch = 1.0f;
        float fadeInFraction = 0.0f;
        float fadeOutFraction = 0.45f;
        bool depthSorted = true;
    };

    struct LightningBranch {
        std::array<Vec2, 4> points{};
        int pointCount = 0;
        float width = 1.0f;
    };

    struct LightningStrike {
        bool active = false;
        std::array<Vec2, 12> points{};
        int pointCount = 0;
        std::array<LightningBranch, 8> branches{};
        int branchCount = 0;
        float age = 0.0f;
        float lifetime = 0.14f;
        float outerWidth = 6.0f;
        float coreWidth = 2.0f;
        bool strong = true;
    };

    MagicFxEmitterHandle addEmitter(const MagicFxEmitterConfig& config);
    void emitBurst(const MagicFxEmitterConfig& config);
    MagicFxEmitterHandle startFireAura(Vec2 position, float radius);
    MagicFxEmitterHandle startFireballLoop(Vec2 position, Vec2 direction, float radius);
    void playFireGroundBurn(Vec2 position, float radius, float duration);
    MagicFxEmitterHandle startIceAura(Vec2 position, float radius);
    MagicFxEmitterHandle startIceShardLoop(Vec2 position, Vec2 direction, float radius);
    void playIceShatter(Vec2 position, float radius);
    MagicFxEmitterHandle startThunderAura(Vec2 position, float radius);
    void playThunderStrike(Vec2 origin, Vec2 target, bool strong);
    void playThunderBolt(Vec2 origin, Vec2 target);
    void playThunderSparkBurst(Vec2 position, float radius);
    MagicFxEmitterHandle startWindAura(Vec2 position, float radius);
    MagicFxEmitterHandle startWindWaveLoop(Vec2 position, Vec2 direction, float radius);
    void playWindImpact(Vec2 position, Vec2 direction, float radius);
    MagicFxEmitterHandle startEarthAura(Vec2 position, float radius);
    MagicFxEmitterHandle startDirtClodLoop(Vec2 position, Vec2 direction, float radius);
    void playEarthSpikeRise(Vec2 position, float radius);
    void playEarthDebrisBurst(Vec2 position, float radius);
    bool setEmitterPosition(MagicFxEmitterHandle handle, Vec2 position);
    bool stopEmitter(MagicFxEmitterHandle handle);
    void update(float dt);
    void appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const;
    void clear();

    [[nodiscard]] int activeParticleCount() const;
    [[nodiscard]] int activeEmitterCount() const;

private:
    struct Emitter {
        bool active = false;
        std::uint32_t id = 0;
        std::uint32_t parentId = 0;
        MagicFxEmitterConfig config{};
        float age = 0.0f;
        float emitAccumulator = 0.0f;
    };

    MagicFxEmitterHandle addEmitterWithParent(const MagicFxEmitterConfig& config, std::uint32_t parentId);
    void spawnParticles(const MagicFxEmitterConfig& config, int count);
    void updateEmitters(float dt);
    void updateParticles(float dt);
    void updateLightningStrikes(float dt);
    [[nodiscard]] float sample(MagicFxRange range);
    [[nodiscard]] Vec2 sampleSpawnOffset(const MagicFxEmitterConfig& config);
    [[nodiscard]] Vec2 sampleVelocity(const MagicFxEmitterConfig& config);
    void addParticle(Particle particle);

    std::vector<Particle> particles_;
    std::vector<Emitter> emitters_;
    std::vector<LightningStrike> lightningStrikes_;
    std::uint32_t nextEmitterId_ = 1;
};

}
