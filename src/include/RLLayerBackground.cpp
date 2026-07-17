#include "RLLayerBackground.hpp"
#include <Geode/Geode.hpp>
#include <cue/RepeatingBackground.hpp>
#include "utils/CachedSettings.hpp"

using namespace geode::prelude;
using namespace rl;

void rl::addLayerBackground(cocos2d::CCNode* parent) {
    auto color = CachedSettings::get()->rgbBackground;

    if (CachedSettings::get()->disableBackground) {
        auto bg = createLayerBG();
        bg->setColor(color);
        parent->addChild(bg, -1);
        return;
    }

    auto value = CachedSettings::get()->backgroundType;
    std::string bgIndex = (value >= 1 && value <= 9)
                              ? ("0" + numToString(value))
                              : numToString(value);
    std::string bgName = "game_bg_" + bgIndex + "_001.png";
    auto bg = cue::RepeatingBackground::create(bgName.c_str(), 1.f, cue::RepeatMode::X);
    bg->setColor(color);
    parent->addChild(bg, -1);
}
