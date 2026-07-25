#pragma once

#include <optional>
#include <Geode/ui/LazySprite.hpp>
#include "ccTypes.h"

namespace rl {
struct LazyNameplate {
    struct Opts {
        bool loadingCircle = true;
        bool autoResize = false;
        std::optional<cocos2d::CCPoint> position = std::nullopt;
    };
    static geode::LazySprite* create(cocos2d::CCSize size, int id, bool loadingCircle = true);
    static geode::LazySprite* create(cocos2d::CCSize size, int id, LazyNameplate::Opts const& opts);
};
}  // namespace rl
