#include "RLAchievementCell.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

TableViewCell* RLAchievementCell(RLAchievements::Achievement const& ach, bool unlocked) {
    auto cellBase = TableViewCell::create();
    auto cell = static_cast<TableViewCell*>(cellBase);
    if (!cell) return nullptr;
    cell->setContentSize({345.f, 64.f});

    // left icon
    cocos2d::CCSprite* icon = nullptr;
    if (!ach.sprite.empty()) {
        const char* sprite = ach.sprite.data();
        icon = unlocked ? CCSprite::createWithSpriteFrameName(sprite) : CCSpriteGrayscale::createWithSpriteFrameName(sprite);
    }
    if (icon) {
        icon->setPosition({30.f, 32.f});
        float baseScale = 1.f;
        if (ach.type == RLAchievements::Collectable::Sparks || ach.type == RLAchievements::Collectable::Planets || ach.type == RLAchievements::Collectable::Coins) {
            baseScale = 0.6f;
        }

        auto iconSize = icon->getContentSize();
        if (iconSize.width > 0.f && iconSize.height > 0.f) {
            float maxSize = 30.f;
            float scaledWidth = iconSize.width * baseScale;
            float scaledHeight = iconSize.height * baseScale;
            float fitScale = 1.0f;
            if (scaledWidth > maxSize || scaledHeight > maxSize) {
                fitScale = maxSize / std::max(scaledWidth, scaledHeight);
            } else if (scaledWidth < maxSize && scaledHeight < maxSize) {
                fitScale = maxSize / std::max(scaledWidth, scaledHeight);
            }
            icon->setScale(baseScale * fitScale);
        } else {
            icon->setScale(baseScale);
        }

        cell->addChild(icon);
    }

    // title
    auto title = CCLabelBMFont::create(ach.name.data(), "goldFont.fnt");
    title->setScale(0.7f);
    title->limitLabelWidth(200.f, 0.6f, 0.45f);
    title->setAnchorPoint({0.f, 1.f});
    title->setPosition({60.f, 50.f});
    cell->addChild(title);

    // description
    auto desc = CCLabelBMFont::create(ach.desc.data(), "chatFont.fnt");
    desc->setScale(0.7f);
    desc->setAnchorPoint({0.f, 1.f});
    desc->setPosition({60.f, 25.f});
    desc->limitLabelWidth(200.f, 0.7f, 0.45f);
    cell->addChild(desc);

    // status icon (check or lock)
    const char* frameName = unlocked ? "GJ_completesIcon_001.png" : "GJ_lock_001.png";
    cocos2d::CCSprite* status = CCSprite::createWithSpriteFrameName(frameName);
    if (!status)
        status = CCSprite::create(frameName);
    if (status) {
        status->setPosition({300.f, 32.f});
        status->setScale(.8f);
        cell->addChild(status);
    } else {
        auto statLabel = CCLabelBMFont::create(unlocked ? "Unlocked" : "Locked", "bigFont.fnt");
        statLabel->setScale(0.45f);
        statLabel->setPosition({310.f, 32.f});
        cell->addChild(statLabel);
    }

    return cell;
}
