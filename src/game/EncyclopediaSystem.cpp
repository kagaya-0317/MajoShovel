#include "game/EncyclopediaSystem.hpp"

#include "engine/Ui.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace majo {

namespace {
constexpr float PopupSeconds = 2.6f;
constexpr float PopupWidth = 300.0f;
constexpr float PopupMinHeight = 72.0f;
constexpr int PopupTextScale = 2;
constexpr std::string_view TreasureCategory = "\xE5\xAE\x9D";

int stageValue(EncyclopediaStage stage)
{
    return static_cast<int>(stage);
}

std::string fallbackEffectDescription(std::string_view effectKey)
{
    if (effectKey == "basic_attack") {
        return "敵にダメージを与える";
    }
    if (effectKey == "dig" || effectKey == "dig_hard") {
        return "地形を掘削できる";
    }
    if (effectKey == "light") {
        return "リング上で周囲を照らす";
    }
    if (effectKey == "guard_projectile") {
        return "飛んできた弾を防ぐ";
    }
    if (effectKey == "detect") {
        return "探知効果を発揮する";
    }
    if (effectKey == "heal") {
        return "HPを回復する";
    }
    if (effectKey.rfind("status_", 0) == 0) {
        return "状態異常を付与する";
    }
    if (effectKey.rfind("buff_", 0) == 0 || effectKey.rfind("debuff_", 0) == 0) {
        return "能力変化を与える";
    }
    if (effectKey.rfind("dig", 0) == 0) {
        return "地形へ掘削効果を発揮する";
    }
    return "効果が発動する";
}
}

void EncyclopediaSystem::clear()
{
    itemStages_.clear();
    treasureStages_.clear();
    enemyStages_.clear();
    objectEffects_.clear();
    queuedPopups_.clear();
    activePopup_ = Popup{};
    updateLog_.clear();
}

void EncyclopediaSystem::update(float dt)
{
    if (activePopup_.remaining > 0.0f) {
        activePopup_.remaining = std::max(0.0f, activePopup_.remaining - dt);
    }
    if (activePopup_.remaining <= 0.0f && !queuedPopups_.empty()) {
        activePopup_ = queuedPopups_.front();
        queuedPopups_.pop_front();
        activePopup_.remaining = PopupSeconds;
    }
}

void EncyclopediaSystem::renderPopups(Renderer& renderer, const Camera& camera)
{
    if (activePopup_.remaining <= 0.0f || activePopup_.text.empty()) {
        return;
    }

    const float contentWidth = std::max(0.0f, PopupWidth - ui::SubPanelPadding.x * 2.0f);
    const Vec2 textSize = renderer.measureWrappedText(activePopup_.text, contentWidth, PopupTextScale);
    const Vec2 size{PopupWidth, std::max(PopupMinHeight, textSize.y + ui::SubPanelPadding.y * 2.0f)};
    if (!activePopup_.screenPositionLocked) {
        activePopup_.screenPosition = camera.worldToScreen(activePopup_.position) + Vec2{14.0f, -34.0f};
        activePopup_.screenPositionLocked = true;
    }
    Vec2 pos = activePopup_.screenPosition;
    pos.x = clamp(pos.x, 12.0f, static_cast<float>(camera.width()) - size.x - 12.0f);
    pos.y = clamp(pos.y, 58.0f, static_cast<float>(camera.height()) - size.y - 14.0f);
    activePopup_.screenPosition = pos;

    renderer.setScreenSpace();
    const UiRect panel{pos, size};
    drawUiSubPanel(renderer, panel);
    renderer.drawWrappedText(uiSubPanelContentPos(panel), activePopup_.text, contentWidth, ui::Text, PopupTextScale);
}

void EncyclopediaSystem::noteItemDiscovered(const ObjectDefinition& object, Vec2 position)
{
    raiseObjectStage(object, EncyclopediaStage::Discovered, position, false);
}

void EncyclopediaSystem::noteItemObtained(const ObjectDefinition& object, Vec2 position)
{
    raiseObjectStage(object, EncyclopediaStage::Obtained, position, false);
}

void EncyclopediaSystem::noteItemEquipped(const ObjectDefinition& object, Vec2 position)
{
    raiseObjectStage(object, EncyclopediaStage::Equipped, position, false);
}

void EncyclopediaSystem::noteItemEffect(const ObjectDefinition& object, std::string_view effectKey, std::string_view description, Vec2 position)
{
    const std::string key = canonicalEffectKey(effectKey);
    if (object.id.empty() || key.empty()) {
        return;
    }
    auto& effects = objectEffects_[object.id];
    const bool newEffect = effects.insert(key).second;
    raiseObjectStage(object, EncyclopediaStage::EffectTriggered, position, false);
    if (!newEffect) {
        return;
    }

    const DiscoveryEffectLine* effectLine = findEffectLineByKey(object, key);
    const std::string resolvedDescription = effectLine != nullptr
        ? effectLine->text
        : (description.empty() ? fallbackEffectDescription(key) : std::string(description));
    const std::string itemName = object.name.empty() ? object.id : object.name;
    std::string popup = "効果判明：" + itemName + "\n・" + resolvedDescription;
    enqueuePopup(std::move(popup), position);
}

void EncyclopediaSystem::noteEffectEvent(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog)
{
    if (event.objectId.empty() || event.effectKey.empty()) {
        return;
    }
    if (discoverObjectEffect(event.objectId, event.effectKey, catalog, event.position, event.note)) {
        return;
    }
    const ObjectDefinition* object = catalog.registry.findById(event.objectId);
    if (object != nullptr && !event.description.empty()) {
        noteItemEffect(*object, event.effectKey, event.description, event.position);
    }
}

bool EncyclopediaSystem::discoverObjectEffect(
    std::string_view objectId,
    std::string_view effectKey,
    const ObjectCatalog& catalog,
    Vec2 worldPosition,
    std::string_view optionalNote)
{
    const std::string canonicalKey = canonicalEffectKey(effectKey);
    if (objectId.empty() || canonicalKey.empty()) {
        return false;
    }

    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr) {
        return false;
    }
    const std::size_t lineIndex = findEffectLineIndexByKey(*object, canonicalKey);
    if (lineIndex == object->discoveryEffectLines.size()) {
        return false;
    }

    auto& effects = objectEffects_[object->id];
    if (!effects.insert(canonicalKey).second) {
        return false;
    }

    raiseObjectStage(*object, EncyclopediaStage::EffectTriggered, worldPosition, false);
    const std::vector<std::string> allLines = getObjectEffectDisplayLines(object->id, catalog, EffectRevealMode::DebugAll);
    std::string lineText = lineIndex < allLines.size()
        ? allLines[lineIndex]
        : object->discoveryEffectLines[lineIndex].text;
    if (lineText.empty()) {
        lineText = fallbackEffectDescription(canonicalKey);
    }

    const std::string itemName = object->name.empty() ? object->id : object->name;
    std::string popup = "効果判明：" + itemName + "\n・" + lineText;
    if (!optionalNote.empty()) {
        popup += "\n";
        popup += std::string(optionalNote);
    }
    enqueuePopup(std::move(popup), worldPosition);
    return true;
}

void EncyclopediaSystem::noteEnemyDiscovered(std::string_view enemyId, std::string_view enemyName, Vec2 position)
{
    raiseEnemyStage(enemyId, enemyName, EncyclopediaStage::Discovered, position, false);
}

void EncyclopediaSystem::noteEnemyDefeated(std::string_view enemyId, std::string_view enemyName, Vec2 position)
{
    raiseEnemyStage(enemyId, enemyName, EncyclopediaStage::Complete, position, false);
}

EncyclopediaStage EncyclopediaSystem::objectStage(std::string_view objectId, bool treasure) const
{
    const auto& stages = treasure ? treasureStages_ : itemStages_;
    const auto it = stages.find(std::string(objectId));
    return it == stages.end() ? EncyclopediaStage::Undiscovered : it->second;
}

EncyclopediaStage EncyclopediaSystem::enemyStage(std::string_view enemyId) const
{
    const auto it = enemyStages_.find(std::string(enemyId));
    return it == enemyStages_.end() ? EncyclopediaStage::Undiscovered : it->second;
}

bool EncyclopediaSystem::hasObjectEffect(std::string_view objectId, std::string_view effectKey) const
{
    const auto it = objectEffects_.find(std::string(objectId));
    if (it == objectEffects_.end()) {
        return false;
    }
    return it->second.find(canonicalEffectKey(effectKey)) != it->second.end();
}

std::vector<std::string> EncyclopediaSystem::objectEffects(std::string_view objectId) const
{
    std::vector<std::string> effects;
    const auto it = objectEffects_.find(std::string(objectId));
    if (it == objectEffects_.end()) {
        return effects;
    }
    for (const std::string& effectKey : it->second) {
        effects.emplace_back(fallbackEffectDescription(effectKey));
    }
    std::sort(effects.begin(), effects.end());
    return effects;
}

void EncyclopediaSystem::loadEntry(EncyclopediaKind kind, std::string id, EncyclopediaStage stage)
{
    if (id.empty()) {
        return;
    }
    switch (kind) {
    case EncyclopediaKind::Item:
        itemStages_[std::move(id)] = stage;
        break;
    case EncyclopediaKind::Treasure:
        treasureStages_[std::move(id)] = stage;
        break;
    case EncyclopediaKind::Enemy:
        enemyStages_[std::move(id)] = stage;
        break;
    }
}

void EncyclopediaSystem::loadEffect(std::string objectId, std::string effectKey)
{
    effectKey = canonicalEffectKey(effectKey);
    if (objectId.empty() || effectKey.empty()) {
        return;
    }
    objectEffects_[std::move(objectId)].insert(std::move(effectKey));
}

std::vector<std::string> EncyclopediaSystem::getObjectEffectDisplayLines(
    std::string_view objectId,
    const ObjectCatalog& catalog,
    EffectRevealMode revealMode) const
{
    std::vector<std::string> lines;
    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr) {
        return lines;
    }
    if (object->discoveryEffectLines.empty()) {
        return lines;
    }

    const auto discoveredIt = objectEffects_.find(std::string(objectId));
    const std::unordered_set<std::string>* discovered = discoveredIt != objectEffects_.end()
        ? &discoveredIt->second
        : nullptr;
    for (const DiscoveryEffectLine& line : object->discoveryEffectLines) {
        bool visible = false;
        if (discovered != nullptr) {
            const std::string canonical = canonicalEffectKey(line.effectKey);
            visible = discovered->contains(line.effectKey) || discovered->contains(canonical);
        }
        if (revealMode == EffectRevealMode::DebugAll || visible) {
            lines.push_back(line.text);
        } else if (revealMode == EffectRevealMode::WithUnknown) {
            lines.push_back("？？？");
        }
    }
    return lines;
}

std::vector<EncyclopediaEntrySave> EncyclopediaSystem::saveEntries() const
{
    std::vector<EncyclopediaEntrySave> entries;
    for (const auto& [id, stage] : itemStages_) {
        entries.push_back({EncyclopediaKind::Item, id, stage});
    }
    for (const auto& [id, stage] : treasureStages_) {
        entries.push_back({EncyclopediaKind::Treasure, id, stage});
    }
    for (const auto& [id, stage] : enemyStages_) {
        entries.push_back({EncyclopediaKind::Enemy, id, stage});
    }
    return entries;
}

std::vector<EncyclopediaEffectSave> EncyclopediaSystem::saveEffects() const
{
    std::vector<EncyclopediaEffectSave> effects;
    for (const auto& [objectId, objectEffects] : objectEffects_) {
        for (const std::string& effectKey : objectEffects) {
            effects.push_back({objectId, effectKey});
        }
    }
    return effects;
}

bool EncyclopediaSystem::isTreasure(const ObjectDefinition& object)
{
    return object.category == TreasureCategory;
}

std::string EncyclopediaSystem::canonicalEffectKey(std::string_view effectKey)
{
    if (effectKey == "enemy_damage") {
        return "basic_attack";
    }
    if (effectKey == "terrain_dig") {
        return "dig";
    }
    if (effectKey == "projectile_guard" || effectKey == "guard") {
        return "guard_projectile";
    }
    if (effectKey == "detect") {
        return "detect_treasure";
    }
    return std::string(effectKey);
}

const DiscoveryEffectLine* EncyclopediaSystem::findEffectLineByKey(const ObjectDefinition& object, std::string_view effectKey)
{
    const auto it = std::find_if(
        object.discoveryEffectLines.begin(),
        object.discoveryEffectLines.end(),
        [effectKey](const DiscoveryEffectLine& line) {
            return line.effectKey == effectKey;
        });
    return it != object.discoveryEffectLines.end() ? &*it : nullptr;
}

std::size_t EncyclopediaSystem::findEffectLineIndexByKey(const ObjectDefinition& object, std::string_view effectKey)
{
    const std::string canonical = canonicalEffectKey(effectKey);
    const auto it = std::find_if(
        object.discoveryEffectLines.begin(),
        object.discoveryEffectLines.end(),
        [&canonical](const DiscoveryEffectLine& line) {
            return line.effectKey == canonical || canonicalEffectKey(line.effectKey) == canonical;
        });
    if (it == object.discoveryEffectLines.end()) {
        return object.discoveryEffectLines.size();
    }
    return static_cast<std::size_t>(std::distance(object.discoveryEffectLines.begin(), it));
}

std::string EncyclopediaSystem::makeEffectText(const ObjectDefinition& object, std::string_view description)
{
    const std::string name = object.name.empty() ? object.id : object.name;
    return name + ": " + std::string(description);
}

bool EncyclopediaSystem::raiseObjectStage(const ObjectDefinition& object, EncyclopediaStage stage, Vec2 position, bool popup)
{
    if (object.id.empty()) {
        return false;
    }
    auto& stages = isTreasure(object) ? treasureStages_ : itemStages_;
    EncyclopediaStage& current = stages[object.id];
    if (stageValue(current) >= stageValue(stage)) {
        return false;
    }
    current = stage;
    const std::string name = object.name.empty() ? object.id : object.name;
    const std::string log = std::string("図鑑更新 ") + name + " " + encyclopediaStageName(stage);
    updateLog_.push_back(log);
    if (popup) {
        enqueuePopup(log, position);
    }
    return true;
}

bool EncyclopediaSystem::raiseEnemyStage(std::string_view enemyId, std::string_view enemyName, EncyclopediaStage stage, Vec2 position, bool popup)
{
    if (enemyId.empty()) {
        return false;
    }
    EncyclopediaStage& current = enemyStages_[std::string(enemyId)];
    if (stageValue(current) >= stageValue(stage)) {
        return false;
    }
    current = stage;
    const std::string name = enemyName.empty() ? std::string(enemyId) : std::string(enemyName);
    const std::string log = std::string("敵図鑑更新 ") + name + " " + encyclopediaStageName(stage);
    updateLog_.push_back(log);
    if (popup) {
        enqueuePopup(log, position);
    }
    return true;
}

void EncyclopediaSystem::enqueuePopup(std::string text, Vec2 position)
{
    if (text.empty()) {
        return;
    }
    updateLog_.push_back(text);
    queuedPopups_.push_back(Popup{
        .text = std::move(text),
        .position = position,
        .remaining = PopupSeconds,
    });
}

const char* encyclopediaStageName(EncyclopediaStage stage)
{
    switch (stage) {
    case EncyclopediaStage::Undiscovered:
        return "未発見";
    case EncyclopediaStage::Discovered:
        return "発見済み";
    case EncyclopediaStage::Obtained:
        return "入手済み";
    case EncyclopediaStage::Equipped:
        return "リング装備済み";
    case EncyclopediaStage::EffectTriggered:
        return "効果発動済み";
    case EncyclopediaStage::Complete:
        return "完全確認済み";
    }
    return "未発見";
}

std::string_view encyclopediaKindSaveName(EncyclopediaKind kind)
{
    switch (kind) {
    case EncyclopediaKind::Item:
        return "item";
    case EncyclopediaKind::Treasure:
        return "treasure";
    case EncyclopediaKind::Enemy:
        return "enemy";
    }
    return "item";
}

bool encyclopediaKindFromSaveName(std::string_view name, EncyclopediaKind& outKind)
{
    if (name == "item") {
        outKind = EncyclopediaKind::Item;
        return true;
    }
    if (name == "treasure") {
        outKind = EncyclopediaKind::Treasure;
        return true;
    }
    if (name == "enemy") {
        outKind = EncyclopediaKind::Enemy;
        return true;
    }
    return false;
}

EncyclopediaStage encyclopediaStageFromInt(int value)
{
    return static_cast<EncyclopediaStage>(std::clamp(value, 0, 5));
}

}
