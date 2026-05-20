#pragma once

#include "data/EnemyCatalog.hpp"
#include "data/ObjectCatalog.hpp"
#include "engine/Camera.hpp"
#include "engine/Renderer.hpp"

#include <deque>
#include <optional>
#include <span>
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
    std::string note;
    Vec2 position{};
};

struct UiRect;

enum class EffectRevealMode {
    RevealedOnly,
    WithUnknown,
    DebugAll,
};

class EncyclopediaSystem {
public:
    void clear();
    void update(float dt);
    void renderPopups(
        Renderer& renderer,
        const Camera& camera,
        const ObjectCatalog& catalog,
        std::span<const UiRect> avoidRects = {});

    void noteItemDiscovered(const ObjectDefinition& object, Vec2 position);
    bool noteItemObtained(const ObjectDefinition& object, Vec2 position);
    void noteItemEquipped(const ObjectDefinition& object, Vec2 position);
    void noteItemEffect(const ObjectDefinition& object, std::string_view effectKey, std::string_view description, Vec2 position);
    void noteEffectEvent(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog);
    void noteEffectEvents(std::span<const EffectDiscoveryEvent> events, const ObjectCatalog& catalog);
    bool discoverObjectEffect(
        std::string_view objectId,
        std::string_view effectKey,
        const ObjectCatalog& catalog,
        Vec2 worldPosition,
        std::string_view optionalNote = {});
    void noteEnemyDiscovered(std::string_view enemyId, std::string_view enemyName, Vec2 position);
    void noteEnemyDefeated(std::string_view enemyId, std::string_view enemyName, Vec2 position);

    EncyclopediaStage objectStage(std::string_view objectId, bool treasure) const;
    EncyclopediaStage enemyStage(std::string_view enemyId) const;
    bool hasObjectEffect(std::string_view objectId, std::string_view effectKey) const;
    std::vector<std::string> objectEffects(std::string_view objectId) const;
    std::vector<std::string> getObjectEffectDisplayLines(std::string_view objectId, const ObjectCatalog& catalog, EffectRevealMode revealMode) const;

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
        float elapsed = 0.0f;
        float duration = 0.0f;
        float revealSeconds = 0.0f;
        int revealUnitCount = 0;
        bool screenPositionLocked = false;
    };

    struct EffectPopupLine {
        std::string objectId;
        std::string objectName;
        std::string lineText;
        std::string note;
        Vec2 position{};
    };

    static bool isTreasure(const ObjectDefinition& object);
    static std::string canonicalEffectKey(std::string_view effectKey);
    static std::size_t findEffectLineIndexByKey(const ObjectDefinition& object, std::string_view effectKey);
    std::optional<EffectPopupLine> recordObjectEffect(
        const ObjectDefinition& object,
        std::string_view effectKey,
        std::string_view description,
        std::string_view note,
        Vec2 position,
        std::string_view objectNameOverride,
        bool allowGenericFallback);
    std::optional<EffectPopupLine> recordEffectDiscovery(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog);
    bool raiseObjectStage(const ObjectDefinition& object, EncyclopediaStage stage, Vec2 position, bool popup);
    bool raiseEnemyStage(std::string_view enemyId, std::string_view enemyName, EncyclopediaStage stage, Vec2 position, bool popup);
    void enqueueEffectPopup(std::span<const EffectPopupLine> lines);
    void enqueuePopup(std::string text, Vec2 position);

    std::unordered_map<std::string, EncyclopediaStage> itemStages_;
    std::unordered_map<std::string, EncyclopediaStage> treasureStages_;
    std::unordered_map<std::string, EncyclopediaStage> enemyStages_;
    std::unordered_map<std::string, std::unordered_set<std::string>> objectEffects_;
    std::deque<Popup> queuedPopups_;
    std::vector<Popup> activePopups_;
    std::vector<std::string> updateLog_;
};

const char* encyclopediaStageName(EncyclopediaStage stage);
std::string_view encyclopediaKindSaveName(EncyclopediaKind kind);
bool encyclopediaKindFromSaveName(std::string_view name, EncyclopediaKind& outKind);
EncyclopediaStage encyclopediaStageFromInt(int value);

}
