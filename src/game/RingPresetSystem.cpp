#include "game/RingPresetSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace majo {

namespace {

constexpr int NoMatchScore = std::numeric_limits<int>::max() / 4;

enum class PresetCandidateSource {
    Ring,
    InventoryInstance,
    InventoryStack,
};

struct PresetCandidate {
    PresetCandidateSource source = PresetCandidateSource::Ring;
    int ringIndex = -1;
    int ringItemIndex = -1;
    int inventoryIndex = -1;
    int stackOrdinal = 0;
    const ItemData* item = nullptr;
    std::optional<ItemInstance> instance;
    std::string objectId;
    bool used = false;
};

int boolMismatchPenalty(bool left, bool right, int penalty)
{
    return left == right ? 0 : penalty;
}

int doubleDifferencePenalty(double left, double right, int scale)
{
    return static_cast<int>(std::round(std::abs(left - right) * static_cast<double>(scale)));
}

int stateDifferenceScore(const RingPresetItem& presetItem, const ItemInstance& candidate)
{
    int score = 100'000;
    score += std::abs(presetItem.enhanceLevel - candidate.enhanceLevel) * 4'000;
    score += std::abs(presetItem.attackBonus - candidate.attackBonus) * 900;
    score += std::abs(presetItem.digBonus - candidate.digBonus) * 900;
    score += std::abs(presetItem.durabilityBonus - candidate.durabilityBonus) * 900;
    score += std::abs(presetItem.currentDurability - candidate.currentDurability) * 12;
    score += std::abs(presetItem.maxDurability - candidate.maxDurability) * 8;
    score += doubleDifferencePenalty(presetItem.weightModifier, candidate.weightModifier, 700);
    score += doubleDifferencePenalty(presetItem.sizeModifier, candidate.sizeModifier, 700);
    score += boolMismatchPenalty(presetItem.isBroken, candidate.isBroken, 2'000);
    score += boolMismatchPenalty(presetItem.protectionEnabled, candidate.protectionEnabled, 50);
    return score;
}

ItemInstance baseInstanceForStackCandidate(const ItemData& item)
{
    return makeItemInstanceFromDefinition({}, item);
}

ItemInstance instanceFromRingItem(const SpellRingItem& item, InventorySystem& inventory, const ObjectCatalog& objectCatalog)
{
    ItemInstance instance;
    if (item.instanceId.empty()) {
        const ItemData* object = objectCatalog.registry.findById(item.objectId);
        const ItemData missingObject = object == nullptr ? makeMissingItemData(item.objectId) : ItemData{};
        instance = inventory.createDetachedObjectInstance(object != nullptr ? *object : missingObject);
    } else {
        instance.instanceId = item.instanceId;
        instance.objectId = item.objectId;
    }

    instance.objectId = item.objectId;
    instance.currentDurability = item.durability;
    instance.maxDurability = item.maxDurability;
    instance.enhanceLevel = item.enhanceLevel;
    instance.attackBonus = item.attackBonus;
    instance.digBonus = item.digBonus;
    instance.durabilityBonus = item.durabilityBonus;
    instance.weightModifier = item.weightModifier;
    instance.sizeModifier = item.sizeModifier;
    instance.protectionEnabled = item.protectionEnabled;
    instance.isBroken = item.broken();
    instance.addedEffects = item.addedEffects;
    instance.addedTags = item.addedTags;
    return instance;
}

int sourcePreferencePenalty(PresetCandidateSource source)
{
    switch (source) {
    case PresetCandidateSource::Ring: return 0;
    case PresetCandidateSource::InventoryInstance: return 20;
    case PresetCandidateSource::InventoryStack: return 80;
    }
    return 100;
}

int candidateMatchScore(const RingPresetItem& presetItem, const PresetCandidate& candidate)
{
    if (candidate.objectId != presetItem.objectId || candidate.item == nullptr) {
        return NoMatchScore;
    }

    if (candidate.instance) {
        int score = ringPresetInstanceMatchScore(presetItem, *candidate.instance);
        if (score >= NoMatchScore) {
            return score;
        }
        return score + sourcePreferencePenalty(candidate.source);
    }

    int score = ringPresetStackMatchScore(presetItem, *candidate.item);
    if (score >= NoMatchScore) {
        return score;
    }
    return score + sourcePreferencePenalty(candidate.source);
}

std::vector<PresetCandidate> buildPresetCandidates(
    const RingPreset& preset,
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog)
{
    std::unordered_map<std::string, int> wantedCounts;
    for (const auto& ringItems : preset.rings) {
        for (const RingPresetItem& item : ringItems) {
            if (!item.objectId.empty()) {
                ++wantedCounts[item.objectId];
            }
        }
    }

    std::vector<PresetCandidate> candidates;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing.itemsForRing(ringIndex);
        for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
            const SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            if (item.objectId.empty()) {
                continue;
            }
            const ItemData* object = objectCatalog.registry.findById(item.objectId);
            const ItemData missingObject = object == nullptr ? makeMissingItemData(item.objectId) : ItemData{};
            candidates.push_back(PresetCandidate{
                .source = PresetCandidateSource::Ring,
                .ringIndex = ringIndex,
                .ringItemIndex = itemIndex,
                .item = object != nullptr ? object : &missingObject,
                .instance = ItemInstance{
                    .instanceId = item.instanceId,
                    .objectId = item.objectId,
                    .currentDurability = item.durability,
                    .maxDurability = item.maxDurability,
                    .enhanceLevel = item.enhanceLevel,
                    .attackBonus = item.attackBonus,
                    .digBonus = item.digBonus,
                    .durabilityBonus = item.durabilityBonus,
                    .weightModifier = item.weightModifier,
                    .sizeModifier = item.sizeModifier,
                    .protectionEnabled = item.protectionEnabled,
                    .isBroken = item.broken(),
                    .addedEffects = item.addedEffects,
                    .addedTags = item.addedTags,
                },
                .objectId = item.objectId,
            });
            if (object == nullptr) {
                candidates.back().item = nullptr;
            }
        }
    }

    const auto& instances = inventory.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        if (instance.instance.objectId.empty() ||
            inventory.isStaffEquipped(instance.instance.instanceId)) {
            continue;
        }
        candidates.push_back(PresetCandidate{
            .source = PresetCandidateSource::InventoryInstance,
            .inventoryIndex = i,
            .item = &instance.item,
            .instance = instance.instance,
            .objectId = instance.instance.objectId,
        });
    }

    const auto& stacks = inventory.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.objectId.empty() || stack.count <= 0) {
            continue;
        }
        const int candidateCount = std::min(stack.count, wantedCounts[stack.objectId]);
        for (int ordinal = 0; ordinal < candidateCount; ++ordinal) {
            candidates.push_back(PresetCandidate{
                .source = PresetCandidateSource::InventoryStack,
                .inventoryIndex = i,
                .stackOrdinal = ordinal,
                .item = &stack.item,
                .objectId = stack.objectId,
            });
        }
    }

    return candidates;
}

PresetCandidate* bestCandidateForItem(RingPresetItem presetItem, std::vector<PresetCandidate>& candidates)
{
    PresetCandidate* best = nullptr;
    int bestScore = NoMatchScore;
    for (PresetCandidate& candidate : candidates) {
        if (candidate.used) {
            continue;
        }
        const int score = candidateMatchScore(presetItem, candidate);
        if (score < bestScore) {
            best = &candidate;
            bestScore = score;
        }
    }
    return best;
}

void setWorkingActiveRing(SpellRingSystem& spellRing, int ringIndex)
{
    spellRing.switchActiveRing(ringIndex - spellRing.activeRingIndex());
}

bool addCandidateToWorkingRing(
    const PresetCandidate& candidate,
    const RingPresetItem& presetItem,
    InventorySystem& inventory,
    SpellRingSystem& working,
    const ObjectCatalog& objectCatalog,
    SpellRingAddResult* outResult)
{
    if (candidate.item == nullptr) {
        return false;
    }

    setWorkingActiveRing(working, presetItem.ringIndex);
    if (candidate.instance) {
        return working.addObjectItemAtAngle(*candidate.item, *candidate.instance, presetItem.localAngle, outResult);
    }

    ItemInstance instance = inventory.createDetachedObjectInstance(*candidate.item);
    if (instance.objectId.empty()) {
        instance.objectId = candidate.objectId;
    }
    if (objectCatalog.registry.findById(instance.objectId) == nullptr && !candidate.objectId.empty()) {
        instance.objectId = candidate.objectId;
    }
    return working.addObjectItemAtAngle(*candidate.item, instance, presetItem.localAngle, outResult);
}

std::string applyStatusText(int presetIndex, const RingPresetApplyResult& result)
{
    std::string status = "プリセット" + std::to_string(presetIndex + 1) + "を呼び出しました";
    if (result.missingCount > 0 || result.blockedCount > 0) {
        status += " / 配置 " + std::to_string(result.placedCount);
        if (result.missingCount > 0) {
            status += " 不足 " + std::to_string(result.missingCount);
        }
        if (result.blockedCount > 0) {
            status += " 配置不可 " + std::to_string(result.blockedCount);
        }
    }
    return status;
}

} // namespace

RingPresetItem ringPresetItemFromRingItem(const SpellRingItem& item, int ringIndex)
{
    RingPresetItem presetItem;
    presetItem.type = item.type;
    presetItem.ringIndex = ringIndex;
    presetItem.localAngle = item.localAngle;
    presetItem.objectId = item.objectId;
    presetItem.instanceId = item.instanceId;
    presetItem.currentDurability = item.durability;
    presetItem.maxDurability = item.maxDurability;
    presetItem.enhanceLevel = item.enhanceLevel;
    presetItem.attackBonus = item.attackBonus;
    presetItem.digBonus = item.digBonus;
    presetItem.durabilityBonus = item.durabilityBonus;
    presetItem.weightModifier = item.weightModifier;
    presetItem.sizeModifier = item.sizeModifier;
    presetItem.protectionEnabled = item.protectionEnabled;
    presetItem.isBroken = item.broken();
    return presetItem;
}

int ringPresetInstanceMatchScore(const RingPresetItem& presetItem, const ItemInstance& candidate)
{
    if (presetItem.objectId.empty() || candidate.objectId != presetItem.objectId) {
        return NoMatchScore;
    }
    if (!presetItem.instanceId.empty() && candidate.instanceId == presetItem.instanceId) {
        return 0;
    }
    return stateDifferenceScore(presetItem, candidate);
}

int ringPresetStackMatchScore(const RingPresetItem& presetItem, const ItemData& candidateItem)
{
    if (presetItem.objectId.empty() || candidateItem.id != presetItem.objectId) {
        return NoMatchScore;
    }
    const ItemInstance baseInstance = baseInstanceForStackCandidate(candidateItem);
    return stateDifferenceScore(presetItem, baseInstance) + 1'000;
}

void RingPresetSystem::clear()
{
    presets_ = {};
}

bool RingPresetSystem::validPresetIndex(int presetIndex) const
{
    return presetIndex >= 0 && presetIndex < PresetCount;
}

bool RingPresetSystem::registered(int presetIndex) const
{
    return validPresetIndex(presetIndex) && presets_[static_cast<std::size_t>(presetIndex)].registered;
}

const RingPreset& RingPresetSystem::preset(int presetIndex) const
{
    static const RingPreset EmptyPreset{};
    if (!validPresetIndex(presetIndex)) {
        return EmptyPreset;
    }
    return presets_[static_cast<std::size_t>(presetIndex)];
}

RingPreset& RingPresetSystem::preset(int presetIndex)
{
    static RingPreset EmptyPreset{};
    if (!validPresetIndex(presetIndex)) {
        EmptyPreset = {};
        return EmptyPreset;
    }
    return presets_[static_cast<std::size_t>(presetIndex)];
}

void RingPresetSystem::setPreset(int presetIndex, RingPreset preset)
{
    if (!validPresetIndex(presetIndex)) {
        return;
    }
    presets_[static_cast<std::size_t>(presetIndex)] = std::move(preset);
}

bool RingPresetSystem::capturePreset(int presetIndex, const SpellRingSystem& spellRing)
{
    if (!validPresetIndex(presetIndex)) {
        return false;
    }

    RingPreset captured;
    captured.registered = true;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing.itemsForRing(ringIndex);
        auto& presetItems = captured.rings[static_cast<std::size_t>(ringIndex)];
        presetItems.reserve(ringItems.size());
        for (const SpellRingItem& item : ringItems) {
            if (item.objectId.empty()) {
                continue;
            }
            presetItems.push_back(ringPresetItemFromRingItem(item, ringIndex));
        }
    }

    presets_[static_cast<std::size_t>(presetIndex)] = std::move(captured);
    return true;
}

RingPresetApplyResult RingPresetSystem::applyPreset(
    int presetIndex,
    InventorySystem& inventory,
    SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog) const
{
    RingPresetApplyResult result;
    if (!registered(presetIndex)) {
        result.status = "プリセット" + std::to_string(presetIndex + 1) + "は未登録です";
        return result;
    }

    const RingPreset& targetPreset = preset(presetIndex);
    std::vector<PresetCandidate> candidates = buildPresetCandidates(targetPreset, inventory, spellRing, objectCatalog);
    InventorySystem workingInventory = inventory;
    SpellRingSystem workingRing = spellRing;
    const int previousActiveRing = spellRing.activeRingIndex();
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        workingRing.itemsForRing(ringIndex).clear();
    }

    std::vector<const PresetCandidate*> usedCandidates;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const auto& presetItems = targetPreset.rings[static_cast<std::size_t>(ringIndex)];
        for (const RingPresetItem& presetItem : presetItems) {
            PresetCandidate* candidate = bestCandidateForItem(presetItem, candidates);
            if (candidate == nullptr) {
                ++result.missingCount;
                continue;
            }

            SpellRingAddResult addResult{};
            if (!addCandidateToWorkingRing(*candidate, presetItem, workingInventory, workingRing, objectCatalog, &addResult)) {
                ++result.blockedCount;
                continue;
            }

            candidate->used = true;
            usedCandidates.push_back(candidate);
            ++result.placedCount;
        }
    }

    int projectedSlots = static_cast<int>(inventory.objectStacks().size() + inventory.objectInstances().size());
    std::unordered_map<int, int> usedStackCounts;
    std::unordered_set<std::string> usedInventoryInstanceIds;
    std::unordered_set<std::string> usedRingKeys;
    for (const PresetCandidate* candidate : usedCandidates) {
        if (candidate->source == PresetCandidateSource::InventoryStack) {
            ++usedStackCounts[candidate->inventoryIndex];
        } else if (candidate->source == PresetCandidateSource::InventoryInstance && candidate->instance) {
            usedInventoryInstanceIds.insert(candidate->instance->instanceId);
        } else if (candidate->source == PresetCandidateSource::Ring) {
            usedRingKeys.insert(std::to_string(candidate->ringIndex) + ":" + std::to_string(candidate->ringItemIndex));
        }
    }

    const auto& stacks = inventory.objectStacks();
    for (const auto& [stackIndex, useCount] : usedStackCounts) {
        if (stackIndex >= 0 && stackIndex < static_cast<int>(stacks.size()) &&
            stacks[static_cast<std::size_t>(stackIndex)].count <= useCount) {
            --projectedSlots;
        }
    }
    projectedSlots -= static_cast<int>(usedInventoryInstanceIds.size());

    std::vector<SpellRingItem> ringItemsToReturn;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing.itemsForRing(ringIndex);
        for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
            const SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            if (item.objectId.empty()) {
                continue;
            }
            const std::string key = std::to_string(ringIndex) + ":" + std::to_string(itemIndex);
            if (usedRingKeys.find(key) == usedRingKeys.end()) {
                ringItemsToReturn.push_back(item);
                ++projectedSlots;
            }
        }
    }

    if (projectedSlots > inventory.screenSlotCount()) {
        result.status = "リュックがいっぱいでリングを入れ替えられません";
        return result;
    }

    for (const std::string& instanceId : usedInventoryInstanceIds) {
        InventoryObjectInstance moved;
        if (!workingInventory.takeObjectInstance(instanceId, moved)) {
            result.status = "リュックのアイテムを移動できませんでした";
            return result;
        }
    }
    for (const auto& [stackIndex, useCount] : usedStackCounts) {
        if (stackIndex < 0 || stackIndex >= static_cast<int>(stacks.size())) {
            result.status = "リュックのアイテムを移動できませんでした";
            return result;
        }
        const std::string objectId = stacks[static_cast<std::size_t>(stackIndex)].objectId;
        if (!workingInventory.removeObjectItemCount(objectId, useCount)) {
            result.status = "リュックのアイテムを移動できませんでした";
            return result;
        }
    }
    for (const SpellRingItem& item : ringItemsToReturn) {
        if (!workingInventory.addObjectInstance(objectCatalog, instanceFromRingItem(item, workingInventory, objectCatalog))) {
            result.status = "リュックがいっぱいでリングを入れ替えられません";
            return result;
        }
    }

    inventory = std::move(workingInventory);
    setWorkingActiveRing(workingRing, previousActiveRing);
    workingRing.resetBaseWeightToCurrent();
    spellRing = std::move(workingRing);

    result.applied = true;
    result.status = applyStatusText(presetIndex, result);
    return result;
}

std::vector<RingPresetItem> RingPresetSystem::missingItemsForPreset(
    int presetIndex,
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog) const
{
    std::vector<RingPresetItem> missing;
    if (!registered(presetIndex)) {
        return missing;
    }

    const RingPreset& targetPreset = preset(presetIndex);
    std::vector<PresetCandidate> candidates = buildPresetCandidates(targetPreset, inventory, spellRing, objectCatalog);
    for (const auto& ringItems : targetPreset.rings) {
        for (const RingPresetItem& presetItem : ringItems) {
            PresetCandidate* candidate = bestCandidateForItem(presetItem, candidates);
            if (candidate == nullptr) {
                missing.push_back(presetItem);
                continue;
            }
            candidate->used = true;
        }
    }
    return missing;
}

} // namespace majo
