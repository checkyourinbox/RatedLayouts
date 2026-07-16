#include "RLData.hpp"
#include <optional>
#include <string>
#include <unordered_map>
//#include <Geode/Geode.hpp>
#include <Geode/loader/Mod.hpp>
//#include <argon/argon.hpp>
//#include <arc/prelude.hpp>
#include <arc/sync/Mutex.hpp>
#include "utils/NoHashHasher.hpp"

using namespace geode::prelude;
using namespace rl;

namespace {
enum { kDefaultCachePruningSize = 3072 };
struct UserCacheEntry {
    RequestTimestamp timestamp = 0;
    RLUserInfo info;
};
}  // namespace

using IdHasher = NoHashHasher<RLUserId>;
using UserCacheType = std::unordered_map<RLUserId, UserCacheEntry, IdHasher>;

static arc::Mutex<UserCacheType> UserCache;

static bool cacheMapNeedsPruning(size_t cacheSize, int64_t& maxItems) {
    if (maxItems <= 0)
        maxItems = kDefaultCachePruningSize;
    // Allow some extra entries to avoid constantly clearing cache (~1.75x).
    const size_t cacheMaxWithTolerance = size_t((maxItems * 7) / 4);
    return cacheSize > cacheMaxWithTolerance;
}

static void pruneCacheMap(UserCacheType& cache) {
    int64_t maxItems = getRequestCacheMaxItems();
    if (!cacheMapNeedsPruning(cache.size(), maxItems)) return;

    std::vector<std::pair<int, std::time_t>> entries;
    entries.reserve(cache.size());
    for (auto const& [id, entry] : cache) {
        entries.emplace_back(id, entry.timestamp);
    }
    std::sort(entries.begin(), entries.end(),
    [](auto const& a, auto const& b) {
        return a.second < b.second;
    });

    const size_t removeCount = cache.size() - size_t(maxItems);
    for (size_t i = 0; i < removeCount; ++i) {
        cache.erase(entries[i].first);
    }
    log::debug("Pruned in-memory user cache");
}

static const char* jsonTypeToString(matjson::Type const& type) {
    switch (type) {
        case matjson::Type::Object: return "object";
        case matjson::Type::Array: return "array";
        case matjson::Type::Bool: return "boolean";
        case matjson::Type::Number: return "number";
        case matjson::Type::String: return "string";
        case matjson::Type::Null: return "null";
        default: return "unknown";
    }
}

static Result<std::string> getArgonToken() {
    if (geode::Mod* mod = Mod::get()) {
        // TODO: Add global settings cache
        if (!mod->hasSavedValue("argon_token"))
            return Err("Argon token not stored");
        return Ok(mod->getSavedValue<std::string>("argon_token"));
    }
    return Err("Mod not loaded");
}

static Result<matjson::Value> parseServerResponse(web::WebResponse const& response) {
    auto asJson = response.json();
    if (!asJson) {
        return Err(fmt::format("Response was not valid JSON: {}", asJson.unwrapErr()));
    }
    auto json = std::move(asJson).unwrap();
    if (!json.isObject()) {
        return Err(fmt::format("Expected object, got {}", jsonTypeToString(json.type())));
    }
    return Ok(std::move(json));
}

static std::string parseServerError(web::WebResponse const& error, RLUserId id) {
    if (error.code() == 404)
        return fmt::format("User info for '{}' not found on server", id);
    return fmt::format("User info for '{}' failed with code {}", id, error.code());
}

static RLUserInfo::ResFuture getUserInfoFromWeb(RLUserId id) {
    ARC_FRAME();
    auto token = getArgonToken();
    if (token.isErr()) {
        log::warn("Argon token missing, aborting role fetch for {}", id);
        co_return Err(std::move(token).unwrapErr());
    }

    auto response = co_await rl::createWebRequest({{"accountId", id},
                                                   {"argonToken", std::move(token).unwrap()}})
                        .post(rl::getAPIEndpoint("profile"));

    log::info("Received response from server");
    if (!response.ok())
        co_return Err(parseServerError(response, id));

    // Get the actual json response.
    matjson::Value data = ARC_CO_UNWRAP(parseServerResponse(response));
    RLUserInfo out = ARC_CO_UNWRAP(data.as<RLUserInfo>());
    out.accountId = id;
    co_return Ok(std::move(out));
}

RLUserInfo::ResFuture RLUserInfo::get(RLUserId id, bool useCache) {
    ARC_FRAME();
    if (!useCache) {
        co_return co_await getUserInfoFromWeb(id);
    }

    /*Initial check*/ {
        auto cache = co_await UserCache.lock();
        if (cache->contains(id)) {
            auto [timestamp, info] = cache->at(id);
            if (rl::isRequestCacheValid(timestamp))
                co_return Ok(info);
            cache->erase(id);
        }
    }

    RLUserInfo newInfo = ARC_CO_UNWRAP(co_await getUserInfoFromWeb(id));
    /*Insertion*/ {
        auto cache = co_await UserCache.lock();
        if (!cache->contains(id)) {
            pruneCacheMap(*cache);
            cache->emplace(id, UserCacheEntry{getCurrentTimestamp(), newInfo});
        }
    }

    co_return Ok(newInfo);
}
