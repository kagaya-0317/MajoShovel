#include "game/GameInternal.hpp"

#include "engine/Audio.hpp"

namespace majo {

namespace {

constexpr float DebugItemPickerPanelMaxWidth = 1200.0f;
constexpr float DebugItemPickerPanelMaxHeight = 660.0f;
constexpr float DebugItemPickerPanelMargin = 24.0f;
constexpr float DebugItemPickerDetailWidth = 332.0f;
constexpr float DebugItemPickerPaneGap = 18.0f;
constexpr float DebugItemPickerButtonAreaHeight = 66.0f;
constexpr float DebugItemPickerCardWidth = 118.0f;
constexpr float DebugItemPickerCardHeight = 106.0f;
constexpr float DebugItemPickerCardGap = 10.0f;
constexpr float DebugItemPickerIconSize = 58.0f;
constexpr float DebugStoryTestDetailWidth = 360.0f;
constexpr float DebugStoryTestRowHeight = 56.0f;
constexpr float DebugStoryTestRowGap = 5.0f;
constexpr std::string_view DebugFinalStoryStageId = "stage_03_star_core";
constexpr std::string_view DebugEndingSeenFlag = "ending_seen";
constexpr std::string_view DebugEndingMainFlag = "story_ending_main";
constexpr std::string_view DebugStage03ClearFlag = "story_stage_03_clear";
constexpr std::string_view DebugPostEndingIntroFlag = "story_post_ending_intro";
constexpr std::string_view AudioCueEditManifestPath = "assets/audio/audio_manifest.tsv";
constexpr std::string_view AudioCueEditAudioRoot = "assets/audio";
constexpr float AudioCueEditPanelMaxWidth = 1200.0f;
constexpr float AudioCueEditPanelMaxHeight = 680.0f;
constexpr float AudioCueEditPanelMargin = 24.0f;
constexpr float AudioCueEditPaneGap = 14.0f;
constexpr float AudioCueEditCueListWidth = 330.0f;
constexpr float AudioCueEditDetailWidth = 330.0f;
constexpr float AudioCueEditButtonAreaHeight = 64.0f;
constexpr float AudioCueEditRowHeight = 44.0f;
constexpr float AudioCueEditRowGap = 4.0f;

struct DebugItemPickerLayout {
    UiRect panel{};
    UiRect grid{};
    UiRect detail{};
    int columns = 1;
    float rowHeight = DebugItemPickerCardHeight + DebugItemPickerCardGap;
};

struct DebugStoryTestLayout {
    UiRect panel{};
    UiRect list{};
    UiRect detail{};
    float rowPitch = DebugStoryTestRowHeight + DebugStoryTestRowGap;
};

struct AudioCueEditLayout {
    UiRect panel{};
    UiRect cueList{};
    UiRect fileList{};
    UiRect detail{};
    float rowPitch = AudioCueEditRowHeight + AudioCueEditRowGap;
};

struct AudioCueManifestRow {
    AudioCueEditEntry entry;
    bool valid = false;
};

UiScrollAreaStyle debugListScrollStyle(float wheelStep = 48.0f)
{
    UiScrollAreaStyle style;
    style.wheelStep = wheelStep;
    style.scrollbarWidth = 8.0f;
    style.scrollbarGap = 8.0f;
    style.scrollbarPaddingX = 6.0f;
    style.scrollbarPaddingY = 8.0f;
    style.scrollbarMinThumbHeight = 28.0f;
    style.scrollbarTrack = {255, 255, 255, 42};
    style.scrollbarThumb = {255, 230, 150, 190};
    style.outline = {126, 138, 168, 220};
    return style;
}

UiScrollableListStyle audioCueEditListStyle()
{
    UiScrollableListStyle style;
    style.rowHeight = AudioCueEditRowHeight;
    style.rowGap = AudioCueEditRowGap;
    style.rowInsetX = 8.0f;
    style.scroll = debugListScrollStyle(36.0f);
    return style;
}

UiRect audioCueEditListViewport(UiRect listPanel)
{
    constexpr float HeaderHeight = 36.0f;
    constexpr float BottomPadding = 8.0f;
    return {{
        listPanel.pos.x,
        listPanel.pos.y + HeaderHeight,
    }, {
        listPanel.size.x,
        std::max(1.0f, listPanel.size.y - HeaderHeight - BottomPadding),
    }};
}

DebugItemPickerLayout makeDebugItemPickerLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        DebugItemPickerPanelMaxWidth,
        std::max(760.0f, static_cast<float>(screenWidth) - DebugItemPickerPanelMargin * 2.0f));
    const float height = std::min(
        DebugItemPickerPanelMaxHeight,
        std::max(520.0f, static_cast<float>(screenHeight) - DebugItemPickerPanelMargin * 1.5f));

    DebugItemPickerLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(80.0f, body.size.y - DebugItemPickerButtonAreaHeight);
    layout.detail = {{
        body.pos.x + body.size.x - DebugItemPickerDetailWidth,
        body.pos.y,
    }, {DebugItemPickerDetailWidth, contentHeight}};
    layout.grid = {{
        body.pos.x,
        body.pos.y,
    }, {
        std::max(120.0f, layout.detail.pos.x - body.pos.x - DebugItemPickerPaneGap),
        contentHeight,
    }};

    const float pitch = DebugItemPickerCardWidth + DebugItemPickerCardGap;
    layout.columns = std::max(1, static_cast<int>((layout.grid.size.x + DebugItemPickerCardGap) / pitch));
    return layout;
}

DebugStoryTestLayout makeDebugStoryTestLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        DebugItemPickerPanelMaxWidth,
        std::max(760.0f, static_cast<float>(screenWidth) - DebugItemPickerPanelMargin * 2.0f));
    const float height = std::min(
        DebugItemPickerPanelMaxHeight,
        std::max(520.0f, static_cast<float>(screenHeight) - DebugItemPickerPanelMargin * 1.5f));

    DebugStoryTestLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(80.0f, body.size.y - DebugItemPickerButtonAreaHeight);
    layout.detail = {{
        body.pos.x + body.size.x - DebugStoryTestDetailWidth,
        body.pos.y,
    }, {DebugStoryTestDetailWidth, contentHeight}};
    layout.list = {{
        body.pos.x,
        body.pos.y,
    }, {
        std::max(220.0f, layout.detail.pos.x - body.pos.x - DebugItemPickerPaneGap),
        contentHeight,
    }};
    return layout;
}

AudioCueEditLayout makeAudioCueEditLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        AudioCueEditPanelMaxWidth,
        std::max(900.0f, static_cast<float>(screenWidth) - AudioCueEditPanelMargin * 2.0f));
    const float height = std::min(
        AudioCueEditPanelMaxHeight,
        std::max(540.0f, static_cast<float>(screenHeight) - AudioCueEditPanelMargin * 1.5f));

    AudioCueEditLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(120.0f, body.size.y - AudioCueEditButtonAreaHeight);
    layout.cueList = {body.pos, {AudioCueEditCueListWidth, contentHeight}};
    layout.detail = {{
        body.pos.x + body.size.x - AudioCueEditDetailWidth,
        body.pos.y,
    }, {AudioCueEditDetailWidth, contentHeight}};
    layout.fileList = {{
        layout.cueList.pos.x + layout.cueList.size.x + AudioCueEditPaneGap,
        body.pos.y,
    }, {
        std::max(180.0f, layout.detail.pos.x - (layout.cueList.pos.x + layout.cueList.size.x) - AudioCueEditPaneGap * 2.0f),
        contentHeight,
    }};
    return layout;
}

UiRect debugItemPickerCardRect(const DebugItemPickerLayout& layout, int index, float scrollOffset)
{
    const int columns = std::max(1, layout.columns);
    const int row = index / columns;
    const int column = index % columns;
    return {{
        layout.grid.pos.x + static_cast<float>(column) * (DebugItemPickerCardWidth + DebugItemPickerCardGap),
        layout.grid.pos.y + static_cast<float>(row) * layout.rowHeight - scrollOffset,
    }, {DebugItemPickerCardWidth, DebugItemPickerCardHeight}};
}

float debugItemPickerMaxScroll(const DebugItemPickerLayout& layout, int itemCount)
{
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    const float contentHeight = rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * DebugItemPickerCardHeight +
            static_cast<float>(rows - 1) * DebugItemPickerCardGap;
    return std::max(0.0f, contentHeight - layout.grid.size.y);
}

UiRect debugStoryTestRowRect(const DebugStoryTestLayout& layout, int index, float scrollOffset)
{
    return {{
        layout.list.pos.x,
        layout.list.pos.y + static_cast<float>(index) * layout.rowPitch - scrollOffset,
    }, {layout.list.size.x, DebugStoryTestRowHeight}};
}

float debugStoryTestMaxScroll(const DebugStoryTestLayout& layout, int itemCount)
{
    const float contentHeight = itemCount <= 0
        ? 0.0f
        : static_cast<float>(itemCount) * DebugStoryTestRowHeight +
            static_cast<float>(itemCount - 1) * DebugStoryTestRowGap;
    return std::max(0.0f, contentHeight - layout.list.size.y);
}

UiRect audioCueEditCloseButtonRect(UiRect panel)
{
    return uiBottomLeftButtonRect(panel, {130.0f, ui::ButtonHeight});
}

UiRect audioCueEditRescanButtonRect(UiRect panel)
{
    const UiRect close = audioCueEditCloseButtonRect(panel);
    return {{close.pos.x + close.size.x + 12.0f, close.pos.y}, {150.0f, ui::ButtonHeight}};
}

UiRect audioCueEditPreviewButtonRect(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return {{body.pos.x + body.size.x - 404.0f, body.pos.y + body.size.y - ui::ButtonHeight}, {126.0f, ui::ButtonHeight}};
}

UiRect audioCueEditApplyButtonRect(UiRect panel)
{
    const UiRect preview = audioCueEditPreviewButtonRect(panel);
    return {{preview.pos.x + preview.size.x + 12.0f, preview.pos.y}, {126.0f, ui::ButtonHeight}};
}

UiRect audioCueEditSaveButtonRect(UiRect panel)
{
    return uiBottomRightButtonRect(panel, {128.0f, ui::ButtonHeight});
}

void keepDebugItemPickerSelectionVisible(
    const DebugItemPickerLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = clamp(scrollOffset, 0.0f, debugItemPickerMaxScroll(layout, itemCount));
        return;
    }

    const UiRect rect = debugItemPickerCardRect(layout, selectedIndex, scrollOffset);
    const float top = layout.grid.pos.y;
    const float bottom = layout.grid.pos.y + layout.grid.size.y;
    if (rect.pos.y < top) {
        scrollOffset -= top - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > bottom) {
        scrollOffset += rect.pos.y + rect.size.y - bottom;
    }
    scrollOffset = clamp(scrollOffset, 0.0f, debugItemPickerMaxScroll(layout, itemCount));
}

void keepDebugStoryTestSelectionVisible(
    const DebugStoryTestLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = clamp(scrollOffset, 0.0f, debugStoryTestMaxScroll(layout, itemCount));
        return;
    }

    const UiRect rect = debugStoryTestRowRect(layout, selectedIndex, scrollOffset);
    const float top = layout.list.pos.y;
    const float bottom = layout.list.pos.y + layout.list.size.y;
    if (rect.pos.y < top) {
        scrollOffset -= top - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > bottom) {
        scrollOffset += rect.pos.y + rect.size.y - bottom;
    }
    scrollOffset = clamp(scrollOffset, 0.0f, debugStoryTestMaxScroll(layout, itemCount));
}

void keepAudioCueEditSelectionVisible(UiRect list, int selectedIndex, int itemCount, float& scrollOffset)
{
    keepUiScrollableListItemVisible(audioCueEditListViewport(list), selectedIndex, itemCount, scrollOffset, audioCueEditListStyle());
}

UiRect debugItemPickerAddButtonRect(UiRect panel)
{
    return uiBottomRightButtonRect(panel, {180.0f, ui::ButtonHeight});
}

UiRect debugItemPickerCloseButtonRect(UiRect panel)
{
    return uiBottomLeftButtonRect(panel, {150.0f, ui::ButtonHeight});
}

UiRect debugStoryTestPlayButtonRect(UiRect panel)
{
    return debugItemPickerAddButtonRect(panel);
}

UiRect debugStoryTestCloseButtonRect(UiRect panel)
{
    return debugItemPickerCloseButtonRect(panel);
}

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

std::vector<std::string> splitTabs(std::string_view line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.emplace_back(line.substr(start));
            break;
        }
        fields.emplace_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

bool parseFloatOrDefault(std::string_view text, float& outValue, float fallback)
{
    const std::string trimmed = trimAscii(std::string(text));
    if (trimmed.empty()) {
        outValue = fallback;
        return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str()) {
        outValue = fallback;
        return false;
    }
    outValue = parsed;
    return true;
}

bool parseBoolOrDefault(std::string_view text, bool fallback)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(text)));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

std::string_view fieldOrEmpty(const std::vector<std::string>& fields, std::size_t index)
{
    return index < fields.size() ? std::string_view(fields[index]) : std::string_view{};
}

std::string formatAudioFloat(float value)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    std::string text(buffer);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? std::string("0") : text;
}

std::string normalizeAudioRelativePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.rfind("./", 0) == 0) {
        path.erase(0, 2);
    }
    constexpr std::string_view AudioRootPrefix = "assets/audio/";
    if (path.rfind(AudioRootPrefix, 0) == 0) {
        path.erase(0, AudioRootPrefix.size());
    }
    return path;
}

std::string audioCueEditTypeText(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? "bgm" : "se";
}

std::string audioCueEditFolderText(AudioCueEditMode mode)
{
    return std::string(AudioCueEditAudioRoot) + "/" + audioCueEditTypeText(mode);
}

std::string audioCueEditTitle(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? "BGM編集" : "効果音編集";
}

AudioCueType audioCueEngineType(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? AudioCueType::Bgm : AudioCueType::Se;
}

bool audioCueEditEntryMatchesMode(const AudioCueEditEntry& entry, AudioCueEditMode mode)
{
    return lowerAscii(trimAscii(entry.type)) == audioCueEditTypeText(mode);
}

std::string audioCueDisplayName(const AudioCueEditEntry& entry)
{
    return entry.displayName.empty() ? entry.id : entry.displayName;
}

int audioCueEditFileIndexForPath(const std::vector<AudioCueFileEntry>& files, std::string path)
{
    const std::string normalized = lowerAscii(normalizeAudioRelativePath(trimAscii(std::move(path))));
    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        if (lowerAscii(normalizeAudioRelativePath(files[static_cast<std::size_t>(i)].relativePath)) == normalized) {
            return i;
        }
    }
    return -1;
}

AudioCueEditEntry parseAudioCueManifestEntry(const std::vector<std::string>& fields)
{
    AudioCueEditEntry entry;
    if (fields.size() >= 1) {
        entry.id = trimAscii(fields[0]);
    }
    if (fields.size() >= 2) {
        entry.type = lowerAscii(trimAscii(fields[1]));
    }
    if (fields.size() >= 3) {
        entry.path = normalizeAudioRelativePath(trimAscii(fields[2]));
    }
    if (fields.size() >= 7) {
        entry.displayName = trimAscii(fields[6]);
    }
    parseFloatOrDefault(fieldOrEmpty(fields, 3), entry.volume, 1.0f);
    entry.volume = clamp(entry.volume, 0.0f, 1.0f);
    entry.loop = parseBoolOrDefault(fieldOrEmpty(fields, 4), entry.type == "bgm");
    parseFloatOrDefault(fieldOrEmpty(fields, 5), entry.cooldownMs, 0.0f);
    entry.cooldownMs = std::max(0.0f, entry.cooldownMs);
    return entry;
}

bool loadAudioCueManifestRows(std::vector<AudioCueManifestRow>& rows, std::string& message)
{
    rows.clear();

    std::ifstream file(std::filesystem::path(std::string(AudioCueEditManifestPath)), std::ios::binary);
    if (!file) {
        message = "Audio manifest not found";
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            firstLine = false;
            stripUtf8Bom(line);
        }
        const std::string trimmed = trimAscii(line);
        if (trimmed.empty() || trimmed.rfind("#", 0) == 0) {
            continue;
        }

        std::vector<std::string> fields = splitTabs(line);
        if (!fields.empty() && lowerAscii(trimAscii(fields[0])) == "id") {
            continue;
        }
        if (fields.size() < 3) {
            continue;
        }

        AudioCueManifestRow row;
        row.entry = parseAudioCueManifestEntry(fields);
        row.valid = !row.entry.id.empty() && (row.entry.type == "bgm" || row.entry.type == "se");
        if (row.valid) {
            rows.push_back(std::move(row));
        }
    }

    message = "Audio manifest loaded";
    return true;
}

bool writeAudioCueManifestRows(const std::vector<AudioCueManifestRow>& rows, std::string& message)
{
    std::ofstream file(std::filesystem::path(std::string(AudioCueEditManifestPath)), std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "Audio manifest save failed";
        return false;
    }

    file << "\xEF\xBB\xBF";
    file << "id\ttype\tpath\tvolume\tloop\tcooldown_ms\tdisplay_name\n";
    for (const AudioCueManifestRow& row : rows) {
        if (!row.valid || row.entry.id.empty()) {
            continue;
        }
        file
            << row.entry.id << '\t'
            << row.entry.type << '\t'
            << normalizeAudioRelativePath(row.entry.path) << '\t'
            << formatAudioFloat(row.entry.volume) << '\t'
            << (row.entry.loop ? "true" : "false") << '\t'
            << formatAudioFloat(row.entry.cooldownMs) << '\t'
            << row.entry.displayName << '\n';
    }

    if (!file) {
        message = "Audio manifest write failed";
        return false;
    }
    message = "Audio manifest saved";
    return true;
}

std::string debugItemPickerDisplayName(const ItemData& item)
{
    return item.name.empty() ? item.id : item.name;
}

std::string debugItemPickerSubtitle(const ItemData& item)
{
    std::string text = item.category.empty() ? std::string("未分類") : item.category;
    if (item.rarity > 0) {
        text += " / R";
        text += std::to_string(item.rarity);
    }
    return text;
}

bool debugStoryTestIsTutorialTrigger(std::string_view trigger)
{
    constexpr std::string_view Prefix = "tutorial:";
    return trigger.size() >= Prefix.size() && trigger.substr(0, Prefix.size()) == Prefix;
}

std::string debugStoryTestDisplayTitle(const StoryEvent& event)
{
    return event.title.empty() ? event.id : event.title;
}

std::string debugStoryTestDisplayTrigger(const StoryEvent& event)
{
    return event.trigger.empty() ? std::string("triggerなし") : event.trigger;
}

std::string debugStoryTestDisplayOnceFlag(const StoryEvent& event)
{
    if (event.repeatable) {
        return "repeat";
    }
    return event.onceFlag.empty() ? std::string("onceなし") : event.onceFlag;
}

std::string debugStageIdForToken(std::string_view token)
{
    if (token == "stage1" || token == "stage-1" || token == "1" || token == "stage_01_stardust") {
        return "stage_01_stardust";
    }
    if (token == "stage2" || token == "stage-2" || token == "2" || token == "stage_02_junk_magic") {
        return "stage_02_junk_magic";
    }
    if (token == "stage3" || token == "stage-3" || token == "3" || token == "stage_03_star_core") {
        return "stage_03_star_core";
    }
    if (token == "stage4" || token == "stage-4" || token == "4" || token == "stage_04_astral_mine" || token == "astral") {
        return "stage_04_astral_mine";
    }
    return {};
}

bool debugObjectInstanceMatchesObjectId(const InventoryObjectInstance& instance, std::string_view objectId)
{
    return instance.instance.objectId == objectId || instance.item.id == objectId;
}

bool debugIsRoguelikeStageDefinition(const StageDefinition& stage)
{
    return stage.id == "stage_04_astral_mine" ||
        stage.type == "ローグライク" ||
        stage.generationProfile == "astral_rogue";
}

SpecialRoomType debugSpecialRoomTypeForToken(std::string_view token)
{
    if (token == "ore" || token == "ore-room") {
        return SpecialRoomType::OreRoom;
    }
    if (token == "safe" || token == "safe-cavern") {
        return SpecialRoomType::SafeCavern;
    }
    if (token == "coin" || token == "coin-room") {
        return SpecialRoomType::CoinRoom;
    }
    if (token == "treasure" || token == "treasure-room") {
        return SpecialRoomType::TreasureRoom;
    }
    if (token == "enemy" || token == "enemy-room") {
        return SpecialRoomType::EnemyRoom;
    }
    return SpecialRoomType::None;
}

const char* debugSpecialRoomTypeLabel(SpecialRoomType type)
{
    switch (type) {
    case SpecialRoomType::OreRoom:
        return "鉱石";
    case SpecialRoomType::SafeCavern:
        return "安全";
    case SpecialRoomType::CoinRoom:
        return "コイン";
    case SpecialRoomType::TreasureRoom:
        return "宝物";
    case SpecialRoomType::EnemyRoom:
        return "敵";
    case SpecialRoomType::None:
        break;
    }
    return "なし";
}

} // namespace

BaseEditRect Game::baseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect fallback) const
{
    const auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    const auto it = table.find(std::string(facilityId));
    if (it == table.end()) {
        return normalizeBaseEditRect(fallback);
    }
    return normalizeBaseEditRect(it->second);
}

void Game::setBaseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect rect)
{
    auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    table[std::string(facilityId)] = normalizeBaseEditRect(rect);
}

bool Game::isBasePassabilityBlocked(BaseArea area, int tileX, int tileY) const
{
    const auto& blocked = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
    return blocked.find(packBaseEditTile(tileX, tileY)) != blocked.end();
}

void Game::setBasePassabilityBlocked(BaseArea area, int tileX, int tileY, bool blocked)
{
    auto& table = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
    const std::int64_t key = packBaseEditTile(tileX, tileY);
    if (blocked) {
        table.insert(key);
    } else {
        table.erase(key);
    }
}

void Game::loadBaseEditData()
{
    baseFacilityRectsOutdoor_.clear();
    baseFacilityRectsHome_.clear();
    baseBlockedTilesOutdoor_.clear();
    baseBlockedTilesHome_.clear();
    baseEditDirty_ = false;

    for (BaseArea area : {BaseArea::Outdoor, BaseArea::HomeInterior}) {
        auto& facilityRects = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
        auto& blockedTiles = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
        const std::filesystem::path path = baseEditDataPath(area);
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string key;
            stream >> key;
            if (key == "facility") {
                std::string id;
                BaseEditRect rect{};
                stream >> id >> rect.x >> rect.y >> rect.w >> rect.h;
                if (!stream.fail() && !id.empty()) {
                    facilityRects[id] = normalizeBaseEditRect(rect);
                }
            } else if (key == "blocked") {
                int tileX = 0;
                int tileY = 0;
                stream >> tileX >> tileY;
                if (!stream.fail()) {
                    blockedTiles.insert(packBaseEditTile(tileX, tileY));
                }
            }
        }
    }
}

bool Game::saveBaseEditData(std::string& message)
{
    std::error_code error;
    std::filesystem::create_directories("data", error);
    if (error) {
        message = "Base edit save failed: could not create data directory";
        return false;
    }

    for (BaseArea area : {BaseArea::Outdoor, BaseArea::HomeInterior}) {
        const std::filesystem::path path = baseEditDataPath(area);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            message = "Base edit save failed: could not open " + path.string();
            return false;
        }

        file << "MAJO_BASE_EDIT_V1\n";

        std::vector<BaseFacility> facilities = baseFacilities(area, ringWorkshopUnlocked_);
        for (BaseFacility& facility : facilities) {
            const BaseEditRect rect = baseFacilityRectFor(area, facility.facilityId, toBaseEditRect(facility.rect));
            file << "facility "
                << facility.facilityId << " "
                << rect.x << " " << rect.y << " " << rect.w << " " << rect.h << "\n";
        }

        const auto& blocked = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
        std::vector<std::int64_t> blockedSorted(blocked.begin(), blocked.end());
        std::sort(blockedSorted.begin(), blockedSorted.end());
        for (const std::int64_t packed : blockedSorted) {
            file << "blocked " << baseEditTileXFromPacked(packed) << " " << baseEditTileYFromPacked(packed) << "\n";
        }

        if (!file) {
            message = "Base edit save failed while writing " + path.string();
            return false;
        }
    }

    baseEditDirty_ = false;
    message = "Base edit saved";
    return true;
}

bool Game::loadObjectImageScaleData()
{
    objectImageScaleById_.clear();
    otherImageScaleByKey_.clear();
    objectImageScaleDirty_ = false;
    objectImageScaleStatus_.clear();

    const std::filesystem::path path = objectImageScaleDataPath();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        rebuildObjectImageScaleList();
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (firstLine) {
            firstLine = false;
            if (line == "MAJO_OBJECT_IMAGE_SCALE_V1" || line == "MAJO_IMAGE_SCALE_V2") {
                continue;
            }
        }

        std::istringstream stream(line);
        std::string kind;
        std::string id;
        float scale = 1.0f;
        stream >> kind >> id >> scale;
        if (stream.fail() || id.empty()) {
            continue;
        }

        std::unordered_map<std::string, float>* target = nullptr;
        if (kind == "scale" || kind == "object") {
            target = &objectImageScaleById_;
        } else if (kind == "other") {
            target = &otherImageScaleByKey_;
        } else {
            continue;
        }

        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        (*target)[id] = snapped;
    }

    rebuildObjectImageScaleList();
    return true;
}

bool Game::saveObjectImageScaleData(std::string& message)
{
    std::error_code error;
    std::filesystem::create_directories("data", error);
    if (error) {
        message = "Image scale save failed: could not create data directory";
        return false;
    }

    const std::filesystem::path path = objectImageScaleDataPath();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "Image scale save failed: could not open " + path.string();
        return false;
    }

    std::vector<std::pair<std::string, float>> objectEntries;
    objectEntries.reserve(objectImageScaleById_.size());
    for (const auto& [objectId, scale] : objectImageScaleById_) {
        if (objectId.empty()) {
            continue;
        }
        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        objectEntries.push_back({objectId, snapped});
    }
    std::sort(objectEntries.begin(), objectEntries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<std::pair<std::string, float>> otherEntries;
    otherEntries.reserve(otherImageScaleByKey_.size());
    for (const auto& [key, scale] : otherImageScaleByKey_) {
        if (key.empty() || worldIconDefinitionByKey(key) == nullptr) {
            continue;
        }
        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        otherEntries.push_back({key, snapped});
    }
    std::sort(otherEntries.begin(), otherEntries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    file << "MAJO_IMAGE_SCALE_V2\n";
    for (const auto& [objectId, scale] : objectEntries) {
        file << "object " << objectId << " " << scale << "\n";
    }
    for (const auto& [key, scale] : otherEntries) {
        file << "other " << key << " " << scale << "\n";
    }

    if (!file) {
        message = "Image scale save failed while writing " + path.string();
        return false;
    }

    objectImageScaleDirty_ = false;
    message = "Image scale saved";
    return true;
}

void Game::rebuildObjectImageScaleList()
{
    objectImageScaleObjectIds_.clear();
    objectImageScaleObjectIds_.reserve(objectCatalog_.objects.size());
    std::unordered_set<std::string> seen;
    seen.reserve(objectCatalog_.objects.size());

    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (object.id.empty() || object.imageNumber <= 0) {
            continue;
        }
        if (!seen.insert(object.id).second) {
            continue;
        }
        objectImageScaleObjectIds_.push_back(object.id);
    }

    std::sort(objectImageScaleObjectIds_.begin(), objectImageScaleObjectIds_.end(), [this](const std::string& left, const std::string& right) {
        const ObjectDefinition* lhs = objectCatalog_.registry.findById(left);
        const ObjectDefinition* rhs = objectCatalog_.registry.findById(right);
        if (lhs == nullptr || rhs == nullptr) {
            return left < right;
        }
        if (lhs->imageNumber != rhs->imageNumber) {
            return lhs->imageNumber < rhs->imageNumber;
        }
        return left < right;
    });

    if (objectImageScaleObjectIds_.empty()) {
        objectImageScaleSelectedIndex_ = -1;
    } else if (objectImageScaleSelectedIndex_ < 0 || objectImageScaleSelectedIndex_ >= static_cast<int>(objectImageScaleObjectIds_.size())) {
        objectImageScaleSelectedIndex_ = 0;
    }

    otherImageScaleKeys_.clear();
    for (const WorldIconDefinition& definition : worldIconDefinitions()) {
        if (!definition.key.empty() && definition.imageNumber > 0) {
            otherImageScaleKeys_.push_back(std::string(definition.key));
        }
    }

    if (otherImageScaleKeys_.empty()) {
        otherImageScaleSelectedIndex_ = -1;
    } else if (otherImageScaleSelectedIndex_ < 0 || otherImageScaleSelectedIndex_ >= static_cast<int>(otherImageScaleKeys_.size())) {
        otherImageScaleSelectedIndex_ = 0;
    }
}

void Game::enterObjectImageScaleEditMode()
{
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    rebuildObjectImageScaleList();
    objectImageScaleReturnMode_ = mode_;
    if (objectImageScaleReturnMode_ == ScreenMode::ObjectImageScaleEdit) {
        objectImageScaleReturnMode_ = ScreenMode::Playing;
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    objectImageScaleScrollOffset_ = std::max(0.0f, objectImageScaleScrollOffset_);
    otherImageScaleScrollOffset_ = std::max(0.0f, otherImageScaleScrollOffset_);
    objectImageScaleStatus_ = objectImageScaleObjectIds_.empty() && otherImageScaleKeys_.empty()
        ? "No images available"
        : "画像サイズ編集";
    mode_ = ScreenMode::ObjectImageScaleEdit;
}

void Game::exitObjectImageScaleEditMode()
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit) {
        return;
    }

    mode_ = objectImageScaleReturnMode_;
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::updateObjectImageScaleEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit) {
        return;
    }

    if (input.backPressed()) {
        exitObjectImageScaleEditMode();
        return;
    }

    if (ui.pressed(objectImageScaleTabRect(0))) {
        imageScaleEditTab_ = ImageScaleEditTab::Objects;
        objectImageScaleStatus_ = "Objects";
    }
    if (ui.pressed(objectImageScaleTabRect(1))) {
        imageScaleEditTab_ = ImageScaleEditTab::Others;
        objectImageScaleStatus_ = "Others";
    }

    std::vector<std::string>& itemKeys = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleKeys_
        : objectImageScaleObjectIds_;
    std::unordered_map<std::string, float>& scaleMap = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleByKey_
        : objectImageScaleById_;
    int& selectedIndex = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleSelectedIndex_
        : objectImageScaleSelectedIndex_;
    float& scrollOffset = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleScrollOffset_
        : objectImageScaleScrollOffset_;
    const bool editingOthers = imageScaleEditTab_ == ImageScaleEditTab::Others;

    const ObjectImageScaleLayout layout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(itemKeys.size());
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    const float contentHeight = rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * ObjectImageScaleCardHeight + static_cast<float>(rows - 1) * ObjectImageScaleCardGap;
    const float maxScroll = std::max(0.0f, contentHeight - layout.viewport.size.y);
    scrollOffset = clamp(scrollOffset, 0.0f, maxScroll);

    if (input.saveShortcutPressed()) {
        std::string message;
        if (saveObjectImageScaleData(message)) {
            objectImageScaleStatus_ = message;
        } else {
            objectImageScaleStatus_ = message;
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
        if (ui.pressed(rect)) {
            selectedIndex = i;
            break;
        }
    }

    auto adjustSelectedScale = [this, &itemKeys, &scaleMap, &selectedIndex, editingOthers](int direction) {
        if (direction == 0 || selectedIndex < 0 || selectedIndex >= static_cast<int>(itemKeys.size())) {
            return;
        }

        const std::string& key = itemKeys[static_cast<std::size_t>(selectedIndex)];
        float currentScale = 1.0f;
        if (const auto it = scaleMap.find(key); it != scaleMap.end()) {
            currentScale = it->second;
        }
        const float nextScale = snapObjectImageScale(currentScale + ObjectImageScaleStep * static_cast<float>(direction));
        if (std::abs(nextScale - 1.0f) <= 0.0001f) {
            scaleMap.erase(key);
        } else {
            scaleMap[key] = nextScale;
        }
        objectImageScaleDirty_ = true;

        char status[160];
        std::snprintf(status, sizeof(status), "%s %s scale = %.2f", editingOthers ? "other" : "object", key.c_str(), nextScale);
        objectImageScaleStatus_ = status;
    };

    if (input.pressed(InputAction::MoveUp)) {
        adjustSelectedScale(+1);
    }
    if (input.pressed(InputAction::MoveDown)) {
        adjustSelectedScale(-1);
    }

    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        int hoveredIndex = -1;
        for (int i = 0; i < itemCount; ++i) {
            const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
            if (rect.contains(ui.mouse())) {
                hoveredIndex = i;
                break;
            }
        }

        if (hoveredIndex == selectedIndex && selectedIndex >= 0) {
            adjustSelectedScale(-wheel);
        } else {
            scrollOffset = clamp(
                scrollOffset + static_cast<float>(wheel) * 36.0f,
                0.0f,
                maxScroll);
        }
    }
}

void Game::renderObjectImageScaleEditScreen(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const bool editingOthers = imageScaleEditTab_ == ImageScaleEditTab::Others;
    const std::vector<std::string>& itemKeys = editingOthers ? otherImageScaleKeys_ : objectImageScaleObjectIds_;
    const std::unordered_map<std::string, float>& scaleMap = editingOthers ? otherImageScaleByKey_ : objectImageScaleById_;
    const int selectedIndex = editingOthers ? otherImageScaleSelectedIndex_ : objectImageScaleSelectedIndex_;
    const float activeScrollOffset = editingOthers ? otherImageScaleScrollOffset_ : objectImageScaleScrollOffset_;

    const ObjectImageScaleLayout layout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(itemKeys.size());
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    const float contentHeight = rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * ObjectImageScaleCardHeight + static_cast<float>(rows - 1) * ObjectImageScaleCardGap;
    const float maxScroll = std::max(0.0f, contentHeight - layout.viewport.size.y);
    const float scrollOffset = clamp(activeScrollOffset, 0.0f, maxScroll);

    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {10, 12, 18, 255});
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), ObjectImageScaleHeaderHeight}, {18, 24, 38, 255});
    renderer.fillRect({0.0f, static_cast<float>(camera_.height()) - ObjectImageScaleFooterHeight}, {static_cast<float>(camera_.width()), ObjectImageScaleFooterHeight}, {18, 24, 38, 255});
    renderer.drawText({22.0f, 18.0f}, "画像サイズ編集 (48px baseline)", {245, 245, 252, 255}, 3);

    for (int tab = 0; tab < 2; ++tab) {
        const bool active = (tab == 0 && !editingOthers) || (tab == 1 && editingOthers);
        const UiRect rect = objectImageScaleTabRect(tab);
        renderer.fillRect(rect.pos, rect.size, active ? Color{58, 76, 118, 255} : Color{26, 34, 50, 255});
        renderer.drawRect(rect.pos, rect.size, active ? Color{255, 228, 138, 255} : Color{92, 104, 126, 255});
        renderer.drawText(rect.pos + Vec2{14.0f, 8.0f}, tab == 0 ? "Objects" : "Others", active ? Color{255, 236, 166, 255} : Color{198, 206, 222, 255}, 2);
    }

    renderer.drawText({22.0f, 58.0f}, "Click to select  Wheel: scroll / selected card wheel: scale  Up/Down: scale  Ctrl+S: save  Esc: exit", {198, 206, 222, 255}, 2);
    renderer.drawRect(layout.viewport.pos, layout.viewport.size, {96, 108, 132, 255});

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.viewport.pos.y || rect.pos.y > layout.viewport.pos.y + layout.viewport.size.y) {
            continue;
        }

        const bool selected = i == selectedIndex;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{44, 58, 92, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{74, 86, 108, 255});

        const std::string& key = itemKeys[static_cast<std::size_t>(i)];
        float scale = 1.0f;
        if (const auto it = scaleMap.find(key); it != scaleMap.end()) {
            scale = it->second;
        }
        scale = snapObjectImageScale(scale);

        std::string name;
        std::string subtitle;
        bool drewImage = false;
        const Vec2 previewCenter = rect.pos + Vec2{rect.size.x * 0.5f, 38.0f};
        if (editingOthers) {
            const WorldIconDefinition* definition = worldIconDefinitionByKey(key);
            if (definition != nullptr) {
                WorldIconDrawOptions options;
                options.allowUpscale = true;
                drewImage = drawWorldIcon(renderer, definition->iconId, previewCenter, {ObjectImageScalePreviewSize, ObjectImageScalePreviewSize}, options);
                name = std::string(definition->displayName);
                subtitle = "img_" + std::to_string(definition->imageNumber) + " / " + key;
            } else {
                name = key;
                subtitle = "missing icon";
            }
        } else {
            const ObjectDefinition* object = objectCatalog_.registry.findById(key);
            if (object != nullptr) {
                ObjectImageDrawOptions options;
                options.allowUpscale = true;
                drewImage = drawObjectImage(
                    renderer,
                    *object,
                    previewCenter,
                    {ObjectImageScalePreviewSize, ObjectImageScalePreviewSize},
                    objectGroundImageOptions(*object, options));
                name = !object->name.empty() ? object->name : key;
                subtitle = key;
            } else {
                name = key;
                subtitle = "missing object";
            }
        }

        if (!drewImage) {
            renderer.fillCircle(previewCenter, 22.0f, {92, 102, 120, 255});
        }

        const std::string shownName = fittedSingleLineText(renderer, name, rect.size.x - 12.0f, 2);
        renderer.drawText(rect.pos + Vec2{6.0f, 68.0f}, shownName, {232, 236, 245, 255}, 2);
        const std::string shownSubtitle = fittedSingleLineText(renderer, subtitle, rect.size.x - 12.0f, 1);
        renderer.drawText(rect.pos + Vec2{6.0f, 92.0f}, shownSubtitle, {146, 158, 178, 255}, 1);

        char scaleText[64];
        std::snprintf(scaleText, sizeof(scaleText), "x%.2f", scale);
        renderer.drawText(rect.pos + Vec2{6.0f, 108.0f}, scaleText, selected ? Color{255, 232, 148, 255} : Color{186, 198, 216, 255}, 2);
    }

    const char* dirty = objectImageScaleDirty_ ? "Unsaved (*)" : "Saved";
    renderer.drawText({22.0f, static_cast<float>(camera_.height()) - 40.0f}, dirty, objectImageScaleDirty_ ? Color{255, 230, 150, 255} : Color{170, 220, 170, 255}, 2);
    if (!objectImageScaleStatus_.empty()) {
        renderer.drawText({220.0f, static_cast<float>(camera_.height()) - 40.0f}, objectImageScaleStatus_, {198, 206, 222, 255}, 2);
    }
}

bool Game::loadAudioCueManifestForEdit()
{
    std::string previousId;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        previousId = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].id;
    }

    std::vector<AudioCueManifestRow> rows;
    std::string message;
    if (!loadAudioCueManifestRows(rows, message)) {
        audioCueEditEntries_.clear();
        audioCueEditCueIndex_ = -1;
        audioCueEditDirty_ = false;
        audioCueEditStatus_ = message;
        return false;
    }

    audioCueEditEntries_.clear();
    for (const AudioCueManifestRow& row : rows) {
        if (row.valid && audioCueEditEntryMatchesMode(row.entry, audioCueEditMode_)) {
            audioCueEditEntries_.push_back(row.entry);
        }
    }

    audioCueEditCueIndex_ = -1;
    if (!previousId.empty()) {
        for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
            if (audioCueEditEntries_[static_cast<std::size_t>(i)].id == previousId) {
                audioCueEditCueIndex_ = i;
                break;
            }
        }
    }
    if (audioCueEditCueIndex_ < 0 && !audioCueEditEntries_.empty()) {
        audioCueEditCueIndex_ = 0;
    }

    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
        const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, cue.path);
        if (fileIndex >= 0) {
            audioCueEditFileIndex_ = fileIndex;
        }
    }

    audioCueEditDirty_ = false;
    audioCueEditStatus_ = audioCueEditEntries_.empty()
        ? audioCueEditTitle(audioCueEditMode_) + std::string(": cueなし")
        : audioCueEditTitle(audioCueEditMode_) + std::string(": manifest読込");
    return true;
}

bool Game::saveAudioCueManifestFromEdit(std::string& message)
{
    std::vector<AudioCueManifestRow> rows;
    if (!loadAudioCueManifestRows(rows, message)) {
        return false;
    }

    std::vector<bool> saved(audioCueEditEntries_.size(), false);
    for (AudioCueManifestRow& row : rows) {
        if (!row.valid || !audioCueEditEntryMatchesMode(row.entry, audioCueEditMode_)) {
            continue;
        }
        for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
            const AudioCueEditEntry& edited = audioCueEditEntries_[static_cast<std::size_t>(i)];
            if (edited.id == row.entry.id) {
                row.entry = edited;
                saved[static_cast<std::size_t>(i)] = true;
                break;
            }
        }
    }

    for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
        if (saved[static_cast<std::size_t>(i)]) {
            continue;
        }
        AudioCueManifestRow row;
        row.entry = audioCueEditEntries_[static_cast<std::size_t>(i)];
        row.valid = true;
        rows.push_back(std::move(row));
    }

    if (!writeAudioCueManifestRows(rows, message)) {
        return false;
    }

    audioCueEditDirty_ = false;
    if (audio_ != nullptr && !audio_->reloadManifest()) {
        message += " / reload failed: " + audio_->lastError();
        return false;
    }
    return true;
}

void Game::rebuildAudioCueFileList()
{
    std::string previousPath;
    if (audioCueEditFileIndex_ >= 0 && audioCueEditFileIndex_ < static_cast<int>(audioCueEditFiles_.size())) {
        previousPath = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)].relativePath;
    } else if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        previousPath = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path;
    }

    audioCueEditFiles_.clear();
    const std::filesystem::path folder = audioCueEditFolderText(audioCueEditMode_);
    const std::filesystem::path audioRoot = std::filesystem::path(std::string(AudioCueEditAudioRoot));
    std::error_code error;
    if (std::filesystem::exists(folder, error)) {
        std::filesystem::recursive_directory_iterator it(
            folder,
            std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && it != end) {
            const std::filesystem::directory_entry entry = *it;
            it.increment(error);
            if (error) {
                break;
            }
            std::error_code entryError;
            if (!entry.is_regular_file(entryError)) {
                continue;
            }
            const std::string extension = lowerAscii(entry.path().extension().string());
            if (extension != ".wav") {
                continue;
            }

            std::filesystem::path relative = std::filesystem::relative(entry.path(), audioRoot, entryError);
            if (entryError) {
                relative = entry.path().lexically_relative(audioRoot);
            }

            AudioCueFileEntry file;
            file.name = entry.path().filename().string();
            file.relativePath = normalizeAudioRelativePath(relative.generic_string());
            file.fileSize = entry.file_size(entryError);
            if (entryError) {
                file.fileSize = 0;
            }
            audioCueEditFiles_.push_back(std::move(file));
        }
    }

    std::sort(audioCueEditFiles_.begin(), audioCueEditFiles_.end(), [](const AudioCueFileEntry& lhs, const AudioCueFileEntry& rhs) {
        return lowerAscii(lhs.relativePath) < lowerAscii(rhs.relativePath);
    });

    audioCueEditFileIndex_ = audioCueEditFileIndexForPath(audioCueEditFiles_, previousPath);
    if (audioCueEditFileIndex_ < 0 && !audioCueEditFiles_.empty()) {
        audioCueEditFileIndex_ = 0;
    }
    if (audioCueEditFiles_.empty()) {
        audioCueEditFileIndex_ = -1;
    }

    audioCueEditStatus_ = std::to_string(audioCueEditFiles_.size()) + " files: " + audioCueEditFolderText(audioCueEditMode_);
}

void Game::enterAudioCueEditMode(AudioCueEditMode editMode)
{
    if (mode_ == ScreenMode::AudioCueEdit && audioCueEditMode_ == editMode) {
        rebuildAudioCueFileList();
        loadAudioCueManifestForEdit();
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    if (baseEditEnabled_) {
        exitBaseEditMode();
    }

    audioCueEditMode_ = editMode;
    audioCueEditReturnMode_ = mode_;
    if (audioCueEditReturnMode_ == ScreenMode::AudioCueEdit ||
        audioCueEditReturnMode_ == ScreenMode::ObjectImageScaleEdit) {
        audioCueEditReturnMode_ = ScreenMode::Playing;
    }
    audioCueEditPreviousBgmCue_ = activeAudioBgmCue_;

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    audioCueEditCueScrollOffset_ = 0.0f;
    audioCueEditFileScrollOffset_ = 0.0f;
    audioCueEditCueScrollState_ = {};
    audioCueEditFileScrollState_ = {};
    audioCueEditCancelState_ = {};

    rebuildAudioCueFileList();
    loadAudioCueManifestForEdit();
    mode_ = ScreenMode::AudioCueEdit;
}

void Game::exitAudioCueEditMode()
{
    if (mode_ != ScreenMode::AudioCueEdit) {
        return;
    }

    if (audio_ != nullptr) {
        if (!audioCueEditPreviousBgmCue_.empty()) {
            audio_->playBgm(audioCueEditPreviousBgmCue_, 0.12f, true);
            activeAudioBgmCue_ = audioCueEditPreviousBgmCue_;
        } else if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
            audio_->stopBgm(0.12f);
            activeAudioBgmCue_.clear();
        }
    }

    mode_ = audioCueEditReturnMode_;
    if (mode_ == ScreenMode::AudioCueEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::previewSelectedAudioCueFile()
{
    if (audio_ == nullptr) {
        audioCueEditStatus_ = "AudioEngine unavailable";
        return;
    }
    if (audioCueEditFileIndex_ < 0 || audioCueEditFileIndex_ >= static_cast<int>(audioCueEditFiles_.size())) {
        audioCueEditStatus_ = "ファイルを選択してください";
        return;
    }

    AudioCueOptions options;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
        options.volume = cue.volume;
        options.loop = cue.loop;
        options.cooldownSeconds = cue.cooldownMs / 1000.0f;
    } else {
        options.loop = audioCueEditMode_ == AudioCueEditMode::Bgm;
    }
    if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
        options.loop = true;
    }

    const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    const std::filesystem::path clipPath = std::filesystem::path(std::string(AudioCueEditAudioRoot)) / std::filesystem::path(file.relativePath);
    const std::string previewId = audioCueEditMode_ == AudioCueEditMode::Bgm ? "__preview.bgm" : "__preview.se";
    if (!audio_->loadCue(previewId, audioCueEngineType(audioCueEditMode_), clipPath, options)) {
        audioCueEditStatus_ = "試聴失敗: " + audio_->lastError();
        return;
    }

    if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
        audio_->playBgm(previewId, 0.08f, true);
    } else {
        audio_->playSe(previewId);
    }
    audioCueEditStatus_ = "試聴: " + file.relativePath;
}

void Game::applySelectedAudioCueFile()
{
    if (audioCueEditCueIndex_ < 0 || audioCueEditCueIndex_ >= static_cast<int>(audioCueEditEntries_.size())) {
        audioCueEditStatus_ = "cueを選択してください";
        return;
    }
    if (audioCueEditFileIndex_ < 0 || audioCueEditFileIndex_ >= static_cast<int>(audioCueEditFiles_.size())) {
        audioCueEditStatus_ = "ファイルを選択してください";
        return;
    }

    AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
    const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    cue.path = file.relativePath;
    audioCueEditDirty_ = true;

    AudioCueOptions options;
    options.volume = cue.volume;
    options.loop = cue.loop;
    options.cooldownSeconds = cue.cooldownMs / 1000.0f;
    const std::filesystem::path clipPath = std::filesystem::path(std::string(AudioCueEditAudioRoot)) / std::filesystem::path(file.relativePath);
    if (audio_ != nullptr) {
        if (audio_->loadCue(cue.id, audioCueEngineType(audioCueEditMode_), clipPath, options)) {
            if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
                audio_->playBgm(cue.id, 0.08f, true);
            } else {
                audio_->playSe(cue.id);
            }
            audioCueEditStatus_ = audioCueDisplayName(cue) + " => " + cue.path;
        } else {
            audioCueEditStatus_ = "適用したが試聴失敗: " + audio_->lastError();
        }
    } else {
        audioCueEditStatus_ = audioCueDisplayName(cue) + " => " + cue.path;
    }
}

void Game::updateAudioCueEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::AudioCueEdit) {
        return;
    }

    const AudioCueEditLayout layout = makeAudioCueEditLayout(camera_.width(), camera_.height());
    const int cueCount = static_cast<int>(audioCueEditEntries_.size());
    const int fileCount = static_cast<int>(audioCueEditFiles_.size());
    const UiScrollableListStyle listStyle = audioCueEditListStyle();
    const UiRect cueViewport = audioCueEditListViewport(layout.cueList);
    const UiRect fileViewport = audioCueEditListViewport(layout.fileList);
    UiScrollAreaLayout cueList = updateUiScrollableList(
        ui,
        input,
        cueViewport,
        cueCount,
        audioCueEditCueScrollOffset_,
        listStyle,
        &audioCueEditCueScrollState_);
    UiScrollAreaLayout fileList = updateUiScrollableList(
        ui,
        input,
        fileViewport,
        fileCount,
        audioCueEditFileScrollOffset_,
        listStyle,
        &audioCueEditFileScrollState_);
    bool keepCueSelectionVisible = false;
    bool keepFileSelectionVisible = false;

    if (uiCancelRequested(audioCueEditCancelState_, input, ui, layout.panel) ||
        ui.pressed(audioCueEditCloseButtonRect(layout.panel))) {
        exitAudioCueEditMode();
        return;
    }

    if (input.saveShortcutPressed() || ui.pressed(audioCueEditSaveButtonRect(layout.panel))) {
        std::string message;
        if (saveAudioCueManifestFromEdit(message)) {
            audioCueEditStatus_ = message;
        } else {
            audioCueEditStatus_ = message;
        }
    }

    if (ui.pressed(audioCueEditRescanButtonRect(layout.panel))) {
        rebuildAudioCueFileList();
        if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < cueCount) {
            const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path);
            if (fileIndex >= 0) {
                audioCueEditFileIndex_ = fileIndex;
                keepFileSelectionVisible = true;
            }
        }
    }

    if (ui.pressed(audioCueEditPreviewButtonRect(layout.panel))) {
        previewSelectedAudioCueFile();
    }
    if (ui.pressed(audioCueEditApplyButtonRect(layout.panel)) || input.confirmPressed()) {
        applySelectedAudioCueFile();
    }
    if (input.useItemPressed()) {
        previewSelectedAudioCueFile();
    }

    for (int i = 0; i < cueCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(cueList, i, listStyle);
        if (!uiScrollAreaRectVisible(cueList, rect)) {
            continue;
        }
        if (cueViewport.contains(ui.mouse()) && ui.pressed(rect)) {
            audioCueEditCueIndex_ = i;
            const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(i)].path);
            if (fileIndex >= 0) {
                audioCueEditFileIndex_ = fileIndex;
                keepFileSelectionVisible = true;
            }
            audioCueEditStatus_ = audioCueDisplayName(audioCueEditEntries_[static_cast<std::size_t>(i)]);
            break;
        }
    }

    for (int i = 0; i < fileCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(fileList, i, listStyle);
        if (!uiScrollAreaRectVisible(fileList, rect)) {
            continue;
        }
        if (fileViewport.contains(ui.mouse()) && ui.pressed(rect)) {
            audioCueEditFileIndex_ = i;
            audioCueEditStatus_ = audioCueEditFiles_[static_cast<std::size_t>(i)].relativePath;
            break;
        }
    }

    const int cueDelta =
        (input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    if (cueDelta != 0 && cueCount > 0) {
        audioCueEditCueIndex_ = std::clamp(audioCueEditCueIndex_ + cueDelta, 0, cueCount - 1);
        keepCueSelectionVisible = true;
        const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path);
        if (fileIndex >= 0) {
            audioCueEditFileIndex_ = fileIndex;
            keepFileSelectionVisible = true;
        }
    }

    const int fileDelta =
        (input.pressed(InputAction::MoveDown) ? 1 : 0) -
        (input.pressed(InputAction::MoveUp) ? 1 : 0);
    if (fileDelta != 0 && fileCount > 0) {
        audioCueEditFileIndex_ = std::clamp(audioCueEditFileIndex_ + fileDelta, 0, fileCount - 1);
        keepFileSelectionVisible = true;
    }

    if (keepCueSelectionVisible) {
        keepAudioCueEditSelectionVisible(layout.cueList, audioCueEditCueIndex_, cueCount, audioCueEditCueScrollOffset_);
    }
    if (keepFileSelectionVisible) {
        keepAudioCueEditSelectionVisible(layout.fileList, audioCueEditFileIndex_, fileCount, audioCueEditFileScrollOffset_);
    }
}

void Game::renderAudioCueEditScreen(Renderer& renderer) const
{
    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {8, 10, 16, 255});

    const AudioCueEditLayout layout = makeAudioCueEditLayout(camera_.width(), camera_.height());
    const std::string title = audioCueEditTitle(audioCueEditMode_);
    UiWindowScope window(
        renderer,
        audioCueEditMode_ == AudioCueEditMode::Bgm ? "audio.cue_edit.bgm" : "audio.cue_edit.se",
        layout.panel,
        title,
        "クリック選択 / F適用 / Space試聴 / Ctrl+S保存 / Esc戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.cueList);
    drawUiSubPanel(renderer, layout.fileList);
    drawUiSubPanel(renderer, layout.detail);

    renderer.drawText(layout.cueList.pos + Vec2{12.0f, 10.0f}, "cue ID", ui::TextMuted, 2);
    renderer.drawText(layout.fileList.pos + Vec2{12.0f, 10.0f}, audioCueEditFolderText(audioCueEditMode_), ui::TextMuted, 2);

    const int cueCount = static_cast<int>(audioCueEditEntries_.size());
    const int fileCount = static_cast<int>(audioCueEditFiles_.size());
    const UiScrollableListStyle listStyle = audioCueEditListStyle();
    const UiScrollAreaLayout cueList = makeUiScrollableListLayout(
        audioCueEditListViewport(layout.cueList),
        cueCount,
        audioCueEditCueScrollOffset_,
        listStyle);
    const UiScrollAreaLayout fileList = makeUiScrollableListLayout(
        audioCueEditListViewport(layout.fileList),
        fileCount,
        audioCueEditFileScrollOffset_,
        listStyle);
    renderer.pushClipRect(cueList.viewport.pos, cueList.viewport.size);
    for (int i = 0; i < cueCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(cueList, i, listStyle);
        if (!uiScrollAreaRectVisible(cueList, rect)) {
            continue;
        }

        const bool selected = i == audioCueEditCueIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 70, 108, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{76, 88, 112, 255});

        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(i)];
        renderer.drawText(rect.pos + Vec2{10.0f, 6.0f}, fittedSingleLineText(renderer, audioCueDisplayName(cue), rect.size.x - 20.0f, 2), selected ? Color{255, 236, 166, 255} : ui::Text, 2);
        renderer.drawText(rect.pos + Vec2{10.0f, 28.0f}, fittedSingleLineText(renderer, cue.path, rect.size.x - 20.0f, 1), ui::TextMuted, 1);
    }
    renderer.popClipRect();
    if (cueCount == 0) {
        renderer.drawText(layout.cueList.pos + Vec2{18.0f, 54.0f}, "cueがありません", ui::TextDisabled, 2);
    }

    renderer.pushClipRect(fileList.viewport.pos, fileList.viewport.size);
    for (int i = 0; i < fileCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(fileList, i, listStyle);
        if (!uiScrollAreaRectVisible(fileList, rect)) {
            continue;
        }

        const bool selected = i == audioCueEditFileIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{52, 76, 70, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{170, 240, 190, 255} : Color{76, 88, 112, 255});

        const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(i)];
        renderer.drawText(rect.pos + Vec2{10.0f, 6.0f}, fittedSingleLineText(renderer, file.name, rect.size.x - 20.0f, 2), selected ? Color{202, 255, 216, 255} : ui::Text, 2);
        renderer.drawText(rect.pos + Vec2{10.0f, 28.0f}, fittedSingleLineText(renderer, file.relativePath, rect.size.x - 20.0f, 1), ui::TextMuted, 1);
    }
    renderer.popClipRect();
    if (fileCount == 0) {
        renderer.drawText(layout.fileList.pos + Vec2{18.0f, 54.0f}, "WAVがありません", ui::TextDisabled, 2);
    }
    drawUiScrollAreaFrame(renderer, cueList, listStyle.scroll);
    drawUiScrollAreaFrame(renderer, fileList, listStyle.scroll);

    const AudioCueEditEntry* cue = nullptr;
    const AudioCueFileEntry* file = nullptr;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < cueCount) {
        cue = &audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
    }
    if (audioCueEditFileIndex_ >= 0 && audioCueEditFileIndex_ < fileCount) {
        file = &audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    }

    float y = drawUiDetailHeader(renderer, layout.detail, "選択内容");
    drawUiDetailLine(renderer, layout.detail, y, "種類", audioCueEditTypeText(audioCueEditMode_));
    drawUiDetailLine(renderer, layout.detail, y, "表示名", cue != nullptr ? audioCueDisplayName(*cue) : "未選択");
    drawUiDetailLine(renderer, layout.detail, y, "内部ID", cue != nullptr ? cue->id : "-");
    drawUiDetailLine(renderer, layout.detail, y, "現在", cue != nullptr ? cue->path : "-");
    drawUiDetailLine(renderer, layout.detail, y, "候補", file != nullptr ? file->relativePath : "-");
    if (cue != nullptr) {
        drawUiDetailLine(renderer, layout.detail, y, "volume", formatAudioFloat(cue->volume));
        drawUiDetailLine(renderer, layout.detail, y, "loop", cue->loop ? "true" : "false");
        drawUiDetailLine(renderer, layout.detail, y, "cooldown", formatAudioFloat(cue->cooldownMs) + " ms");
    }
    if (file != nullptr) {
        const float kib = static_cast<float>(file->fileSize) / 1024.0f;
        drawUiDetailLine(renderer, layout.detail, y, "size", formatAudioFloat(kib) + " KiB");
    }

    const bool canPreview = file != nullptr;
    const bool canApply = cue != nullptr && file != nullptr;
    drawUiButton(renderer, audioCueEditCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, audioCueEditRescanButtonRect(layout.panel), "再スキャン", false, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditPreviewButtonRect(layout.panel), "試聴", canPreview, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditApplyButtonRect(layout.panel), "適用", canApply, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditSaveButtonRect(layout.panel), "保存", audioCueEditDirty_, uiActionButtonStyle());

    const UiRect rescan = audioCueEditRescanButtonRect(layout.panel);
    const UiRect preview = audioCueEditPreviewButtonRect(layout.panel);
    const float statusX = rescan.pos.x + rescan.size.x + 16.0f;
    const float statusW = std::max(0.0f, preview.pos.x - statusX - 16.0f);
    const std::string dirty = audioCueEditDirty_ ? "未保存" : "保存済み";
    const std::string status = dirty + (audioCueEditStatus_.empty() ? std::string{} : " / " + audioCueEditStatus_);
    renderer.drawText(
        {statusX, rescan.pos.y + 9.0f},
        fittedSingleLineText(renderer, status, statusW, 2),
        audioCueEditDirty_ ? Color{255, 230, 150, 255} : ui::TextMuted,
        2);
}

void Game::rebuildDebugItemPickerList()
{
    std::string previousSelection;
    if (debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < static_cast<int>(debugItemPickerObjectIds_.size())) {
        previousSelection = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    }

    debugItemPickerObjectIds_.clear();
    debugItemPickerObjectIds_.reserve(objectCatalog_.registry.size());
    for (const ItemData& item : objectCatalog_.registry.items()) {
        if (!item.id.empty()) {
            debugItemPickerObjectIds_.push_back(item.id);
        }
    }

    std::sort(debugItemPickerObjectIds_.begin(), debugItemPickerObjectIds_.end(), [this](const std::string& left, const std::string& right) {
        const ItemData* lhs = objectCatalog_.registry.findById(left);
        const ItemData* rhs = objectCatalog_.registry.findById(right);
        if (lhs == nullptr || rhs == nullptr) {
            return left < right;
        }
        if (lhs->category != rhs->category) {
            return lhs->category < rhs->category;
        }
        if (lhs->rarity != rhs->rarity) {
            return lhs->rarity < rhs->rarity;
        }
        const std::string lhsName = debugItemPickerDisplayName(*lhs);
        const std::string rhsName = debugItemPickerDisplayName(*rhs);
        if (lhsName != rhsName) {
            return lhsName < rhsName;
        }
        return left < right;
    });

    debugItemPickerSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find(debugItemPickerObjectIds_.begin(), debugItemPickerObjectIds_.end(), previousSelection);
        if (it != debugItemPickerObjectIds_.end()) {
            debugItemPickerSelectedIndex_ = static_cast<int>(std::distance(debugItemPickerObjectIds_.begin(), it));
        }
    }
    if (debugItemPickerSelectedIndex_ < 0 && !debugItemPickerObjectIds_.empty()) {
        debugItemPickerSelectedIndex_ = 0;
    }
}

void Game::openDebugItemPicker()
{
    if (mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title || mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: item picker requires an active game screen.");
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    rebuildDebugItemPickerList();
    inventory_.cancelGrab();
    cancelRingGrab();
    debugItemPickerScrollOffset_ = std::max(0.0f, debugItemPickerScrollOffset_);
    debugItemPickerStatus_ = debugItemPickerObjectIds_.empty()
        ? "Objects DBにアイテムがありません"
        : "アイテムを選んで追加できます";
    debugItemPickerCancelState_ = {};
    debugItemPickerActive_ = true;
}

void Game::closeDebugItemPicker()
{
    debugItemPickerActive_ = false;
    debugItemPickerCancelState_ = {};
}

void Game::addSelectedDebugItem()
{
    if (debugItemPickerSelectedIndex_ < 0 ||
        debugItemPickerSelectedIndex_ >= static_cast<int>(debugItemPickerObjectIds_.size())) {
        debugItemPickerStatus_ = "追加するアイテムが選択されていません";
        return;
    }

    const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    const ItemData* item = objectCatalog_.registry.findById(objectId);
    if (item == nullptr) {
        debugItemPickerStatus_ = "Objects DBに見つかりません: " + objectId;
        return;
    }

    const std::string itemName = debugItemPickerDisplayName(*item);
    InventoryAddResult addResult;
    if (!inventory_.addObjectItem(objectCatalog_, objectId, &addResult)) {
        debugItemPickerStatus_ = "追加できません: " + itemName;
        reloadNotice_ = debugItemPickerStatus_;
        reloadNoticeTimer_ = 1.4f;
        logWarning("Debug: failed to add object_id=\"" + objectId + "\".");
        return;
    }

    debugItemPickerStatus_ = "追加: " + itemName;
    reloadNotice_ = "Debug add: " + inlineItemTag(objectId) + " " + itemName;
    reloadNoticeTimer_ = 1.6f;
    recordObjectObtainedForFirstNotice(
        objectId,
        addResult.instanceId,
        addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
        player_.position);
    syncEncyclopediaFromInventoryAndRing();
    logInfo("Debug: added object_id=\"" + objectId + "\".");
}

void Game::updateDebugItemPicker(const Input& input, UiContext& ui)
{
    if (!debugItemPickerActive_) {
        return;
    }

    const DebugItemPickerLayout layout = makeDebugItemPickerLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugItemPickerObjectIds_.size());
    if (itemCount > 0) {
        debugItemPickerSelectedIndex_ = std::clamp(debugItemPickerSelectedIndex_, 0, itemCount - 1);
    } else {
        debugItemPickerSelectedIndex_ = -1;
    }

    const float maxScroll = debugItemPickerMaxScroll(layout, itemCount);
    debugItemPickerScrollOffset_ = clamp(debugItemPickerScrollOffset_, 0.0f, maxScroll);

    if (uiCancelRequested(debugItemPickerCancelState_, input, ui, layout.panel) ||
        ui.pressed(debugItemPickerCloseButtonRect(layout.panel))) {
        closeDebugItemPicker();
        return;
    }

    if (itemCount > 0) {
        const int moveX =
            (input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveLeft) ? 1 : 0);
        const int moveY =
            (input.pressed(InputAction::MoveDown) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) ? 1 : 0);
        const int moveDelta = moveX + moveY * std::max(1, layout.columns);
        if (moveDelta != 0) {
            debugItemPickerSelectedIndex_ = std::clamp(debugItemPickerSelectedIndex_ + moveDelta, 0, itemCount - 1);
            keepDebugItemPickerSelectionVisible(layout, debugItemPickerSelectedIndex_, itemCount, debugItemPickerScrollOffset_);
        }
    }

    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        debugItemPickerScrollOffset_ = clamp(
            debugItemPickerScrollOffset_ + static_cast<float>(wheel) * 48.0f,
            0.0f,
            maxScroll);
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugItemPickerCardRect(layout, i, debugItemPickerScrollOffset_);
        if (rect.pos.y + rect.size.y < layout.grid.pos.y ||
            rect.pos.y > layout.grid.pos.y + layout.grid.size.y) {
            continue;
        }
        if (ui.pressed(rect)) {
            debugItemPickerSelectedIndex_ = i;
            keepDebugItemPickerSelectionVisible(layout, debugItemPickerSelectedIndex_, itemCount, debugItemPickerScrollOffset_);
            break;
        }
    }

    if (ui.pressed(debugItemPickerAddButtonRect(layout.panel)) || input.confirmPressed() || input.useItemPressed()) {
        addSelectedDebugItem();
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::renderDebugItemPicker(Renderer& renderer) const
{
    if (!debugItemPickerActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugItemPickerLayout layout = makeDebugItemPickerLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugItemPickerObjectIds_.size());
    const float scrollOffset = clamp(
        debugItemPickerScrollOffset_,
        0.0f,
        debugItemPickerMaxScroll(layout, itemCount));

    drawUiModalBackdrop(
        renderer,
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        {0, 0, 0, 142});

    UiCancelControlScope cancelScope(debugItemPickerCancelState_);
    UiWindowScope pickerWindow(
        renderer,
        "debug.item_picker",
        layout.panel,
        "デバッグ: 任意アイテム追加",
        "クリック/方向キー 選択  F/Enter 追加  Esc/右クリック 戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.grid);
    drawUiSubPanel(renderer, layout.detail);

    if (itemCount <= 0) {
        renderer.drawText(layout.grid.pos + Vec2{22.0f, 24.0f}, "Objects DBにアイテムがありません", ui::TextMuted, 2);
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugItemPickerCardRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.grid.pos.y ||
            rect.pos.y > layout.grid.pos.y + layout.grid.size.y) {
            continue;
        }

        const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(i)];
        const ItemData* item = objectCatalog_.registry.findById(objectId);
        const bool selected = i == debugItemPickerSelectedIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 44, 72, 232} : Color{18, 20, 30, 218});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 230, 150, 255} : Color{92, 100, 126, 210});

        if (item == nullptr) {
            renderer.drawText(rect.pos + Vec2{8.0f, 36.0f}, "missing", ui::TextDisabled, 2);
            continue;
        }

        InventoryUiEntryView entry{};
        entry.item = item;
        entry.stackCount = 1;
        InventoryUiSlotStyle style;
        style.selected = selected;
        style.imageMaxSize = 46.0f;
        style.showProtectionLabel = false;
        const UiRect iconRect{{
            rect.pos.x + (rect.size.x - DebugItemPickerIconSize) * 0.5f,
            rect.pos.y + 7.0f,
        }, {DebugItemPickerIconSize, DebugItemPickerIconSize}};
        drawInventoryUiSlot(renderer, iconRect, entry, style);

        const std::string name = fittedSingleLineText(renderer, debugItemPickerDisplayName(*item), rect.size.x - 12.0f, 2);
        renderer.drawText(rect.pos + Vec2{6.0f, 68.0f}, name, ui::Text, 2);
        const std::string subtitle = fittedSingleLineText(renderer, debugItemPickerSubtitle(*item), rect.size.x - 12.0f, 1);
        renderer.drawText(rect.pos + Vec2{6.0f, 91.0f}, subtitle, ui::TextMuted, 1);
    }

    InventoryUiEntryView detailEntry{};
    std::vector<InventoryUiDetailExtraLine> detailLines;
    if (debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < itemCount) {
        const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
        if (const ItemData* item = objectCatalog_.registry.findById(objectId)) {
            detailEntry.item = item;
            detailEntry.stackCount = 1;
            detailLines.push_back({"ID", item->id, ui::TextMuted});
            if (!item->category.empty()) {
                detailLines.push_back({"カテゴリ", item->category, ui::Text});
            }
        }
    }
    drawInventoryUiDetailPanel(renderer, layout.detail, detailEntry, objectCatalog_, encyclopedia_, false, detailLines);

    drawUiButton(renderer, debugItemPickerCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, debugItemPickerAddButtonRect(layout.panel), "追加", detailEntry.item != nullptr, uiActionButtonStyle());
    if (!debugItemPickerStatus_.empty()) {
        const UiRect closeRect = debugItemPickerCloseButtonRect(layout.panel);
        const UiRect addRect = debugItemPickerAddButtonRect(layout.panel);
        const float statusX = closeRect.pos.x + closeRect.size.x + 22.0f;
        const float statusW = std::max(40.0f, addRect.pos.x - statusX - 18.0f);
        renderer.drawText(
            {statusX, closeRect.pos.y + 17.0f},
            fittedSingleLineText(renderer, debugItemPickerStatus_, statusW, 2),
            {255, 230, 150, 255},
            2);
    }
}

void Game::rebuildDebugStoryTestList()
{
    std::string previousSelection;
    if (debugStoryTestSelectedIndex_ >= 0 &&
        debugStoryTestSelectedIndex_ < static_cast<int>(debugStoryTestEntries_.size())) {
        previousSelection = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)].eventId;
    }

    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;
    debugStoryTestEntries_.clear();
    debugStoryTestEntries_.reserve(storyEvents_.size());
    for (const StoryEvent& event : storyEvents_) {
        if (event.id.empty()) {
            continue;
        }
        if (debugStoryTestIsTutorialTrigger(event.trigger) != tutorials) {
            continue;
        }

        DebugStoryTestEntry entry;
        entry.eventId = event.id;
        entry.title = debugStoryTestDisplayTitle(event);
        entry.trigger = debugStoryTestDisplayTrigger(event);
        entry.onceFlag = debugStoryTestDisplayOnceFlag(event);
        entry.repeatable = event.repeatable;
        entry.alreadySeen = !event.onceFlag.empty() &&
            std::find(storyFlags_.begin(), storyFlags_.end(), event.onceFlag) != storyFlags_.end();
        debugStoryTestEntries_.push_back(std::move(entry));
    }

    debugStoryTestSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find_if(
            debugStoryTestEntries_.begin(),
            debugStoryTestEntries_.end(),
            [&](const DebugStoryTestEntry& entry) { return entry.eventId == previousSelection; });
        if (it != debugStoryTestEntries_.end()) {
            debugStoryTestSelectedIndex_ = static_cast<int>(std::distance(debugStoryTestEntries_.begin(), it));
        }
    }
    if (debugStoryTestSelectedIndex_ < 0 && !debugStoryTestEntries_.empty()) {
        debugStoryTestSelectedIndex_ = 0;
    }
    debugStoryTestLoadedRevision_ = storyEventsRevision_;
}

void Game::openDebugStoryTest(DebugStoryTestMode mode)
{
    if (mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title || mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: story test requires an active game screen.");
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugItemPicker();
    debugStoryTestReturnAfterDialogue_ = false;
    debugStoryTestMode_ = mode;
    rebuildDebugStoryTestList();
    inventory_.cancelGrab();
    cancelRingGrab();
    debugStoryTestScrollOffset_ = std::max(0.0f, debugStoryTestScrollOffset_);
    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;
    debugStoryTestStatus_ = debugStoryTestEntries_.empty()
        ? (tutorials ? "チュートリアルがありません" : "イベントがありません")
        : (tutorials ? "チュートリアルを選んで再生できます" : "イベントを選んで再生できます");
    debugStoryTestCancelState_ = {};
    debugStoryTestActive_ = true;
}

void Game::closeDebugStoryTest()
{
    debugStoryTestActive_ = false;
    debugStoryTestReturnAfterDialogue_ = false;
    debugStoryTestCancelState_ = {};
}

void Game::playSelectedDebugStoryTest()
{
    if (debugStoryTestSelectedIndex_ < 0 ||
        debugStoryTestSelectedIndex_ >= static_cast<int>(debugStoryTestEntries_.size())) {
        debugStoryTestStatus_ = "再生する会話が選択されていません";
        return;
    }

    const DebugStoryTestEntry entry = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)];
    if (!startStoryEventForDebug(entry.eventId)) {
        debugStoryTestStatus_ = "再生できません: " + entry.eventId;
        reloadNotice_ = debugStoryTestStatus_;
        reloadNoticeTimer_ = 1.6f;
        return;
    }

    reloadNotice_ = "Story test: " + entry.title;
    reloadNoticeTimer_ = 1.6f;
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = true;
}

void Game::updateDebugStoryTest(const Input& input, UiContext& ui)
{
    if (!debugStoryTestActive_) {
        return;
    }

    if (debugStoryTestLoadedRevision_ != storyEventsRevision_) {
        rebuildDebugStoryTestList();
    }

    const DebugStoryTestLayout layout = makeDebugStoryTestLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugStoryTestEntries_.size());
    if (itemCount > 0) {
        debugStoryTestSelectedIndex_ = std::clamp(debugStoryTestSelectedIndex_, 0, itemCount - 1);
    } else {
        debugStoryTestSelectedIndex_ = -1;
    }

    const float maxScroll = debugStoryTestMaxScroll(layout, itemCount);
    debugStoryTestScrollOffset_ = clamp(debugStoryTestScrollOffset_, 0.0f, maxScroll);

    if (uiCancelRequested(debugStoryTestCancelState_, input, ui, layout.panel) ||
        ui.pressed(debugStoryTestCloseButtonRect(layout.panel))) {
        closeDebugStoryTest();
        return;
    }

    if (itemCount > 0) {
        const int moveDelta =
            (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) ? 1 : 0);
        if (moveDelta != 0) {
            debugStoryTestSelectedIndex_ = std::clamp(debugStoryTestSelectedIndex_ + moveDelta, 0, itemCount - 1);
            keepDebugStoryTestSelectionVisible(layout, debugStoryTestSelectedIndex_, itemCount, debugStoryTestScrollOffset_);
        }
    }

    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        debugStoryTestScrollOffset_ = clamp(
            debugStoryTestScrollOffset_ + static_cast<float>(wheel) * 48.0f,
            0.0f,
            maxScroll);
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugStoryTestRowRect(layout, i, debugStoryTestScrollOffset_);
        if (rect.pos.y + rect.size.y < layout.list.pos.y ||
            rect.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }
        if (ui.pressed(rect)) {
            debugStoryTestSelectedIndex_ = i;
            keepDebugStoryTestSelectionVisible(layout, debugStoryTestSelectedIndex_, itemCount, debugStoryTestScrollOffset_);
            break;
        }
    }

    if (ui.pressed(debugStoryTestPlayButtonRect(layout.panel)) || input.confirmPressed() || input.useItemPressed()) {
        playSelectedDebugStoryTest();
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::renderDebugStoryTest(Renderer& renderer) const
{
    if (!debugStoryTestActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugStoryTestLayout layout = makeDebugStoryTestLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugStoryTestEntries_.size());
    const float scrollOffset = clamp(
        debugStoryTestScrollOffset_,
        0.0f,
        debugStoryTestMaxScroll(layout, itemCount));
    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;

    drawUiModalBackdrop(
        renderer,
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        {0, 0, 0, 142});

    UiCancelControlScope cancelScope(debugStoryTestCancelState_);
    UiWindowScope pickerWindow(
        renderer,
        tutorials ? "debug.story_test.tutorials" : "debug.story_test.events",
        layout.panel,
        tutorials ? "デバッグ: チュートリアルテスト" : "デバッグ: イベントテスト",
        "クリック/方向キー 選択  F/Enter 再生  Esc/右クリック 戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.list);
    drawUiSubPanel(renderer, layout.detail);

    if (itemCount <= 0) {
        renderer.drawText(
            layout.list.pos + Vec2{22.0f, 24.0f},
            tutorials ? "チュートリアルがありません" : "イベントがありません",
            ui::TextMuted,
            2);
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugStoryTestRowRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.list.pos.y ||
            rect.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }

        const DebugStoryTestEntry& entry = debugStoryTestEntries_[static_cast<std::size_t>(i)];
        const bool selected = i == debugStoryTestSelectedIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 44, 72, 232} : Color{18, 20, 30, 218});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 230, 150, 255} : Color{92, 100, 126, 210});

        const char* stateText = entry.repeatable ? "REPEAT" : (entry.alreadySeen ? "SEEN" : "ONCE");
        const Color stateColor = entry.repeatable
            ? Color{146, 224, 176, 255}
            : (entry.alreadySeen ? Color{168, 186, 214, 255} : Color{255, 230, 150, 255});
        const Vec2 stateSize = renderer.measureText(stateText, 2);
        const float stateX = rect.pos.x + rect.size.x - stateSize.x - 16.0f;
        renderer.drawText({stateX, rect.pos.y + 16.0f}, stateText, stateColor, 2);

        const float titleMaxWidth = std::max(120.0f, stateX - rect.pos.x - 28.0f);
        const std::string title = fittedSingleLineText(renderer, entry.title, titleMaxWidth, 2);
        renderer.drawText(rect.pos + Vec2{14.0f, 7.0f}, title, ui::Text, 2);

        const std::string meta = entry.eventId + " / " + entry.trigger;
        const std::string shownMeta = fittedSingleLineText(renderer, meta, rect.size.x - 28.0f, 1);
        renderer.drawText(rect.pos + Vec2{14.0f, 34.0f}, shownMeta, ui::TextMuted, 1);
    }

    bool canPlay = false;
    if (debugStoryTestSelectedIndex_ >= 0 &&
        debugStoryTestSelectedIndex_ < itemCount) {
        canPlay = true;
        const DebugStoryTestEntry& entry = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)];
        float y = drawUiDetailHeader(renderer, layout.detail, entry.title);
        drawUiDetailLine(renderer, layout.detail, y, "ID", entry.eventId, ui::TextMuted);
        drawUiDetailLine(renderer, layout.detail, y, "Trigger", entry.trigger, ui::Text);
        drawUiDetailLine(renderer, layout.detail, y, "Once", entry.onceFlag, entry.repeatable ? Color{146, 224, 176, 255} : ui::Text);
        drawUiDetailLine(
            renderer,
            layout.detail,
            y,
            "状態",
            entry.repeatable ? "繰り返し" : (entry.alreadySeen ? "再生済み" : "未再生"),
            entry.alreadySeen ? ui::TextMuted : ui::Text);
        drawUiDetailText(renderer, layout.detail, y, "テスト再生では @once フラグを追加しません。");
    } else {
        renderer.drawText(
            uiSubPanelContentPos(layout.detail),
            tutorials ? "チュートリアルを選択してください" : "イベントを選択してください",
            ui::TextMuted,
            2);
    }

    drawUiButton(renderer, debugStoryTestCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, debugStoryTestPlayButtonRect(layout.panel), "再生", canPlay, uiActionButtonStyle());
    if (!debugStoryTestStatus_.empty()) {
        const UiRect closeRect = debugStoryTestCloseButtonRect(layout.panel);
        const UiRect addRect = debugStoryTestPlayButtonRect(layout.panel);
        const float statusX = closeRect.pos.x + closeRect.size.x + 22.0f;
        const float statusW = std::max(40.0f, addRect.pos.x - statusX - 18.0f);
        renderer.drawText(
            {statusX, closeRect.pos.y + 17.0f},
            fittedSingleLineText(renderer, debugStoryTestStatus_, statusW, 2),
            {255, 230, 150, 255},
            2);
    }
}

void Game::enterEnemyTestMode()
{
    if (!enemyTestActive_ && mode_ == ScreenMode::Playing) {
        captureDungeonState();
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }

    enemyTestActive_ = true;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, std::max(0, static_cast<int>(enemyCatalog_.enemies.size()) - 1));
    enemyTestStatus_ = enemyCatalog_.enemies.empty() ? "敵データがありません" : "敵を選んで召喚できます";

    mode_ = ScreenMode::Playing;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    tileMap_ = TileMap{};
    digging_ = DiggingSystem{};
    effects_ = EffectSystem{};
    groundLines_ = GroundLineSystem{};
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    runStats_ = RunStats{};
    warpPoints_.clear();
    rewardNodes_.clear();
    moneyNodes_.clear();
    moonFragmentNodes_.clear();
    chestNodes_.clear();
    crateNodes_.clear();
    enemyNodes_.clear();
    spawnedWarpPointCount_ = 0;
    bossSpawnPoint_ = {};
    hasBossSpawnPoint_ = false;
    bossSpawned_ = false;

    dungeonLayout_ = generateDungeonLayout(DungeonGenerationContext{
        .stageId = 1,
        .seed = 0xE17E57u,
        .stageHardnessMultiplier = 1.0f,
        .roguelike = false,
    });
    player_.position = tileWorldCenter(dungeonLayout_.startTile);
    player_.velocity = {};
    player_.facing = {1.0f, 0.0f};
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    resetPlayerFootstepDust();
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
}

void Game::exitEnemyTestToBase()
{
    enemyTestActive_ = false;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestStatus_.clear();
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    groundLines_ = GroundLineSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    clearTemporaryPlayerState(true);
    enterBase();
}

void Game::spawnSelectedEnemyTestEnemy()
{
    if (enemyCatalog_.enemies.empty()) {
        enemyTestStatus_ = "敵データがありません";
        return;
    }
    enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, static_cast<int>(enemyCatalog_.enemies.size()) - 1);
    const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(enemyTestSelectedIndex_)];
    Vec2 facing = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
    const Vec2 desiredPosition = player_.position + facing * 120.0f;
    if (enemies_.spawnSpecificEnemy(tileMap_, enemy.id, desiredPosition, player_.position, balance_, enemyCatalog_, true, true)) {
        enemyTestStatus_ = "召喚: " + (enemy.name.empty() ? enemy.id : enemy.name);
    } else {
        enemyTestStatus_ = "召喚できませんでした: " + (enemy.name.empty() ? enemy.id : enemy.name);
    }
}

void Game::clearEnemyTestArena()
{
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    effects_ = EffectSystem{};
    groundLines_ = GroundLineSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    enemyTestStatus_ = "敵と弾を消去しました";
}

void Game::updateEnemyTestUi(const Input& input, UiContext& ui)
{
    if (!enemyTestActive_) {
        return;
    }

    const int itemCount = static_cast<int>(enemyCatalog_.enemies.size());
    if (!enemyTestUiVisible_) {
        if (ui.pressed(enemyTestRestoreButtonRect())) {
            enemyTestUiVisible_ = true;
        }
        return;
    }

    if (itemCount > 0) {
        enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, itemCount - 1);
    } else {
        enemyTestSelectedIndex_ = 0;
    }

    std::vector<std::string> labels;
    std::vector<UiDropdownItem> items;
    labels.reserve(enemyCatalog_.enemies.size());
    items.reserve(enemyCatalog_.enemies.size());
    for (std::size_t i = 0; i < enemyCatalog_.enemies.size(); ++i) {
        labels.push_back(enemyTestDropdownItemLabel(enemyCatalog_.enemies[i], static_cast<int>(i)));
        items.push_back(UiDropdownItem{labels.back(), true});
    }

    const bool dropdownWasOpen = enemyTestDropdown_.open;
    const int dropdownSelection = updateUiDropdown(
        enemyTestDropdown_,
        ui,
        input,
        enemyTestSelectButtonRect(),
        enemyTestSelectedIndex_,
        items.empty() ? nullptr : items.data(),
        itemCount,
        enemyTestDropdownStyle());
    if (dropdownSelection >= 0) {
        enemyTestSelectedIndex_ = dropdownSelection;
    }

    if (!dropdownWasOpen && !enemyTestDropdown_.open && (input.confirmPressed() || input.useItemPressed())) {
        spawnSelectedEnemyTestEnemy();
    }

    if (ui.pressed(enemyTestSummonButtonRect())) {
        enemyTestDropdown_.open = false;
        spawnSelectedEnemyTestEnemy();
    }
    if (ui.pressed(enemyTestClearButtonRect())) {
        enemyTestDropdown_.open = false;
        clearEnemyTestArena();
    }
    if (ui.pressed(enemyTestHideButtonRect())) {
        enemyTestUiVisible_ = false;
        enemyTestDropdown_.open = false;
        return;
    }
    if (ui.pressed(enemyTestExitButtonRect())) {
        exitEnemyTestToBase();
        return;
    }
    if (input.backPressed()) {
        if (dropdownWasOpen || enemyTestDropdown_.open) {
            enemyTestDropdown_.open = false;
        } else {
            exitEnemyTestToBase();
        }
        return;
    }

    ui.block(enemyTestToolbarRect());
}

void Game::renderEnemyTestUi(Renderer& renderer) const
{
    if (!enemyTestActive_ || mode_ != ScreenMode::Playing) {
        return;
    }

    renderer.setScreenSpace();
    const int itemCount = static_cast<int>(enemyCatalog_.enemies.size());
    const int selected = itemCount > 0 ? std::clamp(enemyTestSelectedIndex_, 0, itemCount - 1) : 0;

    if (!enemyTestUiVisible_) {
        drawUiRectButton(renderer, enemyTestRestoreButtonRect(), "敵UI", false, uiActionButtonStyle());
        return;
    }

    const UiRect panel = enemyTestToolbarRect();
    renderer.fillRect(panel.pos, panel.size, {12, 18, 34, 218});
    renderer.drawRect(panel.pos, panel.size, {255, 255, 255, 210});
    renderer.drawText(panel.pos + Vec2{18.0f, 31.0f}, "敵テスト", {255, 230, 150, 255}, 2);

    std::string selectedLabel = "敵データなし";
    if (itemCount > 0) {
        const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(selected)];
        selectedLabel = enemyTestEnemyLabel(enemy);
    }

    std::vector<std::string> labels;
    std::vector<UiDropdownItem> items;
    labels.reserve(enemyCatalog_.enemies.size());
    items.reserve(enemyCatalog_.enemies.size());
    for (std::size_t i = 0; i < enemyCatalog_.enemies.size(); ++i) {
        labels.push_back(enemyTestDropdownItemLabel(enemyCatalog_.enemies[i], static_cast<int>(i)));
        items.push_back(UiDropdownItem{labels.back(), true});
    }

    const UiRect selectRect = enemyTestSelectButtonRect();
    drawUiRectButton(renderer, enemyTestSummonButtonRect(), "召喚", false, uiActionButtonStyle());
    drawUiRectButton(renderer, enemyTestClearButtonRect(), "全消去", false, uiCancelButtonStyle());
    drawUiRectButton(renderer, enemyTestHideButtonRect(), "UI非表示", false);
    drawUiRectButton(renderer, enemyTestExitButtonRect(), "終了", false, uiCancelButtonStyle());

    if (!enemyTestStatus_.empty()) {
        renderer.fillRect({18.0f, 144.0f}, {430.0f, 26.0f}, {0, 0, 0, 160});
        renderer.drawText({26.0f, 150.0f}, fittedSingleLineText(renderer, enemyTestStatus_, 410.0f, 2), {255, 230, 150, 255}, 2);
    }

    drawUiDropdown(
        renderer,
        enemyTestDropdown_,
        selectRect,
        selectedLabel,
        items.empty() ? nullptr : items.data(),
        itemCount,
        enemyTestDropdownStyle());
}

void Game::resetBaseEditDragState()
{
    baseEditDraggingFacilityMove_ = false;
    baseEditDraggingFacilityResize_ = false;
    baseEditResizeMask_ = 0;
    baseEditPassPaintActive_ = false;
    baseEditPassPaintSetBlocked_ = false;
    baseEditPassPaintLastTileX_ = std::numeric_limits<int>::min();
    baseEditPassPaintLastTileY_ = std::numeric_limits<int>::min();
}

void Game::enterBaseEditMode()
{
    if (mode_ != ScreenMode::Base) {
        return;
    }
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageQuantityDialog_ = {};
    baseStorageQuantityPending_ = {};
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    baseMerchantSellSource_ = 0;
    baseMerchantSellSourceTabs_ = {};
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandSource_ = 0;
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseProcessingActive_ = false;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseBookshelfActive_ = false;

    baseEditEnabled_ = true;
    baseEditMode_ = BaseEditMode::Facility;
    baseEditSelectedFacilityIndex_ = -1;
    resetBaseEditDragState();
}

void Game::exitBaseEditMode()
{
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    baseEditSelectedFacilityIndex_ = -1;
    resetBaseEditDragState();
}

void Game::pushBaseEditUndoSnapshot()
{
    BaseEditSnapshot snapshot;
    snapshot.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    snapshot.homeFacilityRects = baseFacilityRectsHome_;
    snapshot.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    snapshot.homeBlockedTiles = baseBlockedTilesHome_;
    baseEditUndoStack_.push_back(std::move(snapshot));
    if (static_cast<int>(baseEditUndoStack_.size()) > BaseEditUndoLimit) {
        baseEditUndoStack_.erase(baseEditUndoStack_.begin());
    }
    baseEditRedoStack_.clear();
}

bool Game::undoBaseEdit()
{
    if (baseEditUndoStack_.empty()) {
        return false;
    }

    BaseEditSnapshot current;
    current.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    current.homeFacilityRects = baseFacilityRectsHome_;
    current.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    current.homeBlockedTiles = baseBlockedTilesHome_;
    baseEditRedoStack_.push_back(std::move(current));

    const BaseEditSnapshot snapshot = std::move(baseEditUndoStack_.back());
    baseEditUndoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoor_ = snapshot.outdoorBlockedTiles;
    baseBlockedTilesHome_ = snapshot.homeBlockedTiles;
    baseEditDirty_ = true;
    return true;
}

bool Game::redoBaseEdit()
{
    if (baseEditRedoStack_.empty()) {
        return false;
    }

    BaseEditSnapshot current;
    current.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    current.homeFacilityRects = baseFacilityRectsHome_;
    current.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    current.homeBlockedTiles = baseBlockedTilesHome_;
    baseEditUndoStack_.push_back(std::move(current));
    if (static_cast<int>(baseEditUndoStack_.size()) > BaseEditUndoLimit) {
        baseEditUndoStack_.erase(baseEditUndoStack_.begin());
    }

    const BaseEditSnapshot snapshot = std::move(baseEditRedoStack_.back());
    baseEditRedoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoor_ = snapshot.outdoorBlockedTiles;
    baseBlockedTilesHome_ = snapshot.homeBlockedTiles;
    baseEditDirty_ = true;
    return true;
}

void Game::updateBaseEditScreen(const Input& input, UiContext& ui, float)
{
    if (!baseEditEnabled_ || mode_ != ScreenMode::Base) {
        return;
    }

    if (input.undoShortcutPressed()) {
        if (undoBaseEdit()) {
            baseStatus_ = "Base edit: undo";
        }
    }
    if (input.redoShortcutPressed()) {
        if (redoBaseEdit()) {
            baseStatus_ = "Base edit: redo";
        }
    }
    if (input.saveShortcutPressed()) {
        std::string message;
        if (saveBaseEditData(message)) {
            baseStatus_ = message;
        } else {
            baseStatus_ = message;
        }
    }
    if (input.backPressed()) {
        exitBaseEditMode();
        baseStatus_ = "Base edit: off";
        return;
    }

    if (ui.pressed(baseEditModeButtonRect(0))) {
        baseEditMode_ = BaseEditMode::Facility;
        resetBaseEditDragState();
        return;
    }
    if (ui.pressed(baseEditModeButtonRect(1))) {
        baseEditMode_ = BaseEditMode::Passability;
        resetBaseEditDragState();
        return;
    }
    if (ui.pressed(baseEditSaveButtonRect())) {
        std::string message;
        saveBaseEditData(message);
        baseStatus_ = message;
        return;
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }

    if (baseEditMode_ == BaseEditMode::Facility) {
        if (baseEditSelectedFacilityIndex_ < 0 || baseEditSelectedFacilityIndex_ >= static_cast<int>(facilities.size())) {
            baseEditSelectedFacilityIndex_ = -1;
        }

        if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
            const Vec2 mouse = ui.mouse();
            bool startedDrag = false;
            if (baseEditSelectedFacilityIndex_ >= 0) {
                const BaseFacility& selected = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)];
                const int resizeMask = baseEditResizeMaskAtPoint(selected.rect, mouse);
                if (resizeMask != 0) {
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityResize_ = true;
                    baseEditResizeMask_ = resizeMask;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(selected.rect);
                    startedDrag = true;
                } else if (pointInExpandedRect(mouse, selected.rect, 0.0f)) {
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityMove_ = true;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(selected.rect);
                    startedDrag = true;
                }
            }

            if (!startedDrag) {
                for (int i = static_cast<int>(facilities.size()) - 1; i >= 0; --i) {
                    if (!facilities[static_cast<std::size_t>(i)].rect.contains(mouse)) {
                        continue;
                    }
                    baseEditSelectedFacilityIndex_ = i;
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityMove_ = true;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(facilities[static_cast<std::size_t>(i)].rect);
                    startedDrag = true;
                    break;
                }
            }

            if (!startedDrag) {
                baseEditSelectedFacilityIndex_ = -1;
            } else {
                ui.consumePointer();
            }
        }

        if ((baseEditDraggingFacilityMove_ || baseEditDraggingFacilityResize_) && input.mouseLeftHeld() && baseEditSelectedFacilityIndex_ >= 0) {
            const Vec2 mouse = ui.mouse();
            const Vec2 delta = mouse - baseEditDragStartMouse_;
            BaseEditRect rect = baseEditDragStartRect_;
            if (baseEditDraggingFacilityMove_) {
                rect.x += delta.x;
                rect.y += delta.y;
                rect = normalizeBaseEditRect(rect);
            } else if (baseEditDraggingFacilityResize_) {
                const float minSize = BaseEditFacilityMinSize;
                float left = rect.x;
                float right = rect.x + rect.w;
                float top = rect.y;
                float bottom = rect.y + rect.h;
                if ((baseEditResizeMask_ & 1) != 0) {
                    left += delta.x;
                }
                if ((baseEditResizeMask_ & 2) != 0) {
                    right += delta.x;
                }
                if ((baseEditResizeMask_ & 4) != 0) {
                    top += delta.y;
                }
                if ((baseEditResizeMask_ & 8) != 0) {
                    bottom += delta.y;
                }
                if (right - left < minSize) {
                    if ((baseEditResizeMask_ & 1) != 0 && (baseEditResizeMask_ & 2) == 0) {
                        left = right - minSize;
                    } else {
                        right = left + minSize;
                    }
                }
                if (bottom - top < minSize) {
                    if ((baseEditResizeMask_ & 4) != 0 && (baseEditResizeMask_ & 8) == 0) {
                        top = bottom - minSize;
                    } else {
                        bottom = top + minSize;
                    }
                }
                rect = normalizeBaseEditRect({left, top, right - left, bottom - top});
            }
            const BaseFacility& facility = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)];
            setBaseFacilityRectFor(baseArea_, facility.facilityId, rect);
            baseEditDirty_ = true;
        }

        if (input.mouseLeftReleased()) {
            resetBaseEditDragState();
        }
    } else if (baseEditMode_ == BaseEditMode::Passability) {
        const UiRect map = baseMapBounds();
        const auto toTile = [](Vec2 position) {
            return std::pair<int, int>{
                static_cast<int>(std::floor(position.x / static_cast<float>(BaseEditGridSize))),
                static_cast<int>(std::floor(position.y / static_cast<float>(BaseEditGridSize))),
            };
        };
        const auto validTile = [](int tileX, int tileY) {
            return tileX >= 0 && tileY >= 0 &&
                tileX * BaseEditGridSize < static_cast<int>(baseMapBounds().size.x) &&
                tileY * BaseEditGridSize < static_cast<int>(baseMapBounds().size.y);
        };
        const auto floodFillBlocked = [this, &validTile](int startTileX, int startTileY) {
            if (!validTile(startTileX, startTileY) || isBasePassabilityBlocked(baseArea_, startTileX, startTileY)) {
                return false;
            }

            std::vector<std::pair<int, int>> queue;
            queue.reserve(512);
            queue.push_back({startTileX, startTileY});
            std::size_t head = 0;
            bool changed = false;

            while (head < queue.size()) {
                const auto [tileX, tileY] = queue[head++];
                if (!validTile(tileX, tileY) || isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }

                setBasePassabilityBlocked(baseArea_, tileX, tileY, true);
                changed = true;
                queue.push_back({tileX - 1, tileY});
                queue.push_back({tileX + 1, tileY});
                queue.push_back({tileX, tileY - 1});
                queue.push_back({tileX, tileY + 1});
            }

            return changed;
        };

        if (input.mouseLeftPressed() && !ui.pointerConsumed() && map.contains(ui.mouse())) {
            const auto [tileX, tileY] = toTile(ui.mouse());
            if (validTile(tileX, tileY)) {
                const bool shiftDown = []() {
                    const bool* keys = SDL_GetKeyboardState(nullptr);
                    return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
                }();
                if (shiftDown) {
                    pushBaseEditUndoSnapshot();
                    const bool changed = floodFillBlocked(tileX, tileY);
                    if (!changed && !baseEditUndoStack_.empty()) {
                        baseEditUndoStack_.pop_back();
                    } else if (changed) {
                        baseEditDirty_ = true;
                        baseStatus_ = "Passability fill: blocked";
                    }
                    ui.consumePointer();
                    return;
                }
                pushBaseEditUndoSnapshot();
                baseEditPassPaintActive_ = true;
                baseEditPassPaintSetBlocked_ = !isBasePassabilityBlocked(baseArea_, tileX, tileY);
                setBasePassabilityBlocked(baseArea_, tileX, tileY, baseEditPassPaintSetBlocked_);
                baseEditPassPaintLastTileX_ = tileX;
                baseEditPassPaintLastTileY_ = tileY;
                baseEditDirty_ = true;
                ui.consumePointer();
            }
        }
        if (baseEditPassPaintActive_ && input.mouseLeftHeld() && map.contains(ui.mouse())) {
            const auto [tileX, tileY] = toTile(ui.mouse());
            if (validTile(tileX, tileY) &&
                (tileX != baseEditPassPaintLastTileX_ || tileY != baseEditPassPaintLastTileY_)) {
                setBasePassabilityBlocked(baseArea_, tileX, tileY, baseEditPassPaintSetBlocked_);
                baseEditPassPaintLastTileX_ = tileX;
                baseEditPassPaintLastTileY_ = tileY;
                baseEditDirty_ = true;
            }
        }
        if (input.mouseLeftReleased()) {
            resetBaseEditDragState();
        }
    }

    ui.block(baseMapBounds());
}

void Game::renderBaseEditOverlay(Renderer& renderer) const
{
    if (!baseEditEnabled_ || !basePresentationActive()) {
        return;
    }

    const bool facilityMode = baseEditMode_ == BaseEditMode::Facility;
    const bool passabilityMode = baseEditMode_ == BaseEditMode::Passability;

    if (passabilityMode) {
        const auto& blocked = baseArea_ == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
        for (const std::int64_t packed : blocked) {
            const float x = static_cast<float>(baseEditTileXFromPacked(packed) * BaseEditGridSize);
            const float y = static_cast<float>(baseEditTileYFromPacked(packed) * BaseEditGridSize);
            renderer.fillRect({x, y}, {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)}, {220, 50, 50, 120});
        }
    }

    if (facilityMode) {
        std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
        for (BaseFacility& facility : facilities) {
            facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
        }
        if (baseEditSelectedFacilityIndex_ >= 0 && baseEditSelectedFacilityIndex_ < static_cast<int>(facilities.size())) {
            const UiRect rect = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)].rect;
            renderer.drawRect(rect.pos, rect.size, {70, 220, 255, 255});
            const Vec2 center = rect.pos + rect.size * 0.5f;
            const std::array<Vec2, 8> handles{
                Vec2{rect.pos.x, rect.pos.y},
                Vec2{center.x, rect.pos.y},
                Vec2{rect.pos.x + rect.size.x, rect.pos.y},
                Vec2{rect.pos.x, center.y},
                Vec2{rect.pos.x + rect.size.x, center.y},
                Vec2{rect.pos.x, rect.pos.y + rect.size.y},
                Vec2{center.x, rect.pos.y + rect.size.y},
                Vec2{rect.pos.x + rect.size.x, rect.pos.y + rect.size.y},
            };
            for (Vec2 handle : handles) {
                renderer.fillRect(
                    {handle.x - BaseEditHandleSize * 0.5f, handle.y - BaseEditHandleSize * 0.5f},
                    {BaseEditHandleSize, BaseEditHandleSize},
                    {70, 220, 255, 255});
            }
        }
    }

    renderer.fillRect({884.0f, 38.0f}, {360.0f, 112.0f}, {10, 16, 28, 210});
    renderer.drawRect({884.0f, 38.0f}, {360.0f, 112.0f}, {130, 168, 232, 255});
    drawUiButton(renderer, baseEditModeButtonRect(0), "Facility", facilityMode);
    drawUiButton(renderer, baseEditModeButtonRect(1), "Passability", passabilityMode);
    drawUiButton(renderer, baseEditSaveButtonRect(), "Save", false, uiActionButtonStyle());
    renderer.drawText({898.0f, 132.0f}, baseEditDirty_ ? "Unsaved changes (*)" : "Saved", {255, 230, 150, 255}, 2);
    renderer.drawText({898.0f, 158.0f}, "Ctrl+S save / Ctrl+Z undo / Shift+Click fill / Esc exit", {198, 198, 206, 255}, 2);
}

bool Game::handleBaseEditCommand(std::string_view normalized)
{
    const auto ensureBaseMode = [this]() {
        if (mode_ == ScreenMode::Base) {
            return true;
        }
        logWarning("Debug: base edit is available only while in base.");
        return false;
    };

    if (normalized == "game base-edit toggle") {
        if (!baseEditEnabled_) {
            if (!ensureBaseMode()) {
                return true;
            }
            enterBaseEditMode();
            logInfo("Debug: base edit enabled.");
        } else {
            exitBaseEditMode();
            logInfo("Debug: base edit disabled.");
        }
        return true;
    }
    if (normalized == "game base-edit on") {
        if (!ensureBaseMode()) {
            return true;
        }
        enterBaseEditMode();
        logInfo("Debug: base edit enabled.");
        return true;
    }
    if (normalized == "game base-edit off") {
        exitBaseEditMode();
        logInfo("Debug: base edit disabled.");
        return true;
    }
    if (normalized == "game base-edit facility") {
        if (!ensureBaseMode()) {
            return true;
        }
        if (!baseEditEnabled_) {
            enterBaseEditMode();
        }
        baseEditMode_ = BaseEditMode::Facility;
        logInfo("Debug: base edit mode = facility.");
        return true;
    }
    if (normalized == "game base-edit passability") {
        if (!ensureBaseMode()) {
            return true;
        }
        if (!baseEditEnabled_) {
            enterBaseEditMode();
        }
        baseEditMode_ = BaseEditMode::Passability;
        logInfo("Debug: base edit mode = passability.");
        return true;
    }
    if (normalized == "game base-edit save") {
        if (!ensureBaseMode()) {
            return true;
        }
        std::string message;
        if (saveBaseEditData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleObjectImageScaleCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game obj-image-scale toggle" || normalized == "game image-scale toggle";
    const bool enable = normalized == "game obj-image-scale on" || normalized == "game image-scale on";
    const bool disable = normalized == "game obj-image-scale off" || normalized == "game image-scale off";
    const bool save = normalized == "game obj-image-scale save" || normalized == "game image-scale save";

    if (toggle) {
        if (mode_ == ScreenMode::ObjectImageScaleEdit) {
            exitObjectImageScaleEditMode();
            logInfo("Debug: image scale edit disabled.");
        } else {
            enterObjectImageScaleEditMode();
            logInfo("Debug: image scale edit enabled.");
        }
        return true;
    }
    if (enable) {
        enterObjectImageScaleEditMode();
        logInfo("Debug: image scale edit enabled.");
        return true;
    }
    if (disable) {
        exitObjectImageScaleEditMode();
        logInfo("Debug: image scale edit disabled.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveObjectImageScaleData(message)) {
            objectImageScaleStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            objectImageScaleStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleAudioCueEditCommand(std::string_view normalized)
{
    const bool bgm = normalized == "game audio-edit bgm" ||
        normalized == "game bgm-edit" ||
        normalized == "game audio bgm";
    const bool se = normalized == "game audio-edit se" ||
        normalized == "game se-edit" ||
        normalized == "game sfx-edit" ||
        normalized == "game audio se";
    const bool close = normalized == "game audio-edit close" ||
        normalized == "game audio-edit off";
    const bool save = normalized == "game audio-edit save" ||
        normalized == "game audio save";

    if (bgm) {
        enterAudioCueEditMode(AudioCueEditMode::Bgm);
        logInfo("Debug: BGM edit opened.");
        return true;
    }
    if (se) {
        enterAudioCueEditMode(AudioCueEditMode::Se);
        logInfo("Debug: sound effect edit opened.");
        return true;
    }
    if (close) {
        exitAudioCueEditMode();
        logInfo("Debug: audio edit closed.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveAudioCueManifestFromEdit(message)) {
            audioCueEditStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            audioCueEditStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleDebugItemPickerCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game items picker" ||
        normalized == "game item picker" ||
        normalized == "game item-picker toggle" ||
        normalized == "game items-picker toggle";
    const bool open = normalized == "game items picker on" ||
        normalized == "game item-picker on" ||
        normalized == "game items add-select";
    const bool close = normalized == "game items picker off" ||
        normalized == "game item-picker off" ||
        normalized == "game item-picker close";

    if (toggle) {
        if (debugItemPickerActive_) {
            closeDebugItemPicker();
            logInfo("Debug: item picker closed.");
        } else {
            openDebugItemPicker();
            if (debugItemPickerActive_) {
                logInfo("Debug: item picker opened.");
            }
        }
        return true;
    }
    if (open) {
        openDebugItemPicker();
        if (debugItemPickerActive_) {
            logInfo("Debug: item picker opened.");
        }
        return true;
    }
    if (close) {
        closeDebugItemPicker();
        logInfo("Debug: item picker closed.");
        return true;
    }

    return false;
}

bool Game::handleDebugStoryTestCommand(std::string_view normalized)
{
    const bool events = normalized == "game story-test events" ||
        normalized == "game story test events" ||
        normalized == "game event-test" ||
        normalized == "game event test";
    const bool tutorials = normalized == "game story-test tutorials" ||
        normalized == "game story test tutorials" ||
        normalized == "game tutorial-test" ||
        normalized == "game tutorial test";
    const bool close = normalized == "game story-test close" ||
        normalized == "game story test close" ||
        normalized == "game dialogue-test close";

    if (events) {
        openDebugStoryTest(DebugStoryTestMode::Events);
        if (debugStoryTestActive_) {
            logInfo("Debug: story event test opened.");
        }
        return true;
    }
    if (tutorials) {
        openDebugStoryTest(DebugStoryTestMode::Tutorials);
        if (debugStoryTestActive_) {
            logInfo("Debug: story tutorial test opened.");
        }
        return true;
    }
    if (close) {
        closeDebugStoryTest();
        logInfo("Debug: story test closed.");
        return true;
    }

    return false;
}

bool Game::executeDebugCommand(std::string_view command)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(command)));

    if (handleBaseEditCommand(normalized)) {
        return true;
    }
    if (handleObjectImageScaleCommand(normalized)) {
        return true;
    }
    if (handleAudioCueEditCommand(normalized)) {
        return true;
    }
    if (handleDebugItemPickerCommand(normalized)) {
        return true;
    }
    if (handleDebugStoryTestCommand(normalized)) {
        return true;
    }

    const auto applyStageUnlockDebugCommand = [&](int unlockedStoryStages, std::string_view label) {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::EndingKamishibai && mode_ != ScreenMode::Title) {
            returnToBaseFromNormalStage(false, false);
        }
        applyDebugStageUnlockState(unlockedStoryStages);
        logInfo("Debug: stage unlock state set to " + std::string(label) + ".");
        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: stage unlock state saved.");
        } else {
            logWarning("Debug: stage unlock state changed in memory, but save failed: " + saveMessage);
        }
        return true;
    };

    const auto eraseStoryFlag = [&](std::string_view flag) {
        storyFlags_.erase(
            std::remove(storyFlags_.begin(), storyFlags_.end(), std::string(flag)),
            storyFlags_.end());
    };

    const auto clearRuntimeDebugPresentation = [&]() {
        pendingStoryTriggers_.clear();
        dialogue_.clear();
        endingKamishibaiPending_ = false;
        resetBossEncounter();
        screenTransition_ = ScreenTransitionState{};
        worldBuildJob_ = WorldBuildJob{};
        inventory_.setOpen(false);
        inventory_.cancelGrab();
        cancelRingGrab();
        closeDebugItemPicker();
        closeDebugStoryTest();
        if (levels_.isChoosing()) {
            levels_ = LevelSystem{};
        }
        levelUpResultDialog_ = {};
        baseRegenerateConfirmActive_ = false;
        baseWarpPointSelectActive_ = false;
        warpReturnConfirmActive_ = false;
    };

    const auto storyUnlockCountForStageId = [&](std::string_view stageId) {
        if (stageId == "stage_04_astral_mine") {
            return 2;
        }

        int storyStageNumber = 0;
        for (const StageDefinition& stage : stageCatalog_.getStagesSortedByDisplayOrder()) {
            if (debugIsRoguelikeStageDefinition(stage)) {
                continue;
            }
            ++storyStageNumber;
            if (stage.id == stageId) {
                return std::max(1, storyStageNumber);
            }
        }
        return 1;
    };

    const auto selectDebugStage = [&](std::string_view stageId) {
        if (stageId.empty() || stageCatalog_.getStageById(stageId) == nullptr) {
            logWarning("Debug: stage not found: " + std::string(stageId));
            return false;
        }
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        }

        unlockedStages_ = std::max(unlockedStages_, storyUnlockCountForStageId(stageId));
        currentStageId_ = std::string(stageId);
        currentStage_ = stageCatalogIndexForId(currentStageId_);
        resolveCurrentStageDefinition();
        syncWarpStateForCurrentStage();
        baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
        baseWarpPointSelectActive_ = false;
        baseWarpPointSelection_ = 0;
        baseRegenerateConfirmActive_ = false;
        return true;
    };

    const auto markStoryTriggerSeenForCurrentStage = [&](std::string_view triggerName) {
        const std::string trigger = currentStageStoryTrigger(triggerName);
        if (trigger.empty()) {
            return;
        }
        for (const StoryEvent& event : storyEvents_) {
            if (event.trigger == trigger && !event.onceFlag.empty()) {
                addStoryFlag(event.onceFlag);
            }
        }
    };

    const auto eraseStoryTriggerSeenForCurrentStage = [&](std::string_view triggerName) {
        const std::string trigger = currentStageStoryTrigger(triggerName);
        if (trigger.empty()) {
            return;
        }
        for (const StoryEvent& event : storyEvents_) {
            if (event.trigger == trigger && !event.onceFlag.empty()) {
                eraseStoryFlag(event.onceFlag);
            }
        }
    };

    const auto markCurrentStageClearedForDebug = [&]() {
        const std::string clearFlag = stageClearFlagForStage(currentStageId_);
        if (!clearFlag.empty()) {
            addStoryFlag(clearFlag);
        }
        unlockedStages_ = std::max(unlockedStages_, storyUnlockCountForStageId(currentStageId_) + 1);
        markStoryTriggerSeenForCurrentStage("boss_before");
        markStoryTriggerSeenForCurrentStage("boss_after");
        markStoryTriggerSeenForCurrentStage("stage_clear");
        syncWarpStateForCurrentStage();
    };

    const auto enterDebugBase = [&]() {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        }
        enterBase();
        baseMiningStartChoiceActive_ = false;
        baseWarpPointSelectActive_ = false;
        baseRegenerateConfirmActive_ = false;
        baseStatus_.clear();
    };

    const auto placePlayerAtBossApproach = [&]() {
        if (!hasBossSpawnPoint_) {
            return;
        }
        Vec2 direction = normalize(bossSpawnPoint_);
        if (lengthSquared(direction) <= 0.0001f) {
            direction = {1.0f, 0.0f};
        }
        player_.position = bossSpawnPoint_ - direction * (BossSpawnTriggerRadius + 18.0f);
        player_.facing = direction;
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        camera_.follow(player_.position, 1.0f);
    };

    const auto buildDebugDungeonWithAllWarps = [&](bool markCleared, bool suppressBossBeforeStory) {
        if (suppressBossBeforeStory) {
            markStoryTriggerSeenForCurrentStage("boss_before");
        }
        clearRuntimeDebugPresentation();
        initializeWorld(false);
        if (!unlockAllWarpPointsForCurrentDungeon()) {
            logWarning("Debug: failed to unlock warp points for " + currentStageId_ + ".");
            return false;
        }
        if (markCleared) {
            markCurrentStageClearedForDebug();
        }
        return true;
    };

    const auto removeCapturedBossForCurrentStage = [&]() {
        const std::string objectId = currentStageBossCaptureObjectId();
        if (objectId.empty()) {
            return 0;
        }

        int removedCount = 0;
        while (inventory_.removeObjectItemCount(objectId, 1)) {
            ++removedCount;
        }

        std::vector<std::string> instanceIds;
        for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
            if (debugObjectInstanceMatchesObjectId(instance, objectId)) {
                instanceIds.push_back(instance.instance.instanceId);
            }
        }
        for (const std::string& instanceId : instanceIds) {
            if (inventory_.removeObjectInstance(instanceId)) {
                ++removedCount;
            }
        }

        for (int i = static_cast<int>(warehouseObjectStacks_.size()) - 1; i >= 0; --i) {
            InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(i)];
            if (stack.objectId != objectId) {
                continue;
            }
            removedCount += std::max(1, stack.count);
            removeWarehouseDisplaySlotAtEntryIndex(i);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + i);
        }

        for (int i = static_cast<int>(warehouseObjectInstances_.size()) - 1; i >= 0; --i) {
            const InventoryObjectInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(i)];
            if (!debugObjectInstanceMatchesObjectId(instance, objectId)) {
                continue;
            }
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + i);
            warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + i);
            ++removedCount;
        }

        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const auto before = ringItems.size();
            ringItems.erase(
                std::remove_if(ringItems.begin(), ringItems.end(), [&](const SpellRingItem& item) {
                    return item.objectId == objectId;
                }),
                ringItems.end());
            removedCount += static_cast<int>(before - ringItems.size());
        }

        if (removedCount > 0) {
            warehouseDisplaySlots_.clear();
            refreshOrbitEffects();
        }
        return removedCount;
    };

    const auto resetBossFlowStoryForCurrentStage = [&]() {
        eraseStoryTriggerSeenForCurrentStage("boss_before");
        eraseStoryTriggerSeenForCurrentStage("boss_after");
        eraseStoryTriggerSeenForCurrentStage("stage_clear");
        eraseStoryFlag(stageClearFlagForStage(currentStageId_));
        if (currentStageId_ == DebugFinalStoryStageId) {
            eraseStoryFlag(DebugEndingSeenFlag);
            eraseStoryFlag(DebugEndingMainFlag);
            eraseStoryFlag(DebugStage03ClearFlag);
            eraseStoryFlag(DebugPostEndingIntroFlag);
        }
    };

    const auto parseDebugInt = [](std::string_view text, int fallback) {
        const std::string trimmed = trimAscii(std::string(text));
        if (trimmed.empty()) {
            return fallback;
        }
        try {
            return std::stoi(trimmed);
        } catch (...) {
            return fallback;
        }
    };

    const auto distortionLabel = [](AstralDistortionKind distortion) {
        switch (distortion) {
        case AstralDistortionKind::FadingStarlight:
            return "星明かりが遠のく";
        case AstralDistortionKind::StarHardened:
            return "星硬化";
        case AstralDistortionKind::EchoSpawn:
            return "残響湧き";
        case AstralDistortionKind::None:
            break;
        }
        return "なし";
    };

    const auto resultForDebugToken = [](std::string_view token) {
        if (token == "died" || token == "death") {
            return AstralRunResult::Died;
        }
        if (token == "dragon-defeated" || token == "dragon") {
            return AstralRunResult::DragonDefeated;
        }
        return AstralRunResult::Returned;
    };

    const auto resultLabel = [](AstralRunResult result) {
        switch (result) {
        case AstralRunResult::Returned:
            return "帰還成功";
        case AstralRunResult::Died:
            return "死亡";
        case AstralRunResult::DragonDefeated:
            return "星脈竜撃破";
        case AstralRunResult::None:
            break;
        }
        return "なし";
    };

    const auto astralDungeonReady = [&]() {
        if (worldBuildJob_.active) {
            logWarning("Debug: astral dungeon is still loading.");
            return false;
        }
        if (!currentStageIsRoguelike() || mode_ != ScreenMode::Playing) {
            logWarning("Debug: astral command requires an active 星間廃坑 run.");
            return false;
        }
        return true;
    };

    const auto forceAstralDepthAndDistortion = [&](int depth) {
        if (!astralRunActive()) {
            return;
        }
        const int maxDepth = std::max(1, astralRun_.maxDepth);
        const int clampedDepth = std::clamp(depth, 1, maxDepth);
        astralRun_.currentDepth = clampedDepth;
        astralRun_.maxReachedDepth = std::max(astralRun_.maxReachedDepth, clampedDepth);
        astralRun_.distortion = chooseAstralDistortionForDepth(clampedDepth, AstralDistortionKind::None);
        applyAstralDistortionToLayout();
    };

    const auto placePlayerForAstralDebug = [&](Vec2 position, int forcedDepth) {
        if (forcedDepth > 0) {
            forceAstralDepthAndDistortion(forcedDepth);
        }
        player_.position = safePlayerStartPosition(position);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        camera_.follow(player_.position, 1.0f);
        updateAstralRunProgress();
    };

    const auto mainPathPositionForDepth = [&](int depth) {
        if (dungeonLayout_.mainPathPoints.empty()) {
            return tileWorldCenter(dungeonLayout_.startTile);
        }
        const int maxDepth = std::max(1, astralRun_.maxDepth);
        const float progress = maxDepth <= 1
            ? 0.0f
            : static_cast<float>(std::clamp(depth, 1, maxDepth) - 1) / static_cast<float>(maxDepth - 1);
        const int index = std::clamp(
            static_cast<int>(std::round(progress * static_cast<float>(dungeonLayout_.mainPathPoints.size() - 1))),
            0,
            static_cast<int>(dungeonLayout_.mainPathPoints.size() - 1));
        return tileWorldCenter(roundDungeonTile(dungeonLayout_.mainPathPoints[static_cast<std::size_t>(index)]));
    };

    const auto applyDebugAstralDistortion = [&]() {
        if (!astralRunActive()) {
            logInfo("Debug: astral distortion mode set to " + debugAstralDistortionMode_ + " for the next active run.");
            return;
        }
        astralRun_.distortion = chooseAstralDistortionForDepth(astralRun_.currentDepth, AstralDistortionKind::None);
        applyAstralDistortionToLayout();
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        logInfo("Debug: astral distortion => " + std::string(distortionLabel(astralRun_.distortion)) + ".");
    };

    constexpr std::string_view AstralDepthPrefix = "game astral depth ";
    if (normalized.rfind(AstralDepthPrefix, 0) == 0) {
        debugAstralDepth_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(AstralDepthPrefix.size()), debugAstralDepth_),
            1,
            9);
        logInfo("Debug: astral depth target => " + std::to_string(debugAstralDepth_) + ".");
        return true;
    }

    constexpr std::string_view AstralMoveTargetPrefix = "game astral move-target ";
    if (normalized.rfind(AstralMoveTargetPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralMoveTargetPrefix.size()));
        if (token == "entrance" || token == "depth" || token == "boss" || token == "return") {
            debugAstralMoveTarget_ = token;
            logInfo("Debug: astral move target => " + token + ".");
        } else {
            logWarning("Debug: unknown astral move target: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralDistortionPrefix = "game astral distortion ";
    if (normalized.rfind(AstralDistortionPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralDistortionPrefix.size()));
        if (token == "auto" || token == "none" || token == "fading-starlight" ||
            token == "star-hardened" || token == "echo-spawn") {
            debugAstralDistortionMode_ = token;
            applyDebugAstralDistortion();
        } else {
            logWarning("Debug: unknown astral distortion mode: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralRoomPrefix = "game astral room ";
    if (normalized.rfind(AstralRoomPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralRoomPrefix.size()));
        if (debugSpecialRoomTypeForToken(token) == SpecialRoomType::None) {
            logWarning("Debug: unknown astral special room: " + token);
        } else {
            debugAstralRoomType_ = token;
            logInfo("Debug: astral special room target => " + token + ".");
        }
        return true;
    }

    constexpr std::string_view AstralRoomIndexPrefix = "game astral room-index ";
    if (normalized.rfind(AstralRoomIndexPrefix, 0) == 0) {
        debugAstralRoomIndex_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(AstralRoomIndexPrefix.size()), debugAstralRoomIndex_),
            1,
            20);
        logInfo("Debug: astral room index => " + std::to_string(debugAstralRoomIndex_) + ".");
        return true;
    }

    constexpr std::string_view AstralResultPrefix = "game astral result ";
    if (normalized.rfind(AstralResultPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralResultPrefix.size()));
        if (token == "returned" || token == "died" || token == "dragon-defeated") {
            debugAstralResultKind_ = token;
            logInfo("Debug: astral result target => " + token + ".");
        } else {
            logWarning("Debug: unknown astral result: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralStatOverridePrefix = "game astral stat-override ";
    if (normalized.rfind(AstralStatOverridePrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralStatOverridePrefix.size()));
        debugAstralStatOverride_ = token == "on" || token == "true" || token == "1";
        logInfo(std::string("Debug: astral stat override => ") + (debugAstralStatOverride_ ? "on." : "off."));
        return true;
    }

    constexpr std::string_view AstralStatPrefix = "game astral stat ";
    if (normalized.rfind(AstralStatPrefix, 0) == 0) {
        const std::string rest = trimAscii(normalized.substr(AstralStatPrefix.size()));
        const std::size_t split = rest.find(' ');
        const std::string key = split == std::string::npos ? rest : rest.substr(0, split);
        const std::string valueText = split == std::string::npos ? std::string{} : rest.substr(split + 1);
        const int value = parseDebugInt(valueText, 0);
        if (key == "kills") {
            debugAstralStatKills_ = std::clamp(value, 0, 9999);
        } else if (key == "dug") {
            debugAstralStatDugTiles_ = std::clamp(value, 0, 99999);
        } else if (key == "items") {
            debugAstralStatItems_ = std::clamp(value, 0, 9999);
        } else if (key == "materials") {
            debugAstralStatMaterials_ = std::clamp(value, 0, 99999);
        } else if (key == "money") {
            debugAstralStatMoney_ = std::clamp(value, 0, 999999);
        } else {
            logWarning("Debug: unknown astral stat key: " + key);
            return true;
        }
        logInfo("Debug: astral stat " + key + " => " + std::to_string(value) + ".");
        return true;
    }

    if (normalized == "game astral start-new") {
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage("stage_04_astral_mine")) {
            startMiningFromBase(false, true);
            logInfo("Debug: astral new run started.");
        }
        return true;
    }

    if (normalized == "game astral regenerate") {
        clearRuntimeDebugPresentation();
        if (!currentStageIsRoguelike()) {
            enterDebugBase();
            if (!selectDebugStage("stage_04_astral_mine")) {
                return true;
            }
        }
        startMiningFromBase(false, true);
        logInfo("Debug: astral run regenerated.");
        return true;
    }

    if (normalized == "game astral return-base") {
        if (mode_ == ScreenMode::AstralResult) {
            returnToBaseAfterAstralResult();
        } else if (basePresentationActive()) {
            enterDebugBase();
        } else {
            returnToBaseFromNormalStage(false, false);
        }
        logInfo("Debug: returned from astral debug.");
        return true;
    }

    if (normalized == "game astral warp") {
        if (!astralDungeonReady()) {
            return true;
        }

        if (debugAstralMoveTarget_ == "boss") {
            if (!hasBossSpawnPoint_) {
                logWarning("Debug: astral boss point is not available.");
                return true;
            }
            Vec2 direction = normalize(bossSpawnPoint_);
            if (lengthSquared(direction) <= 0.0001f) {
                direction = {1.0f, 0.0f};
            }
            placePlayerForAstralDebug(bossSpawnPoint_ - direction * (BossSpawnTriggerRadius + 18.0f), astralRun_.maxDepth);
        } else if (debugAstralMoveTarget_ == "return") {
            placePlayerForAstralDebug(dungeonEntrancePosition(), 1);
        } else if (debugAstralMoveTarget_ == "entrance") {
            placePlayerForAstralDebug(tileWorldCenter(dungeonLayout_.startTile), 1);
        } else {
            placePlayerForAstralDebug(mainPathPositionForDepth(debugAstralDepth_), debugAstralDepth_);
        }
        logInfo("Debug: astral player warped to " + debugAstralMoveTarget_ + ".");
        return true;
    }

    if (normalized == "game astral room-warp") {
        if (!astralDungeonReady()) {
            return true;
        }
        const SpecialRoomType targetType = debugSpecialRoomTypeForToken(debugAstralRoomType_);
        const int targetIndex = std::max(1, debugAstralRoomIndex_);
        int seen = 0;
        for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
            if (room.type != targetType) {
                continue;
            }
            ++seen;
            if (seen != targetIndex) {
                continue;
            }
            const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, room.center);
            const int depth = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
            placePlayerForAstralDebug(tileWorldCenter(roundDungeonTile(room.center)), depth);
            logInfo("Debug: warped to astral special room " +
                std::string(debugSpecialRoomTypeLabel(targetType)) +
                " #" + std::to_string(targetIndex) + ".");
            return true;
        }
        logWarning("Debug: astral special room not found: " +
            std::string(debugSpecialRoomTypeLabel(targetType)) +
            " #" + std::to_string(targetIndex) + ".");
        return true;
    }

    if (normalized == "game astral report-generation") {
        const int maxDepth = astralRunActive() ? std::max(1, astralRun_.maxDepth) : 9;
        std::array<int, 6> roomCounts{};
        for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
            const int index = static_cast<int>(room.type);
            if (index >= 0 && index < static_cast<int>(roomCounts.size())) {
                ++roomCounts[static_cast<std::size_t>(index)];
            }
        }
        logInfo("Debug: astral generation stage=" + currentStageId_ +
            " active=" + (astralRunActive() ? std::string("true") : std::string("false")) +
            " depth=" + std::to_string(astralRun_.currentDepth) + "/" + std::to_string(maxDepth) +
            " distortionMode=" + debugAstralDistortionMode_ +
            " currentDistortion=" + distortionLabel(astralRun_.distortion));
        logInfo("Debug: astral rooms ore=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::OreRoom)]) +
            " safe=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::SafeCavern)]) +
            " coin=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::CoinRoom)]) +
            " treasure=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::TreasureRoom)]) +
            " enemy=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::EnemyRoom)]) +
            " boss=" + (hasBossSpawnPoint_ ? "true" : "false"));
        return true;
    }

    if (normalized == "game astral report-stats") {
        logInfo("Debug: astral stats kills=" + std::to_string(runStats_.defeatedEnemies) +
            " dug=" + std::to_string(runStats_.dugTiles) +
            " items=" + std::to_string(runStats_.acquiredObjectItems) +
            " materials=" + std::to_string(astralRunMaterialDeltaFromStart()) +
            " money=" + std::to_string(astralRunMoneyDeltaFromStart()) +
            " highScore=" + std::to_string(astralHighScore_));
        return true;
    }

    if (normalized == "game astral high-score reset") {
        astralHighScore_ = 0;
        logInfo("Debug: astral high score reset.");
        return true;
    }

    if (normalized == "game astral finish") {
        const AstralRunResult result = resultForDebugToken(debugAstralResultKind_);
        const int highScoreBeforeResult = astralHighScore_;
        enterAstralResult(result);
        if (debugAstralStatOverride_) {
            astralHighScore_ = highScoreBeforeResult;
            AstralRunSummary summary = makeAstralRunSummary(result);
            summary.reachedDepth = std::clamp(debugAstralDepth_, 1, std::max(1, summary.maxDepth));
            summary.defeatedEnemies = std::max(0, debugAstralStatKills_);
            summary.dugTiles = std::max(0, debugAstralStatDugTiles_);
            summary.acquiredItems = std::max(0, debugAstralStatItems_);
            summary.acquiredMaterials = std::max(0, debugAstralStatMaterials_);
            summary.acquiredMoney = std::max(0, debugAstralStatMoney_);
            summary.carriedOut = result != AstralRunResult::Died;
            summary.score = calculateAstralRunScore(summary);
            summary.highScore = astralHighScore_;
            summary.highScoreUpdated = false;
            if (summary.carriedOut && summary.score > astralHighScore_) {
                astralHighScore_ = summary.score;
                summary.highScore = astralHighScore_;
                summary.highScoreUpdated = true;
            }
            astralResult_ = summary;
        } else if (mode_ == ScreenMode::AstralResult && astralResult_.result != result) {
            AstralRunSummary summary = makeAstralRunSummary(result);
            if (summary.carriedOut && summary.score > astralHighScore_) {
                astralHighScore_ = summary.score;
                summary.highScore = astralHighScore_;
                summary.highScoreUpdated = true;
            }
            astralResult_ = summary;
        }
        logInfo("Debug: astral result shown: " + std::string(resultLabel(result)) + ".");
        return true;
    }

    constexpr std::string_view BossFlowTargetPrefix = "game boss-flow target ";
    if (normalized.rfind(BossFlowTargetPrefix, 0) == 0) {
        const std::string stageId = debugStageIdForToken(std::string_view(normalized).substr(BossFlowTargetPrefix.size()));
        if (stageId.empty() || stageId == "stage_04_astral_mine") {
            logWarning("Debug: unknown boss-flow target: " + normalized.substr(BossFlowTargetPrefix.size()));
            return true;
        }
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage(stageId)) {
            logInfo("Debug: boss flow target set to " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow before") {
        clearRuntimeDebugPresentation();
        resetBossFlowStoryForCurrentStage();
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, false)) {
            placePlayerAtBossApproach();
            baseStatus_.clear();
            logInfo("Debug: boss flow set to boss approach for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow defeated") {
        clearRuntimeDebugPresentation();
        resetBossFlowStoryForCurrentStage();
        markStoryTriggerSeenForCurrentStage("boss_before");
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            const Vec2 defeatPosition = hasBossSpawnPoint_ ? bossSpawnPoint_ : player_.position;
            player_.position = defeatPosition;
            camera_.follow(player_.position, 1.0f);
            beginBossDefeatSequence(defeatPosition);
            logInfo("Debug: boss flow set to defeated presentation for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow clear") {
        clearRuntimeDebugPresentation();
        markStoryTriggerSeenForCurrentStage("boss_before");
        markStoryTriggerSeenForCurrentStage("boss_after");
        if (buildDebugDungeonWithAllWarps(true, true)) {
            enterStageClear();
            logInfo("Debug: boss flow set to stage clear result for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game reset-data") {
        money_ = 0;
        maxHpUpgradeLevel_ = 0;
        ringRadiusUpgradeLevel_ = 0;
        ringSpeedUpgradeLevel_ = 0;
        collectionRangeUpgradeLevel_ = 0;
        levelRingUpgradePoints_ = {};
        workshopInitialRadiusLevel_ = 0;
        workshopInitialSpeedLevel_ = 0;
        workshopShiftDistanceLevel_ = 0;
        merchantRefreshPending_ = false;
        merchantUpgradeLevel_ = 1;
        merchantStockVersion_ = 0;
        merchantStock_.clear();
        highValueBuyCategory_.clear();
        highValueBuyObjectIds_.clear();
        warehouseCapacityLevel_ = 0;
        processingUnlockLevel_ = 0;
        ringWorkshopUnlocked_ = false;
        autoSaveOnReturn_ = false;
        storyFlags_.clear();
        warehouseObjectStacks_.clear();
        warehouseObjectInstances_.clear();
        unlockedStages_ = 1;
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        currentStage_ = 0;
        currentStageId_ = "stage_01_stardust";
        resolveCurrentStageDefinition();
        encyclopedia_ = EncyclopediaSystem{};
        encyclopediaOwnedSyncSuppressCounts_.clear();
        encyclopediaRingSyncSuppressCounts_.clear();
        initializeWorld(false);
        enterBase();
        captureRunStartInventoryState();

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: game data reset and saved.");
        } else {
            logWarning("Debug: game data reset in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game codex reset" || normalized == "game encyclopedia reset") {
        encyclopedia_ = EncyclopediaSystem{};
        captureEncyclopediaSyncSuppressState();
        reloadNotice_ = "図鑑をリセット";
        reloadNoticeTimer_ = 1.8f;
        baseStatus_ = "図鑑をリセットしました";

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: codex reset and saved.");
        } else {
            logWarning("Debug: codex reset in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game stage-unlock initial" ||
        normalized == "game stage-unlock reset" ||
        normalized == "game stage-unlock stage1" ||
        normalized == "game stage-unlock 初期状態") {
        return applyStageUnlockDebugCommand(1, "initial");
    }

    if (normalized == "game stage-unlock stage2" ||
        normalized == "game stage-unlock 2" ||
        normalized == "game stage-unlock ステージ2解放") {
        return applyStageUnlockDebugCommand(2, "stage2");
    }

    if (normalized == "game stage-unlock stage3" ||
        normalized == "game stage-unlock 3" ||
        normalized == "game stage-unlock ステージ3解放") {
        return applyStageUnlockDebugCommand(3, "stage3");
    }

    if (normalized == "game return-base") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
            logInfo("Debug: enemy test exited to base.");
            return true;
        }
        returnToBaseFromNormalStage(false, false);
        logInfo("Debug: returned to base.");
        return true;
    }

    if (normalized == "game warp-points unlock-all" ||
        normalized == "game warp-point unlock-all" ||
        normalized == "game unlock-warps") {
        if (unlockAllWarpPointsForCurrentDungeon()) {
            logInfo("Debug: all warp points unlocked for current dungeon.");
        } else {
            logWarning("Debug: warp point unlock-all requires an active dungeon with warp points.");
        }
        return true;
    }

    constexpr std::string_view RematchTargetPrefix = "game rematch target ";
    if (normalized.rfind(RematchTargetPrefix, 0) == 0) {
        const std::string stageId = debugStageIdForToken(std::string_view(normalized).substr(RematchTargetPrefix.size()));
        if (stageId.empty()) {
            logWarning("Debug: unknown rematch target: " + normalized.substr(RematchTargetPrefix.size()));
            return true;
        }
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage(stageId)) {
            logInfo("Debug: rematch target set to " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch unlock-warps") {
        clearRuntimeDebugPresentation();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            returnToBaseFromNormalStage(false, false);
            logInfo("Debug: all warp points discovered for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch mark-clear") {
        clearRuntimeDebugPresentation();
        markCurrentStageClearedForDebug();
        enterDebugBase();
        logInfo("Debug: stage marked cleared for rematch: " + currentStageId_ + ".");
        return true;
    }

    if (normalized == "game rematch setup-regenerate") {
        clearRuntimeDebugPresentation();
        if (buildDebugDungeonWithAllWarps(true, true)) {
            returnToBaseFromNormalStage(false, false);
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = 2;
            logInfo("Debug: regenerate-ready rematch state prepared for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch captured-boss add") {
        clearRuntimeDebugPresentation();
        if (hasCapturedBossForCurrentStage()) {
            logInfo("Debug: captured boss already owned for " + currentStageId_ + ".");
            return true;
        }

        ItemData capturedBoss;
        capturedBoss.id = currentStageBossCaptureObjectId();
        capturedBoss.name = "捕獲ボス";
        capturedBoss.category = "捕獲";
        capturedBoss.description = "デバッグ用の捕獲済みボス判定アイテムです。";
        capturedBoss.rarity = 5;
        capturedBoss.price = 0;
        capturedBoss.damageType = "none";
        capturedBoss.durability = -1;
        capturedBoss.weightKg = 1.0;
        capturedBoss.tags = {"captured_boss"};
        if (inventory_.addRuntimeObjectItem(capturedBoss)) {
            logInfo("Debug: captured boss item added for " + currentStageId_ + ".");
        } else {
            logWarning("Debug: failed to add captured boss item; inventory may be full.");
        }
        return true;
    }

    if (normalized == "game rematch captured-boss remove") {
        clearRuntimeDebugPresentation();
        const int removedCount = removeCapturedBossForCurrentStage();
        logInfo("Debug: captured boss items removed: " + std::to_string(removedCount) + ".");
        return true;
    }

    if (normalized == "game launch-mode pre-title" ||
        normalized == "game launch-mode before-title" ||
        normalized == "game launch-mode opening") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::EndingKamishibai && mode_ != ScreenMode::Title) {
            returnToBaseFromNormalStage(false, false);
        } else {
            enterBase();
        }
        startOpeningKamishibai();
        logInfo("Debug: launch mode set to pre-title opening.");
        return true;
    }

    if (normalized == "game launch-mode base") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (basePresentationActive() || mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title) {
            enterBase();
        } else {
            returnToBaseFromNormalStage(false, false);
        }
        logInfo("Debug: launch mode set to base.");
        return true;
    }

    if (normalized == "game launch-mode dungeon") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        }
        startMiningFromBase(false, false);
        logInfo("Debug: launch mode set to dungeon start.");
        return true;
    }

    if (normalized == "game launch-mode enemy-test") {
        enterEnemyTestMode();
        logInfo("Debug: launch mode set to enemy test.");
        return true;
    }

    if (normalized == "game launch-mode final-boss-before" ||
        normalized == "game launch-mode final-boss") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        eraseStoryFlag("story_stage_03_boss_before");
        eraseStoryFlag("story_stage_03_boss_after");
        eraseStoryFlag(stageClearFlagForStage(currentStageId_));
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, false)) {
            placePlayerAtBossApproach();
            baseStatus_.clear();
            logInfo("Debug: launch mode set to final boss approach.");
        }
        return true;
    }

    if (normalized == "game launch-mode final-boss-after") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        eraseStoryFlag("story_stage_03_boss_after");
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            beginBossDefeatSequence(player_.position);
            logInfo("Debug: launch mode set to final boss defeated event.");
        }
        return true;
    }

    if (normalized == "game launch-mode ending-kamishibai" ||
        normalized == "game launch-mode ending-paper") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        startEndingKamishibai();
        logInfo("Debug: launch mode set to ending kamishibai.");
        return true;
    }

    if (normalized == "game launch-mode post-ending-base" ||
        normalized == "game launch-mode ending-base") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        addStoryFlag(std::string(DebugEndingSeenFlag));
        addStoryFlag(std::string(DebugEndingMainFlag));
        addStoryFlag(std::string(DebugStage03ClearFlag));
        addStoryFlag(stageClearFlagForStage(currentStageId_));
        eraseStoryFlag(DebugPostEndingIntroFlag);
        placeBasePlayerAtMineExitReturnPoint();
        enterDebugBase();
        queueStoryEventForTrigger("post_ending:intro");
        logInfo("Debug: launch mode set to post-ending base.");
        return true;
    }

    if (normalized == "game enemy-test") {
        enterEnemyTestMode();
        logInfo("Debug: enemy test mode started.");
        return true;
    }

    if (normalized == "game save") {
        std::string message;
        if (saveSaveData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
        return true;
    }

    if (normalized == "game money add 10000") {
        money_ = std::max(0, money_ + 10000);
        logInfo("Debug: money +10000 => " + std::to_string(money_) + "G");
        return true;
    }

    if (normalized == "game materials add 100") {
        for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
            inventory_.addMaterial(static_cast<MaterialType>(index), 100);
        }
        logInfo("Debug: all upgrade materials +100.");
        return true;
    }

    if (normalized == "game items random8") {
        const std::vector<ItemData>& objects = objectCatalog_.registry.items();
        std::vector<std::string_view> candidateIds;
        candidateIds.reserve(objects.size());
        for (const ItemData& object : objects) {
            if (!object.id.empty()) {
                candidateIds.push_back(object.id);
            }
        }

        if (candidateIds.empty()) {
            logWarning("Debug: no object entries available; random item add skipped.");
            return true;
        }

        std::mt19937& rng = lootRuntimeRng();
        std::uniform_int_distribution<std::size_t> pick(0, candidateIds.size() - 1);
        int acquiredCount = 0;
        int skippedCount = 0;
        for (int i = 0; i < 8; ++i) {
            const std::string_view objectId = candidateIds[pick(rng)];
            if (inventory_.addObjectItem(objectCatalog_, objectId)) {
                ++acquiredCount;
            } else {
                ++skippedCount;
            }
        }

        logInfo("Debug: random object items added " + std::to_string(acquiredCount) +
            " / skipped " + std::to_string(skippedCount) + ".");
        return true;
    }

    if (normalized == "game hp full") {
        applyPermanentUpgrades();
        player_.hp = player_.maxHp;
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        player_.hotDamageAccumulator = 0.0;
        player_.bleedDamageAccumulator = 0.0;
        logInfo("Debug: player HP restored to " + std::to_string(player_.maxHp) + ".");
        return true;
    }

    if (normalized == "game hp set 1") {
        applyPermanentUpgrades();
        player_.hp = std::min(1, std::max(1, player_.maxHp));
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        player_.hotDamageAccumulator = 0.0;
        player_.bleedDamageAccumulator = 0.0;
        logInfo("Debug: player HP set to 1.");
        return true;
    }

    if (normalized == "game level-up" || normalized == "game levelup") {
        if (basePresentationActive()) {
            if (levels_.isChoosing()) {
                levels_ = LevelSystem{};
            }
            levelUpResultDialog_ = {};
            logWarning("Debug: level-up requires an active dungeon run.");
            return true;
        }
        if (levels_.isChoosing() || levelUpResultDialog_.open) {
            logWarning("Debug: level-up choice is already active.");
            return true;
        }
        const bool dungeonRunMode =
            mode_ == ScreenMode::Playing ||
            (pauseReturnMode_ == ScreenMode::Playing &&
                (mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring));
        if (!dungeonRunMode) {
            logWarning("Debug: level-up requires an active dungeon run.");
            return true;
        }

        if (playerAtMaxLevel(player_)) {
            player_.level = PlayerMaxLevel;
            player_.xp = 0;
            player_.xpToNext = 0;
            logWarning("Debug: player level is already max.");
            return true;
        }

        player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
        const int xpNeeded = std::max(1, player_.xpToNext - player_.xp);
        const LevelGainResult result = gainPlayerXp(xpNeeded);
        if (result.levelsGained <= 0) {
            logWarning("Debug: level-up did not change player level.");
            return true;
        }
        inventory_.setOpen(false);
        inventoryReturnToPause_ = false;
        levelUpResultDialog_ = {};
        mode_ = ScreenMode::LevelUp;
        logInfo("Debug: forced level up to Lv " + std::to_string(player_.level) + ".");
        return true;
    }

    return false;
}

void Game::renderBaseDebugOverlay(Renderer& renderer, const Time& time) const
{
    if (!debug_.visible() || !basePresentationActive()) {
        return;
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }

    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    char debugBuffer[768];
    std::snprintf(debugBuffer, sizeof(debugBuffer),
        "FPS: %03d   Auto reload block: %s\n"
        "Base: area %s   mode %s\n"
        "Player: pos %.1f, %.1f\n"
        "Nearest: %s   interact %s\n"
        "Hovered: %s\n"
        "ReturnMode: %s",
        static_cast<int>(time.fps()),
        autoReloadBlocked_ ? "ON" : "OFF",
        baseAreaName(baseArea_),
        baseEditEnabled_ ? "edit" : "normal",
        basePlayerPosition_.x,
        basePlayerPosition_.y,
        nearest != nullptr ? nearest->displayName : "-",
        nearest != nullptr ? "true" : "false",
        hovered != nullptr ? hovered->displayName : "-",
        screenModeName(pauseReturnMode_));

    renderer.setScreenSpace();
    constexpr Vec2 PanelPos{10.0f, 10.0f};
    constexpr float PanelWidth = 570.0f;
    constexpr float PanelPadding = 10.0f;
    constexpr int TextScale = 2;
    constexpr float MinPanelHeight = 40.0f;
    const float textWidth = PanelWidth - PanelPadding * 2.0f;
    const Vec2 textSize = renderer.measureWrappedText(debugBuffer, textWidth, TextScale);
    const float panelHeight = std::max(MinPanelHeight, textSize.y + PanelPadding * 2.0f);
    renderer.fillRect(PanelPos, {PanelWidth, panelHeight}, {0, 0, 0, 125});
    renderer.drawWrappedText(
        PanelPos + Vec2{PanelPadding, PanelPadding},
        debugBuffer,
        textWidth,
        {220, 244, 224, 255},
        TextScale);
}

void Game::renderDebugOverlay(Renderer& renderer, const Time& time)
{
    const int nearestWarp = nearestWarpPointIndex(player_.position);
    bool nearestWarpDiscovered = false;
    for (const WarpPoint& point : warpPoints_) {
        if (point.index == nearestWarp) {
            nearestWarpDiscovered = point.discovered;
            break;
        }
    }

    debug_.render(
        renderer,
        time,
        enemies_,
        tileMap_,
        spellRing_,
        player_,
        balance_,
        dungeonLayout_,
        currentStageDefinition(),
        nearestWarp,
        nearestWarpDiscovered,
        discoveredWarpPointCount(),
        rewardNodeCount(),
        moneyNodeCount(),
        buriedVisibleNodeCount(),
        buriedHiddenNodeCount(),
        exposedEnemyNodeCount(),
        buriedEnemyNodeCount(),
        spawnedEnemyNodeCount(),
        autoReloadBlocked_);
}

} // namespace majo
