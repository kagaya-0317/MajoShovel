#pragma once

#include "game/InventorySystem.hpp"
#include "game/SpellRingSystem.hpp"

#include <array>
#include <string>
#include <vector>

namespace majo {

constexpr int RingPresetSlotCount = 3;

struct RingPresetItem {
    SpellRingItemType type = SpellRingItemType::Object;
    int ringIndex = 0;
    float localAngle = 0.0f;
    std::string objectId;
    std::string instanceId;
    int currentDurability = -1;
    int maxDurability = -1;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
    bool protectionEnabled = false;
    bool isBroken = false;
};

struct RingPreset {
    bool registered = false;
    std::array<std::vector<RingPresetItem>, SpellRingCount> rings{};
};

struct RingPresetApplyResult {
    bool applied = false;
    int placedCount = 0;
    int missingCount = 0;
    int blockedCount = 0;
    std::string status;
};

RingPresetItem ringPresetItemFromRingItem(const SpellRingItem& item, int ringIndex);
int ringPresetInstanceMatchScore(const RingPresetItem& presetItem, const ItemInstance& candidate);
int ringPresetStackMatchScore(const RingPresetItem& presetItem, const ItemData& candidateItem);

class RingPresetSystem {
public:
    static constexpr int PresetCount = RingPresetSlotCount;

    void clear();
    bool validPresetIndex(int presetIndex) const;
    bool registered(int presetIndex) const;
    const RingPreset& preset(int presetIndex) const;
    RingPreset& preset(int presetIndex);
    void setPreset(int presetIndex, RingPreset preset);
    bool capturePreset(int presetIndex, const SpellRingSystem& spellRing, int unlockedRingCount);
    RingPresetApplyResult applyPreset(
        int presetIndex,
        InventorySystem& inventory,
        SpellRingSystem& spellRing,
        const ObjectCatalog& objectCatalog,
        int unlockedRingCount) const;
    std::vector<RingPresetItem> missingItemsForPreset(
        int presetIndex,
        const InventorySystem& inventory,
        const SpellRingSystem& spellRing,
        const ObjectCatalog& objectCatalog,
        int unlockedRingCount) const;

private:
    std::array<RingPreset, PresetCount> presets_{};
};

} // namespace majo
