#include "game/EncyclopediaSystem.hpp"

#include "engine/Ui.hpp"
#include "game/EntityStatusVisuals.hpp"
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
constexpr float PopupHoldAfterRevealSeconds = 4.5f;
constexpr float PopupRevealUnitsPerSecond = 102.0f;
constexpr int PopupTextScale = 2;
constexpr float PopupMinPlayerDistance = 144.0f;
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

float pointToRectDistance(Vec2 point, UiRect rect)
{
    const float right = rect.pos.x + rect.size.x;
    const float bottom = rect.pos.y + rect.size.y;
    const float dx = std::max({rect.pos.x - point.x, 0.0f, point.x - right});
    const float dy = std::max({rect.pos.y - point.y, 0.0f, point.y - bottom});
    return std::sqrt(dx * dx + dy * dy);
}

float popupTotalDuration(float revealSeconds)
{
    return revealSeconds + PopupHoldAfterRevealSeconds;
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
    Vec2 playerScreenPosition,
    std::span<const UiRect> avoidRects)
{
    constexpr float DiagonalDistance = PopupMinPlayerDistance * 0.70710678118f;
    const std::array<Vec2, 16> candidates{{
        desired,
        anchor + Vec2{14.0f, 18.0f},
        anchor + Vec2{-size.x - 14.0f, -34.0f},
        anchor + Vec2{-size.x - 14.0f, 18.0f},
        anchor + Vec2{-size.x * 0.5f, -size.y - 18.0f},
        anchor + Vec2{-size.x * 0.5f, 18.0f},
        anchor + Vec2{18.0f, -size.y * 0.5f},
        anchor + Vec2{-size.x - 18.0f, -size.y * 0.5f},
        playerScreenPosition + Vec2{PopupMinPlayerDistance, -size.y * 0.5f},
        playerScreenPosition + Vec2{-PopupMinPlayerDistance - size.x, -size.y * 0.5f},
        playerScreenPosition + Vec2{-size.x * 0.5f, PopupMinPlayerDistance},
        playerScreenPosition + Vec2{-size.x * 0.5f, -PopupMinPlayerDistance - size.y},
        playerScreenPosition + Vec2{DiagonalDistance, -DiagonalDistance - size.y},
        playerScreenPosition + Vec2{-DiagonalDistance - size.x, -DiagonalDistance - size.y},
        playerScreenPosition + Vec2{DiagonalDistance, DiagonalDistance},
        playerScreenPosition + Vec2{-DiagonalDistance - size.x, DiagonalDistance},
    }};

    Vec2 best = clampPopupPosition(desired, size, camera);
    float bestPlayerShortfall = std::numeric_limits<float>::max();
    float bestOverlap = std::numeric_limits<float>::max();
    float bestMovement = std::numeric_limits<float>::max();
    for (Vec2 candidate : candidates) {
        const Vec2 pos = clampPopupPosition(candidate, size, camera);
        const UiRect rect{pos, size};
        const float playerDistance = pointToRectDistance(playerScreenPosition, rect);
        const float playerShortfall = std::max(0.0f, PopupMinPlayerDistance - playerDistance);
        const float overlap = totalAvoidOverlap(rect, avoidRects);
        const float movement = lengthSquared(pos - desired);
        const bool better =
            playerShortfall < bestPlayerShortfall ||
            (playerShortfall == bestPlayerShortfall && overlap < bestOverlap) ||
            (playerShortfall == bestPlayerShortfall && overlap == bestOverlap && movement < bestMovement);
        if (better) {
            best = pos;
            bestPlayerShortfall = playerShortfall;
            bestOverlap = overlap;
            bestMovement = movement;
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
        std::string statusKey(effectKey);
        constexpr std::string_view ChanceSuffix = "_chance";
        if (statusKey.size() > ChanceSuffix.size() &&
            std::string_view(statusKey).substr(statusKey.size() - ChanceSuffix.size()) == ChanceSuffix) {
            statusKey.resize(statusKey.size() - ChanceSuffix.size());
        }
        const std::string_view displayName = entityStatusDisplayName(statusKey);
        if (displayName.empty()) {
            return "状態異常を付与する";
        }
        if (statusKey == "status_defense_down") {
            return effectKey.ends_with(ChanceSuffix)
                ? "確率で敵の防御を低下させる"
                : "敵の防御を低下させる";
        }
        return effectKey.ends_with(ChanceSuffix)
            ? "確率で敵を" + std::string(displayName) + "にする"
            : "敵を" + std::string(displayName) + "にする";
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

std::optional<EncyclopediaPopupStartedEvent> EncyclopediaSystem::update(float dt, bool allowPopupStart)
{
    if (!activePopups_.empty()) {
        Popup& active = activePopups_.front();
        if (active.duration > 0.0f) {
            active.elapsed += std::max(0.0f, dt);
        }
        if (active.duration <= 0.0f || active.elapsed >= active.duration) {
            activePopups_.clear();
        }
    }

    if (allowPopupStart && activePopups_.empty() && !queuedPopups_.empty()) {
        Popup popup = std::move(queuedPopups_.front());
        queuedPopups_.pop_front();
        popup.elapsed = 0.0f;
        popup.screenPositionLocked = false;
        activePopups_.push_back(std::move(popup));
        const Popup& active = activePopups_.front();
        if (active.cue != EncyclopediaPopupCue::None) {
            return EncyclopediaPopupStartedEvent{
                .cue = active.cue,
                .position = active.position,
            };
        }
    }
    return std::nullopt;
}

void EncyclopediaSystem::renderPopups(
    Renderer& renderer,
    const Camera& camera,
    const ObjectCatalog& catalog,
    Vec2 playerWorldPosition,
    std::span<const UiRect> avoidRects)
{
    if (activePopups_.empty()) {
        return;
    }

    const InlineItemTextStyle textStyle = popupTextStyle();
    const float contentWidth = std::max(0.0f, PopupWidth - PopupPaddingX * 2.0f);
    const Vec2 playerScreenPosition = camera.worldToScreen(playerWorldPosition);

    renderer.setScreenSpace();
    Popup& popup = activePopups_.front();
    if (popup.duration <= 0.0f || popup.text.empty()) {
        return;
    }

    if (!popup.layoutReady) {
        const std::vector<PopupTextLine> lines = layoutPopupText(renderer, popup.text, contentWidth, textStyle);
        popup.layoutLines.clear();
        popup.layoutLines.reserve(lines.size());
        for (const PopupTextLine& line : lines) {
            popup.layoutLines.push_back({line.text, line.startUnit, line.unitCount});
        }
        popup.baseSize = {
            PopupWidth,
            std::max(PopupMinHeight, popupTextBlockHeight(renderer, popup.layoutLines.size()) + PopupPaddingY * 2.0f),
        };
        popup.layoutReady = true;
    }

    const Vec2 baseSize = popup.baseSize;
    if (!popup.screenPositionLocked) {
        const Vec2 anchor = camera.worldToScreen(popup.position);
        const Vec2 desired = anchor + Vec2{14.0f, -34.0f};
        popup.screenPosition = choosePopupPosition(
            anchor,
            desired,
            baseSize,
            camera,
            playerScreenPosition,
            avoidRects);
        popup.screenPositionLocked = true;
    }

    const UiRect panel{popup.screenPosition, baseSize};
    drawDiscoveryPopupBackdrop(renderer, panel);

    const int visibleUnits = popup.elapsed >= popup.revealSeconds
        ? popup.revealUnitCount
        : std::clamp(static_cast<int>(std::floor(popup.elapsed * PopupRevealUnitsPerSecond)), 0, popup.revealUnitCount);
    const float lineHeight = popupLineHeight(renderer);
    Vec2 linePos = panel.pos + Vec2{PopupPaddingX, PopupPaddingY};
    for (const PopupLayoutLine& line : popup.layoutLines) {
        const int lineVisibleUnits = std::clamp(visibleUnits - line.startUnit, 0, line.unitCount);
        const std::string visibleLine = visiblePopupLineText(line.text, lineVisibleUnits);
        if (!visibleLine.empty()) {
            drawInlineItemText(renderer, catalog, linePos, visibleLine, textStyle);
        }
        linePos.y += lineHeight;
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

bool EncyclopediaSystem::noteEffectEvent(const EffectDiscoveryEvent& event, const ObjectCatalog& catalog)
{
    return noteEffectEvents(std::span<const EffectDiscoveryEvent>(&event, 1), catalog) > 0;
}

std::size_t EncyclopediaSystem::noteEffectEvents(std::span<const EffectDiscoveryEvent> events, const ObjectCatalog& catalog)
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
    return groups.size();
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
    if (object.id.empty() ||
        isCodexHiddenObject(object) ||
        canonicalKey.empty() ||
        isNoEffectKey(canonicalKey) ||
        isNoEffectText(description)) {
        return std::nullopt;
    }

    const std::size_t lineIndex = findEffectLineIndexByKey(object, canonicalKey);
    const bool hasCatalogLine = lineIndex != object.discoveryEffectLines.size();
    if (hasCatalogLine && object.discoveryEffectLines[lineIndex].trigger == DiscoveryTrigger::NormalEffect) {
        return std::nullopt;
    }
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
    enqueuePopup(std::move(popup), first.position, EncyclopediaPopupCue::EffectDiscovery);
}

void EncyclopediaSystem::noteEnemyDiscovered(std::string_view enemyId, std::string_view enemyName, Vec2 position)
{
    raiseEnemyStage(enemyId, enemyName, EncyclopediaStage::Discovered, position, false);
}

void EncyclopediaSystem::noteEnemyDefeated(std::string_view enemyId, std::string_view enemyName, Vec2 position)
{
    raiseEnemyStage(enemyId, enemyName, EncyclopediaStage::Discovered, position, false);
}

bool EncyclopediaSystem::noteEnemyInspected(const EnemyDefinition& enemy, Vec2 position)
{
    if (enemy.id.empty()) {
        return false;
    }

    const std::string name = enemy.name.empty() ? enemy.id : enemy.name;
    if (!raiseEnemyStage(enemy.id, name, EncyclopediaStage::Complete, position, false)) {
        return false;
    }

    std::string popup = "モンスター発見：" + name;
    popup += "\nHP ";
    popup += std::to_string(std::max(1, enemy.hp));
    popup += " / 攻撃 ";
    popup += std::to_string(std::max(0, enemy.contactAttackPower));
    popup += "\n";
    popup += enemy.description.empty() ? "-" : enemy.description;
    enqueuePopup(std::move(popup), position, EncyclopediaPopupCue::MonsterDiscovery);
    return true;
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

ObjectEffectDisplaySections EncyclopediaSystem::buildObjectEffectDisplaySections(
    std::string_view objectId,
    const ObjectCatalog& catalog,
    EffectRevealMode ringRevealMode) const
{
    ObjectEffectDisplaySections sections;
    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr || object->discoveryEffectLines.empty()) {
        return sections;
    }

    const auto discoveredIt = objectEffects_.find(std::string(objectId));
    const std::unordered_set<std::string>* discovered = discoveredIt != objectEffects_.end()
        ? &discoveredIt->second
        : nullptr;

    for (const DiscoveryEffectLine& line : object->discoveryEffectLines) {
        if (line.text.empty()) {
            continue;
        }
        if (line.trigger == DiscoveryTrigger::NormalEffect) {
            sections.useLines.push_back({line.effectKey, line.text});
            continue;
        }

        bool visible = false;
        if (discovered != nullptr) {
            const std::string canonical = canonicalEffectKey(line.effectKey);
            visible = discovered->contains(line.effectKey) || discovered->contains(canonical);
        }
        if (ringRevealMode == EffectRevealMode::DebugAll || visible) {
            sections.ringLines.push_back({line.effectKey, line.text});
        } else if (ringRevealMode == EffectRevealMode::WithUnknown) {
            sections.ringLines.push_back({line.effectKey, "？？？"});
        }
    }

    return sections;
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
    const auto matchesKey = [&canonical](const DiscoveryEffectLine& line) {
        return line.effectKey == canonical || canonicalEffectKey(line.effectKey) == canonical;
    };
    auto it = std::find_if(
        object.discoveryEffectLines.begin(),
        object.discoveryEffectLines.end(),
        [&matchesKey](const DiscoveryEffectLine& line) {
            return line.trigger != DiscoveryTrigger::NormalEffect && matchesKey(line);
        });
    if (it == object.discoveryEffectLines.end()) {
        it = std::find_if(
            object.discoveryEffectLines.begin(),
            object.discoveryEffectLines.end(),
            matchesKey);
    }
    if (it == object.discoveryEffectLines.end()) {
        return object.discoveryEffectLines.size();
    }
    return static_cast<std::size_t>(std::distance(object.discoveryEffectLines.begin(), it));
}

bool EncyclopediaSystem::raiseObjectStage(const ObjectDefinition& object, EncyclopediaStage stage, Vec2 position, bool popup)
{
    if (object.id.empty() || isCodexHiddenObject(object)) {
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
        enqueuePopup(log, position, EncyclopediaPopupCue::None);
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
        enqueuePopup(log, position, EncyclopediaPopupCue::None);
    }
    return true;
}

void EncyclopediaSystem::enqueuePopup(std::string text, Vec2 position, EncyclopediaPopupCue cue)
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
        .cue = cue,
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
