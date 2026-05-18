#include "game/GameInternal.hpp"

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

struct DebugItemPickerLayout {
    UiRect panel{};
    UiRect grid{};
    UiRect detail{};
    int columns = 1;
    float rowHeight = DebugItemPickerCardHeight + DebugItemPickerCardGap;
};

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

UiRect debugItemPickerAddButtonRect(UiRect panel)
{
    return uiBottomRightButtonRect(panel, {180.0f, ui::ButtonHeight});
}

UiRect debugItemPickerCloseButtonRect(UiRect panel)
{
    return uiBottomLeftButtonRect(panel, {150.0f, ui::ButtonHeight});
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
    if (mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::Title || mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: item picker requires an active game screen.");
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }

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
    if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
        debugItemPickerStatus_ = "追加できません: " + itemName;
        reloadNotice_ = debugItemPickerStatus_;
        reloadNoticeTimer_ = 1.4f;
        logWarning("Debug: failed to add object_id=\"" + objectId + "\".");
        return;
    }

    debugItemPickerStatus_ = "追加: " + itemName;
    reloadNotice_ = "Debug add: " + inlineItemTag(objectId) + " " + itemName;
    reloadNoticeTimer_ = 1.6f;
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

bool Game::executeDebugCommand(std::string_view command)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(command)));

    if (handleBaseEditCommand(normalized)) {
        return true;
    }
    if (handleObjectImageScaleCommand(normalized)) {
        return true;
    }
    if (handleDebugItemPickerCommand(normalized)) {
        return true;
    }

    const auto applyStageUnlockDebugCommand = [&](int unlockedStoryStages, std::string_view label) {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::Title) {
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

    if (normalized == "game reset-data") {
        money_ = 0;
        maxHpUpgradeLevel_ = 0;
        ringRadiusUpgradeLevel_ = 0;
        ringSpeedUpgradeLevel_ = 0;
        collectionRangeUpgradeLevel_ = 0;
        levelRingRadiusPoints_ = 0;
        levelRingSpeedPoints_ = 0;
        levelRingWeightLimitPoints_ = 0;
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

    if (normalized == "game launch-mode pre-title" ||
        normalized == "game launch-mode before-title" ||
        normalized == "game launch-mode opening") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::Title) {
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
        } else if (basePresentationActive() || mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::Title) {
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
        logInfo("Debug: player HP restored to " + std::to_string(player_.maxHp) + ".");
        return true;
    }

    if (normalized == "game hp set 1") {
        applyPermanentUpgrades();
        player_.hp = std::min(1, std::max(1, player_.maxHp));
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
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

        player_.xpToNext = std::max(1, player_.xpToNext);
        const int xpNeeded = std::max(0, player_.xpToNext - player_.xp);
        levels_.addXp(player_, xpNeeded, balance_);
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
