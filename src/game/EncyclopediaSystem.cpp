#include "game/EncyclopediaSystem.hpp"

#include "engine/Ui.hpp"
#include "game/InventoryUiCommon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace majo {

namespace {
constexpr float PopupWidth = 300.0f;
constexpr float PopupMinHeight = 72.0f;
constexpr float PopupPaddingX = 16.0f;
constexpr float PopupPaddingY = 12.0f;
constexpr float PopupLineGap = 6.0f;
constexpr float PopupScreenMargin = 12.0f;
constexpr float PopupTopMargin = 58.0f;
constexpr float PopupAvoidPadding = 8.0f;
constexpr float PopupFadeInSeconds = 0.2f;
constexpr float PopupFadeOutSeconds = 0.2f;
constexpr float PopupHoldAfterRevealSeconds = 2.5f;
constexpr float PopupRevealUnitsPerSecond = 34.0f;
constexpr int PopupTextScale = 2;
constexpr int MaxActiveDiscoveryPopups = 3;
constexpr std::string_view TreasureCategory = "\xE5\xAE\x9D";
constexpr std::string_view NoEffectText = "\xE3\x81\xAA\xE3\x81\x97";

struct PopupTextUnit {
    std::string text;
    bool newline = false;
};

struct PopupTextLine {
    std::string text;
    int startUnit = 0;
    int unitCount = 0;
};

int stageValue(EncyclopediaStage stage)
{
    return static_cast<int>(stage);
}

bool isNoEffectKey(std::string_view key)
{
    return key == "none" || key == "None" || key == "NONE" || key == NoEffectText;
}

bool isNoEffectText(std::string_view text)
{
    return text == NoEffectText || text == "none" || text == "None" || text == "NONE";
}

bool inlinePopupIconTagAt(std::string_view text, std::size_t offset, std::size_t& outEnd)
{
    constexpr std::string_view ItemPrefix = "{item:";
    constexpr std::string_view WorldPrefix = "{world:";

    std::string_view prefix;
    if (offset + ItemPrefix.size() < text.size() && text.substr(offset, ItemPrefix.size()) == ItemPrefix) {
        prefix = ItemPrefix;
    } else if (offset + WorldPrefix.size() < text.size() && text.substr(offset, WorldPrefix.size()) == WorldPrefix) {
        prefix = WorldPrefix;
    } else {
        return false;
    }

    const std::size_t close = text.find('}', offset + prefix.size());
    if (close == std::string_view::npos || close == offset + prefix.size()) {
        return false;
    }
    outEnd = close + 1;
    return true;
}

std::size_t utf8CodepointSize(std::string_view text, std::size_t offset)
{
    const unsigned char c = static_cast<unsigned char>(text[offset]);
    std::size_t count = 1;
    if ((c & 0x80u) == 0x00u) {
        count = 1;
    } else if ((c & 0xE0u) == 0xC0u) {
        count = 2;
    } else if ((c & 0xF0u) == 0xE0u) {
        count = 3;
    } else if ((c & 0xF8u) == 0xF0u) {
        count = 4;
    }
    return std::min(count, text.size() - offset);
}

std::vector<PopupTextUnit> popupTextUnits(std::string_view text)
{
    std::vector<PopupTextUnit> units;
    for (std::size_t offset = 0; offset < text.size();) {
        std::size_t tagEnd = 0;
        if (inlinePopupIconTagAt(text, offset, tagEnd)) {
            units.push_back({std::string(text.substr(offset, tagEnd - offset)), false});
            offset = tagEnd;
            continue;
        }

        const std::size_t codepointSize = utf8CodepointSize(text, offset);
        const std::string_view codepoint = text.substr(offset, codepointSize);
        if (codepoint == "\r") {
            offset += codepointSize;
            continue;
        }
        if (codepoint == "\n") {
            units.push_back({{}, true});
            offset += codepointSize;
            continue;
        }
        units.push_back({std::string(codepoint), false});
        offset += codepointSize;
    }
    return units;
}

int popupRevealUnitCount(std::string_view text)
{
    int count = 0;
    for (const PopupTextUnit& unit : popupTextUnits(text)) {
        if (!unit.newline) {
            ++count;
        }
    }
    return count;
}

float popupLineHeight(Renderer& renderer)
{
    return std::max(24.0f, renderer.measureText("あ", PopupTextScale).y + PopupLineGap);
}

InlineItemTextStyle popupTextStyle()
{
    InlineItemTextStyle style;
    style.text = {246, 246, 252, 255};
    style.scale = PopupTextScale;
    style.iconTextGap = 4.0f;
    style.iconScale = 1.15f;
    style.outlineEnabled = true;
    style.outline = {0, 0, 0, 210};
    style.outlinePx = 2;
    return style;
}

std::vector<PopupTextLine> layoutPopupText(
    Renderer& renderer,
    std::string_view text,
    float maxWidth,
    const InlineItemTextStyle& style)
{
    std::vector<PopupTextLine> lines;
    std::string line;
    int lineStart = 0;
    int lineUnits = 0;
    int nextUnit = 0;

    const auto pushLine = [&]() {
        lines.push_back(PopupTextLine{
            .text = line,
            .startUnit = lineStart,
            .unitCount = lineUnits,
        });
        line.clear();
        lineUnits = 0;
        lineStart = nextUnit;
    };

    for (const PopupTextUnit& unit : popupTextUnits(text)) {
        if (unit.newline) {
            pushLine();
            continue;
        }

        const std::string candidate = line + unit.text;
        if (!line.empty() && measureInlineItemText(renderer, candidate, style).x > maxWidth) {
            pushLine();
        }
        if (line.empty()) {
            lineStart = nextUnit;
        }
        line += unit.text;
        ++lineUnits;
        ++nextUnit;
    }

    if (!line.empty() || lines.empty()) {
        pushLine();
    }
    return lines;
}

std::string visiblePopupLineText(std::string_view line, int visibleUnits)
{
    if (visibleUnits <= 0) {
        return {};
    }

    std::string visible;
    int count = 0;
    for (const PopupTextUnit& unit : popupTextUnits(line)) {
        if (unit.newline) {
            continue;
        }
        if (count >= visibleUnits) {
            break;
        }
        visible += unit.text;
        ++count;
    }
    return visible;
}

float popupTextBlockHeight(Renderer& renderer, std::size_t lineCount)
{
    if (lineCount == 0) {
        return 0.0f;
    }
    return popupLineHeight(renderer) * static_cast<float>(lineCount) - PopupLineGap;
}

UiRect expandedRect(UiRect rect, float padding)
{
    rect.pos -= Vec2{padding, padding};
    rect.size += Vec2{padding * 2.0f, padding * 2.0f};
    return rect;
}

float rectOverlapArea(UiRect a, UiRect b)
{
    const float left = std::max(a.pos.x, b.pos.x);
    const float top = std::max(a.pos.y, b.pos.y);
    const float right = std::min(a.pos.x + a.size.x, b.pos.x + b.size.x);
    const float bottom = std::min(a.pos.y + a.size.y, b.pos.y + b.size.y);
    if (right <= left || bottom <= top) {
        return 0.0f;
    }
    return (right - left) * (bottom - top);
}

float totalAvoidOverlap(UiRect popup, std::span<const UiRect> avoidRects)
{
    float total = 0.0f;
    for (UiRect avoid : avoidRects) {
        if (avoid.size.x <= 0.0f || avoid.size.y <= 0.0f) {
            continue;
        }
        total += rectOverlapArea(popup, expandedRect(avoid, PopupAvoidPadding));
    }
    return total;
}

float popupFadeAlpha(float elapsed, float revealSeconds)
{
    if (PopupFadeInSeconds > 0.0f && elapsed < PopupFadeInSeconds) {
        return std::clamp(elapsed / PopupFadeInSeconds, 0.0f, 1.0f);
    }

    const float fadeOutStart = PopupFadeInSeconds + revealSeconds + PopupHoldAfterRevealSeconds;
    if (PopupFadeOutSeconds > 0.0f && elapsed > fadeOutStart) {
        return 1.0f - std::clamp((elapsed - fadeOutStart) / PopupFadeOutSeconds, 0.0f, 1.0f);
    }

    return 1.0f;
}

float popupRevealElapsed(float elapsed)
{
    return std::max(0.0f, elapsed - PopupFadeInSeconds);
}

float popupTotalDuration(float revealSeconds)
{
    return PopupFadeInSeconds + revealSeconds + PopupHoldAfterRevealSeconds + PopupFadeOutSeconds;
}

float clampPopupAxis(float value, float minValue, float maxValue)
{
    if (maxValue < minValue) {
        return minValue;
    }
    return std::clamp(value, minValue, maxValue);
}

Vec2 clampPopupPosition(Vec2 pos, Vec2 size, const Camera& camera)
{
    const float screenWidth = static_cast<float>(camera.width());
    const float screenHeight = static_cast<float>(camera.height());
    pos.x = clampPopupAxis(pos.x, PopupScreenMargin, screenWidth - size.x - PopupScreenMargin);
    pos.y = clampPopupAxis(pos.y, PopupTopMargin, screenHeight - size.y - PopupScreenMargin);
    return pos;
}

Vec2 choosePopupPosition(
    Vec2 anchor,
    Vec2 desired,
    Vec2 size,
    const Camera& camera,
    std::span<const UiRect> avoidRects)
{
    const std::array<Vec2, 8> candidates{{
        desired,
        anchor + Vec2{14.0f, 18.0f},
        anchor + Vec2{-size.x - 14.0f, -34.0f},
        anchor + Vec2{-size.x - 14.0f, 18.0f},
        anchor + Vec2{-size.x * 0.5f, -size.y - 18.0f},
        anchor + Vec2{-size.x * 0.5f, 18.0f},
        anchor + Vec2{18.0f, -size.y * 0.5f},
        anchor + Vec2{-size.x - 18.0f, -size.y * 0.5f},
    }};

    Vec2 best = clampPopupPosition(desired, size, camera);
    float bestScore = std::numeric_limits<float>::max();
    for (Vec2 candidate : candidates) {
        const Vec2 pos = clampPopupPosition(candidate, size, camera);
        const UiRect rect{pos, size};
        const float overlap = totalAvoidOverlap(rect, avoidRects);
        const float movement = lengthSquared(pos - desired);
        const float score = overlap * 100000.0f + movement;
        if (score < bestScore) {
            best = pos;
            bestScore = score;
        }
    }
    return best;
}

void drawDiscoveryPopupBackdrop(Renderer& renderer, UiRect rect)
{
    const float halfWidth = rect.size.x * 0.5f;
    const Color transparent{0, 0, 0, 0};
    const Color center{0, 0, 0, 188};
    renderer.fillGradientRect(rect.pos, {halfWidth, rect.size.y}, transparent, center, GradientDirection::LeftToRight);
    renderer.fillGradientRect(
        rect.pos + Vec2{halfWidth, 0.0f},
        {halfWidth, rect.size.y},
        center,
        transparent,
        GradientDirection::LeftToRight);
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
    activePopups_.clear();
    updateLog_.clear();
}

void EncyclopediaSystem::update(float dt)
{
    for (Popup& popup : activePopups_) {
        if (popup.duration > 0.0f) {
            popup.elapsed += std::max(0.0f, dt);
        }
    }
    activePopups_.erase(
        std::remove_if(activePopups_.begin(), activePopups_.end(), [](const Popup& popup) {
            return popup.duration <= 0.0f || popup.elapsed >= popup.duration;
        }),
        activePopups_.end());

    while (static_cast<int>(activePopups_.size()) < MaxActiveDiscoveryPopups && !queuedPopups_.empty()) {
        Popup popup = queuedPopups_.front();
        queuedPopups_.pop_front();
        popup.elapsed = 0.0f;
        popup.screenPositionLocked = false;
        activePopups_.push_back(std::move(popup));
    }
}

void EncyclopediaSystem::renderPopups(
    Renderer& renderer,
    const Camera& camera,
    const ObjectCatalog& catalog,
    std::span<const UiRect> avoidRects)
{
    if (activePopups_.empty()) {
        return;
    }

    const InlineItemTextStyle textStyle = popupTextStyle();
    const float contentWidth = std::max(0.0f, PopupWidth - PopupPaddingX * 2.0f);
    std::vector<UiRect> popupAvoidRects(avoidRects.begin(), avoidRects.end());

    renderer.setScreenSpace();
    for (Popup& popup : activePopups_) {
        if (popup.duration <= 0.0f || popup.text.empty()) {
            continue;
        }

        const std::vector<PopupTextLine> lines = layoutPopupText(renderer, popup.text, contentWidth, textStyle);
        const Vec2 size{
            PopupWidth,
            std::max(PopupMinHeight, popupTextBlockHeight(renderer, lines.size()) + PopupPaddingY * 2.0f),
        };
        const Vec2 anchor = camera.worldToScreen(popup.position);
        const Vec2 desired = anchor + Vec2{14.0f, -34.0f};
        UiRect panel{clampPopupPosition(popup.screenPosition, size, camera), size};
        if (!popup.screenPositionLocked || totalAvoidOverlap(panel, popupAvoidRects) > 0.0f) {
            popup.screenPosition = choosePopupPosition(anchor, desired, size, camera, popupAvoidRects);
            popup.screenPositionLocked = true;
            panel = {popup.screenPosition, size};
        } else {
            popup.screenPosition = panel.pos;
        }

        const float alpha = popupFadeAlpha(popup.elapsed, popup.revealSeconds);
        renderer.pushScreenTransform({0.0f, 0.0f}, 1.0f, alpha);

        drawDiscoveryPopupBackdrop(renderer, panel);

        const float revealElapsed = popupRevealElapsed(popup.elapsed);
        const int visibleUnits = revealElapsed >= popup.revealSeconds
            ? popup.revealUnitCount
            : std::clamp(static_cast<int>(std::floor(revealElapsed * PopupRevealUnitsPerSecond)), 0, popup.revealUnitCount);
        const float lineHeight = popupLineHeight(renderer);
        Vec2 linePos = panel.pos + Vec2{PopupPaddingX, PopupPaddingY};
        for (const PopupTextLine& line : lines) {
            const int lineVisibleUnits = std::clamp(visibleUnits - line.startUnit, 0, line.unitCount);
            const std::string visibleLine = visiblePopupLineText(line.text, lineVisibleUnits);
            if (!visibleLine.empty()) {
                drawInlineItemText(renderer, catalog, linePos, visibleLine, textStyle);
            }
            linePos.y += lineHeight;
        }
        renderer.popScreenTransform();

        popupAvoidRects.push_back(panel);
    }
}

void EncyclopediaSystem::noteItemDiscovered(const ObjectDefinition& object, Vec2 position)
{
    raiseObjectStage(object, EncyclopediaStage::Discovered, position, false);
}

bool EncyclopediaSystem::noteItemObtained(const ObjectDefinition& object, Vec2 position)
{
    return raiseObjectStage(object, EncyclopediaStage::Obtained, position, false);
}

void EncyclopediaSystem::noteItemEquipped(const ObjectDefinition& object, Vec2 position)
{
    raiseObjectStage(object, EncyclopediaStage::Equipped, position, false);
}

void EncyclopediaSystem::noteItemEffect(const ObjectDefinition& object, std::string_view effectKey, std::string_view description, Vec2 position)
{
    std::optional<EffectPopupLine> line = recordObjectEffect(
        object,
        effectKey,
        description,
        {},
        position,
        {},
        true);
    if (!line.has_value()) {
        return;
    }

    const std::array<EffectPopupLine, 1> lines{std::move(*line)};
    enqueueEffectPopup(lines);
}

void EncyclopediaSystem::noteEffectEvent(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog)
{
    noteEffectEvents(std::span<const EffectDiscoveryEvent>(&event, 1), catalog);
}

void EncyclopediaSystem::noteEffectEvents(std::span<const EffectDiscoveryEvent> events, const ObjectCatalog& catalog)
{
    struct PopupGroup {
        std::vector<EffectPopupLine> lines;
    };

    std::vector<PopupGroup> groups;
    std::unordered_map<std::string, std::size_t> groupIndexByObjectId;

    for (const EffectDiscoveryEvent& event : events) {
        std::optional<EffectPopupLine> line = recordEffectDiscovery(event, catalog);
        if (!line.has_value()) {
            continue;
        }

        auto [it, inserted] = groupIndexByObjectId.emplace(line->objectId, groups.size());
        if (inserted) {
            groups.push_back({});
        }
        groups[it->second].lines.push_back(std::move(*line));
    }

    for (const PopupGroup& group : groups) {
        enqueueEffectPopup(group.lines);
    }
}

bool EncyclopediaSystem::discoverObjectEffect(
    std::string_view objectId,
    std::string_view effectKey,
    const ObjectCatalog& catalog,
    Vec2 worldPosition,
    std::string_view optionalNote)
{
    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr) {
        return false;
    }

    std::optional<EffectPopupLine> line = recordObjectEffect(
        *object,
        effectKey,
        {},
        optionalNote,
        worldPosition,
        {},
        false);
    if (!line.has_value()) {
        return false;
    }

    const std::array<EffectPopupLine, 1> lines{std::move(*line)};
    enqueueEffectPopup(lines);
    return true;
}

std::optional<EncyclopediaSystem::EffectPopupLine> EncyclopediaSystem::recordObjectEffect(
    const ObjectDefinition& object,
    std::string_view effectKey,
    std::string_view description,
    std::string_view note,
    Vec2 position,
    std::string_view objectNameOverride,
    bool allowGenericFallback)
{
    const std::string canonicalKey = canonicalEffectKey(effectKey);
    if (object.id.empty() || canonicalKey.empty() || isNoEffectKey(canonicalKey) || isNoEffectText(description)) {
        return std::nullopt;
    }

    const std::size_t lineIndex = findEffectLineIndexByKey(object, canonicalKey);
    const bool hasCatalogLine = lineIndex != object.discoveryEffectLines.size();
    std::string lineText;
    if (hasCatalogLine) {
        lineText = object.discoveryEffectLines[lineIndex].text;
    } else if (!description.empty()) {
        lineText = std::string(description);
    } else if (allowGenericFallback) {
        lineText = fallbackEffectDescription(canonicalKey);
    } else {
        return std::nullopt;
    }

    if (lineText.empty()) {
        lineText = fallbackEffectDescription(canonicalKey);
    }
    if (isNoEffectText(lineText)) {
        return std::nullopt;
    }

    auto& effects = objectEffects_[object.id];
    if (!effects.insert(canonicalKey).second) {
        return std::nullopt;
    }

    raiseObjectStage(object, EncyclopediaStage::EffectTriggered, position, false);
    const std::string itemName = objectNameOverride.empty()
        ? (object.name.empty() ? object.id : object.name)
        : std::string(objectNameOverride);
    return EffectPopupLine{
        .objectId = object.id,
        .objectName = itemName,
        .lineText = std::move(lineText),
        .note = std::string(note),
        .position = position,
    };
}

std::optional<EncyclopediaSystem::EffectPopupLine> EncyclopediaSystem::recordEffectDiscovery(
    const EffectDiscoveryEvent& event,
    const ObjectCatalog& catalog)
{
    if (event.objectId.empty()) {
        return std::nullopt;
    }

    const ObjectDefinition* object = catalog.registry.findById(event.objectId);
    if (object == nullptr) {
        return std::nullopt;
    }
    return recordObjectEffect(
        *object,
        event.effectKey,
        event.description,
        event.note,
        event.position,
        event.objectName,
        false);
}

void EncyclopediaSystem::enqueueEffectPopup(std::span<const EffectPopupLine> lines)
{
    if (lines.empty()) {
        return;
    }

    const EffectPopupLine& first = lines.front();
    std::string popup = "効果判明：" + inlineItemTag(first.objectId) + " " + first.objectName;
    std::vector<std::string> notes;
    for (const EffectPopupLine& line : lines) {
        popup += "\n";
        popup += "・";
        popup += line.lineText;
        if (!line.note.empty() && std::find(notes.begin(), notes.end(), line.note) == notes.end()) {
            notes.push_back(line.note);
        }
    }
    for (const std::string& note : notes) {
        popup += "\n";
        popup += note;
    }
    enqueuePopup(std::move(popup), first.position);
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
    if (objectId.empty() || effectKey.empty() || isNoEffectKey(effectKey)) {
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
        lines.emplace_back(NoEffectText);
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
    const int revealUnits = popupRevealUnitCount(text);
    const float revealSeconds = static_cast<float>(revealUnits) / PopupRevealUnitsPerSecond;
    queuedPopups_.push_back(Popup{
        .text = std::move(text),
        .position = position,
        .elapsed = 0.0f,
        .duration = popupTotalDuration(revealSeconds),
        .revealSeconds = revealSeconds,
        .revealUnitCount = revealUnits,
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
