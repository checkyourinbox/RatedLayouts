#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <Geode/Result.hpp>
#include <arc/future/Future.hpp>
//#include <matjson.hpp>
#include "RLNetworkUtils.hpp"

namespace rl {
using RLUserId = int64_t;

/// Common interface for player info.
/// Used in `ProfilePage` and `CommentCell`.
struct RLUserInfo {
    RLUserId accountId = 0;
    int points = 0;
    int planets = 0;
    int stars = 0;
    int coins = 0;
    int votes = 0;
    int nameplate = 0;
    // Special flags
    bool isSupporter = false;
    bool isBooster = false;
    // New role flags returned from server
    bool isClassicMod = false;
    bool isClassicAdmin = false;
    bool isLeaderboardMod = false;
    bool isLeaderboardAdmin = false;
    bool isPlatMod = false;
    bool isPlatAdmin = false;
    bool isDeveloper = false;
    bool isOwner = false;

public:
    using Future = arc::Future<RLUserInfo>;
    using ResFuture = arc::Future<geode::Result<RLUserInfo>>;

    /// Gets an `RLUserInfo` instance.
    static ResFuture get(RLUserId id, bool useCache = true);
    RLUserInfo& clear() { return (*this = RLUserInfo{}); }
    RLUserInfo& init(const RLUserInfo& other) { return (*this = other); }
    RLUserInfo& init(RLUserInfo&& other) { return (*this = std::move(other)); }

    bool isEmptyUser() const {
        return stars == 0 && planets == 0 && !isSupporter && !isBooster &&
               !isClassicMod && !isClassicAdmin && !isLeaderboardMod && !isLeaderboardAdmin &&
               !isPlatMod && !isPlatAdmin && !isDeveloper && !isOwner;
    }
};
}  // namespace rl

template <>
struct matjson::Serialize<rl::RLUserInfo> {
    static geode::Result<rl::RLUserInfo> fromJson(const matjson::Value& value) {
        rl::RLUserInfo info;
        info.points = value["points"].asInt().unwrapOrDefault();
        info.stars = value["stars"].asInt().unwrapOrDefault();
        info.planets = value["planets"].asInt().unwrapOrDefault();
        info.nameplate = value["nameplate"].asInt().unwrapOrDefault();
        info.coins = value["coins"].asInt().unwrapOrDefault();
        info.votes = value["votes"].asInt().unwrapOrDefault();
        info.isSupporter = value["isSupporter"].asBool().unwrapOrDefault();
        info.isBooster = value["isBooster"].asBool().unwrapOrDefault();
        info.isClassicMod = value["isClassicMod"].asBool().unwrapOrDefault();
        info.isClassicAdmin = value["isClassicAdmin"].asBool().unwrapOrDefault();
        info.isLeaderboardMod = value["isLeaderboardMod"].asBool().unwrapOrDefault();
        info.isLeaderboardAdmin = value["isLeaderboardAdmin"].asBool().unwrapOrDefault();
        info.isPlatMod = value["isPlatMod"].asBool().unwrapOrDefault();
        info.isPlatAdmin = value["isPlatAdmin"].asBool().unwrapOrDefault();
        info.isDeveloper = value["isDeveloper"].asBool().unwrapOrDefault();
        info.isOwner = value["isOwner"].asBool().unwrapOrDefault();
        return geode::Ok(info);
    }
    static matjson::Value toJson(const rl::RLUserInfo& info) {
        return matjson::makeObject({{"points", info.points},
                                    {"stars", info.stars},
                                    {"planets", info.planets},
                                    {"nameplate", info.nameplate},
                                    {"coins", info.coins},
                                    {"votes", info.votes},
                                    {"isSupporter", info.isSupporter},
                                    {"isBooster", info.isBooster},
                                    {"isClassicMod", info.isClassicMod},
                                    {"isClassicAdmin", info.isClassicAdmin},
                                    {"isLeaderboardMod", info.isLeaderboardMod},
                                    {"isLeaderboardAdmin", info.isLeaderboardAdmin},
                                    {"isPlatMod", info.isPlatMod},
                                    {"isPlatAdmin", info.isPlatAdmin},
                                    {"isDeveloper", info.isDeveloper},
                                    {"isOwner", info.isOwner}});
    }
};
