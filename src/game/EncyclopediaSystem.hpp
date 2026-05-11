#pragma once

#include "data/EnemyCatalog.hpp"
#include "data/ObjectCatalog.hpp"
#include "engine/Camera.hpp"
#include "engine/Renderer.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace majo {

enum class EncyclopediaKind {
    Item,
    Treasure,
    Enemy,
};

enum class EncyclopediaStage {
    Undiscovered = 0,
    Discovered = 1,
    Obtained = 2,
    Equipped = 3,
    EffectTriggered = 4,
    Complete = 5,
};

struct EncyclopediaEntrySave {
    EncyclopediaKind kind = EncyclopediaKind::Item;
    std::string id;
    EncyclopediaStage stage = EncyclopediaStage::Undiscovered;
};

struct EncyclopediaEffectSave {
    std::string objectId;
    std::string effectKey;
};

struct EffectDiscoveryEvent {
    std::string objectId;
    std::string objectName;
    std::string effectKey;
    std::string description;
    Vec2 position{};
};

class EncyclopediaSystem {
public:
    void clear();
    void update(float dt);
    void renderPopups(Renderer& renderer, const Camera& camera);

    void noteItemDiscovered(const ObjectDefinition& object, Vec2 position);
    void noteItemObtained(const ObjectDefinition& object, Vec2 position);
    void noteItemEquipped(const ObjectDefinition& object, Vec2 position);
    void noteItemEffect(const ObjectDefinition& object, std::string_view effectKey, std::string_view description, Vec2 position);
    void noteEffectEvent(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog);
    void noteEnemyDiscovered(std::string_view enemyId, std::string_view enemyName, Vec2 position);
    void noteEnemyDefeated(std::string_view enemyId, std::string_view enemyName, Vec2 position);

    EncyclopediaStage objectStage(std::string_view objectId, bool treasure) const;
    EncyclopediaStage enemyStage(std::string_view enemyId) const;
    bool hasObjectEffect(std::string_view objectId, std::string_view effectKey) const;
    std::vector<std::string> objectEffects(std::string_view objectId) const;

    void loadEntry(EncyclopediaKind kind, std::string id, EncyclopediaStage stage);
    void loadEffect(std::string objectId, std::string effectKey);
    std::vector<EncyclopediaEntrySave> saveEntries() const;
    std::vector<EncyclopediaEffectSave> saveEffects() const;

    const std::vector<std::string>& updateLog() const { return updateLog_; }

private:
    struct Popup {
        std::string text;
        Vec2 position{};
        Vec2 screenPosition{};
        float remaining = 0.0f;
        bool screenPositionLocked = false;
    };

    static bool isTreasure(const ObjectDefinition& object);
    static std::string makeEffectText(const ObjectDefinition& object, std::string_view description);
    bool raiseObjectStage(const ObjectDefinition& object, EncyclopediaStage stage, Vec2 position, bool popup);
    bool raiseEnemyStage(std::string_view enemyId, std::string_view enemyName, EncyclopediaStage stage, Vec2 position, bool popup);
    void enqueuePopup(std::string text, Vec2 position);

    std::unordered_map<std::string, EncyclopediaStage> itemStages_;
    std::unordered_map<std::string, EncyclopediaStage> treasureStages_;
    std::unordered_map<std::string, EncyclopediaStage> enemyStages_;
    std::unordered_map<std::string, std::unordered_set<std::string>> objectEffects_;
    std::deque<Popup> queuedPopups_;
    Popup activePopup_{};
    std::vector<std::string> updateLog_;
};

const char* encyclopediaStageName(EncyclopediaStage stage);
std::string_view encyclopediaKindSaveName(EncyclopediaKind kind);
bool encyclopediaKindFromSaveName(std::string_view name, EncyclopediaKind& outKind);
EncyclopediaStage encyclopediaStageFromInt(int value);

}
