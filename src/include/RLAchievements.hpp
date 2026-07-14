#pragma once

#include <Geode/Geode.hpp>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define RL_DICTIONARY_ENABLED 0

namespace RLAchievements {
    enum class Collectable : int {
        Sparks  = 0,
        Planets = 1,
        Coins   = 2,
        Points  = 3,
        Votes   = 4,
        Misc    = 5,
    };

    struct Achievement {
        std::string_view id;
        std::string_view name;
        std::string_view desc;
        Collectable type;
        int amount;
        std::string_view sprite;
    };

    using AchievementList = std::span<const Achievement>;

    class AchievementData : public std::array<AchievementList, 6> {
        using BaseT = std::array<AchievementList, 6>;
        using BaseT::operator[];
    public:
        constexpr AchievementList operator[](Collectable C) const {
            const auto Ix = static_cast<std::size_t>(C);
            if (Ix > this->size()) return {};
            return (*this)[Ix];
        }
    };

    void init();
    void onUpdated(Collectable type, int oldVal, int newVal);  // call when an Eru value is updated
    void checkAll(Collectable type, int currentVal);           // check current totals and award any missing achievements
    void onReward(std::string const& id);                      // reward a misc achievement once by id (looks up name/desc/sprite)
    std::vector<Achievement> const& getAll();                  // get all achievements
    AchievementData const& getAllByType();                     // get achievements by type
    bool isAchieved(std::string const& id);                    // check if achievement is achieved

#if RL_DICTIONARY_ENABLED
    cocos2d::CCDictionary* getAllAsDictionary();
    cocos2d::CCDictionary* getAchievementDictionary(std::string const& id);  // individual achievement dictionary
#endif // RL_DICTIONARY_ENABLED
}  // namespace RLAchievements
