#pragma once

#include "engine/Math.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace majo {

class EnemySystem;
class MagicFxSystem;
class SpellRingSystem;
class TileMap;
struct LightSource;
struct SpellRingItem;
struct Player;

enum class MagicElement {
    Fire,
    Ice,
    Thunder,
    Wind,
    Earth,
};

enum class MagicSoundEvent {
    Cast,
    Impact,
};

std::string_view magicElementDamageType(MagicElement element);
bool magicElementFromCastEffect(std::string_view effect, MagicElement& outElement);

class MagicSystem {
public:
    void setFxSystem(MagicFxSystem* magicFx);
    void cast(MagicElement element, Vec2 origin, Vec2 direction, int power, SpellRingItem* sourceItem = nullptr);
    void update(Player& player, SpellRingSystem& spellRing, EnemySystem& enemies, TileMap& map, float dt);
    void appendLightSources(std::vector<LightSource>& outLights) const;
    void clear();
    std::vector<MagicSoundEvent> consumeSoundEvents();

    enum class ProjectileKind {
        Fireball,
        IceShard,
        WindWave,
        DirtClod,
    };

    enum class GroundKind {
        FirePatch,
        FireAfterglow,
        IceShatter,
        ThunderStrikeLight,
        EarthSpike,
        EarthDebrisBreakLight,
    };

    struct MagicProjectile {
        ProjectileKind kind = ProjectileKind::Fireball;
        MagicElement element = MagicElement::Fire;
        Vec2 position{};
        Vec2 velocity{};
        float radius = 8.0f;
        float age = 0.0f;
        float lifetime = 1.0f;
        float height = 0.0f;
        float verticalVelocity = 0.0f;
        float gravity = 0.0f;
        int damage = 1;
        bool active = true;
        bool piercesWalls = false;
        std::uint32_t fxEmitterId = 0;
    };

    struct MagicGroundArea {
        GroundKind kind = GroundKind::FirePatch;
        Vec2 position{};
        float radius = 20.0f;
        float age = 0.0f;
        float lifetime = 1.0f;
        float tickTimer = 0.0f;
        int damage = 1;
        bool active = true;
        bool triggered = false;
    };

    struct PendingThunder {
        Vec2 origin{};
        float range = 120.0f;
        int damage = 1;
        double paralyzeChance = 0.0;
        double paralyzeDuration = 1.0;
    };

    struct MagicLight {
        Vec2 position{};
        float radius = 80.0f;
        float age = 0.0f;
        float lifetime = 0.15f;
        bool active = true;
    };

private:
    void castFire(Vec2 origin, Vec2 direction, int power);
    void castIce(Vec2 origin, Vec2 direction, int power);
    void castThunder(Vec2 origin, int power);
    void castWind(Vec2 origin, Vec2 direction, int power);
    void castEarth(Vec2 origin, Vec2 direction, int power);
    void updateProjectiles(EnemySystem& enemies, SpellRingSystem& spellRing, TileMap& map, float dt);
    void updateGroundAreas(EnemySystem& enemies, SpellRingSystem& spellRing, float dt);
    void updateThunder(EnemySystem& enemies, SpellRingSystem& spellRing);
    void updateTransientLights(float dt);
    void updateItemAuras(SpellRingSystem& spellRing, float dt);
    std::uint32_t startAuraFxEmitter(MagicElement element, Vec2 position, float radius);
    std::uint32_t startProjectileFxEmitter(const MagicProjectile& projectile);
    void updateProjectileFx(MagicProjectile& projectile);
    void playProjectileImpactFx(const MagicProjectile& projectile);
    void stopProjectileFx(MagicProjectile& projectile);
    void spawnFirePatch(Vec2 position, float radius, int damage);
    void addTransientLight(Vec2 position, float radius, float lifetime);
    void addProjectile(MagicProjectile projectile);
    void addGroundArea(MagicGroundArea area);

    MagicFxSystem* magicFx_ = nullptr;
    std::vector<MagicProjectile> projectiles_;
    std::vector<MagicGroundArea> groundAreas_;
    std::vector<PendingThunder> pendingThunder_;
    std::vector<MagicLight> transientLights_;
    std::vector<MagicSoundEvent> soundEvents_;
};

}
