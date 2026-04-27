#include "game/UpgradeSystem.hpp"

namespace majo {

void UpgradeSystem::update(const Input& input, LevelSystem& level, SpellRingSystem& spellRing)
{
    if (!level.isChoosing()) {
        return;
    }
    if (input.upgradePressed(0)) {
        spellRing.upgradeRadius(1.1f);
        level.finishChoice();
    } else if (input.upgradePressed(1)) {
        spellRing.upgradeSpeed(1.1f);
        level.finishChoice();
    } else if (input.upgradePressed(2)) {
        spellRing.upgradeShovelPower(1);
        level.finishChoice();
    }
}

void UpgradeSystem::render(Renderer& renderer, const LevelSystem& level)
{
    if (!level.isChoosing()) {
        return;
    }
    renderer.setScreenSpace();
    renderer.fillRect({330.0f, 190.0f}, {620.0f, 270.0f}, {12, 10, 18, 232});
    renderer.drawRect({330.0f, 190.0f}, {620.0f, 270.0f}, {210, 184, 255, 255});
    renderer.drawText({392.0f, 225.0f}, "LEVEL UP", {246, 235, 255, 255}, 4);
    renderer.drawText({388.0f, 300.0f}, "1 SPELL RING RADIUS +10%", {230, 230, 230, 255}, 3);
    renderer.drawText({388.0f, 350.0f}, "2 SPELL RING SPEED +10%", {230, 230, 230, 255}, 3);
    renderer.drawText({388.0f, 400.0f}, "3 SHOVEL POWER +1", {230, 230, 230, 255}, 3);
}

}
