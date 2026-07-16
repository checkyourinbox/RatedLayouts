#pragma once

#include <Geode/Geode.hpp>
#include <argon/argon.hpp>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string_view>
#include "RLConfig.hpp"
#include "RLConstants.hpp"

using namespace geode::prelude;

namespace rl {
    using JSONInitListType = std::initializer_list<std::pair<std::string, matjson::Value>>;

    inline std::string getAPIEndpoint(std::string_view dest) {
        return fmt::format("{}/{}", rl::BASE_API_URL, dest);
    }

    inline web::WebRequest createWebRequest(JSONInitListType entries) {
        web::WebRequest req;
        req.bodyJSON(matjson::makeObject(entries));
        return req;
    }
    inline web::WebRequest createWebRequest(matjson::Value const& json) {
        web::WebRequest req;
        req.bodyJSON(json);
        return req;
    }

    // Returns the server's response body as the error message when it is
    // non-empty, otherwise falls back to the provided fallback string.
    inline std::string getResponseFailMessage(web::WebResponse const& response,
                                              std::string const& fallback) {
        auto message = response.string().unwrapOrDefault();
        if (!message.empty())
            return message;
        return fallback;
    }

    std::string_view getBaseURL();

    inline bool isGDPS() {
        return getBaseURL() != "https://www.boomlings.com/database";
    }

    ////////////////////////////////////////////////////////////////////////////
    // Caches

    // TODO: Tbh this whole setup is clunky, refactor it all

    using RequestTimestamp = std::int64_t;

    inline RequestTimestamp getCurrentTimestamp() {
        // TODO: Look into using alternative to std::time
        return static_cast<RequestTimestamp>(std::time(nullptr));
    }

    inline std::filesystem::path getRequestCachePath() {
        return dirs::getModsSaveDir() / Mod::get()->getID() / "request_cache.json";
    }

    inline std::filesystem::path getNameplateCacheDir() {
        return dirs::getModsSaveDir() / Mod::get()->getID() / "nameplates";
    }

    inline std::filesystem::path getNameplateCachePath(int nameplateId) {
        return getNameplateCacheDir() / fmt::format("nameplate_{}.png", nameplateId);
    }

    inline std::int64_t getRequestCacheLifetimeSeconds() {
        if (!Mod::get()) return 360;
        return Mod::get()->getSettingValue<int>("requestCacheLifetime");
    }

    inline int getRequestCacheMaxItems() {
        if (!Mod::get()) return 128;
        return Mod::get()->getSettingValue<int>("requestCacheMaxItems");
    }

    RL_ALWAYS_INLINE bool isRequestCacheValid(RequestTimestamp timestamp, std::int64_t lifetime) {
        return (getCurrentTimestamp() - timestamp) < lifetime;
    }

    inline bool isRequestCacheValid(RequestTimestamp timestamp) {
        std::int64_t lifetime = getRequestCacheLifetimeSeconds();
        if (lifetime <= 0) return false;
        return isRequestCacheValid(timestamp, lifetime);
    }

    struct RequestCacheEntry {
        matjson::Value json;
        RequestTimestamp timestamp = 0;

    public:
        bool isValid() const {
            return isRequestCacheValid(timestamp);
        }
    };

    /// Checks if either cache is in use, and the cache path is valid.
    bool requestCacheExists();
    matjson::Value* loadRequestCacheRootLockfree();
    [[deprecated]] matjson::Value loadRequestCacheRoot();
    [[nodiscard]] bool saveRequestCacheRoot();
    /// Bypasses the cached data and stores to the cache file directly.
    [[deprecated]] bool saveRequestCacheRoot(matjson::Value const& root);

    bool saveNameplateCache(int nameplateId, std::string const& data);
    bool hasNameplateCache(int nameplateId);

    std::optional<RequestCacheEntry> loadRequestCacheEntry(std::string_view section, int id);
    void storeRequestCacheEntry(std::string_view section, int id, matjson::Value const& data);
    void removeRequestCacheEntry(std::string_view section, int id);

    void clearNameplateCache();
    void clearRequestCache();

    std::optional<matjson::Value> getCachedCommentRole(int accountId);
    std::optional<matjson::Value> getStaleCommentRole(int accountId);
    void setCachedCommentRole(int accountId, matjson::Value const& data);

    std::optional<matjson::Value> getCachedLevelRating(int levelId);
    std::optional<matjson::Value> getStaleLevelRating(int levelId);
    void setCachedLevelRating(int levelId, matjson::Value const& data);
    void removeCachedLevelRating(int levelId);
}  // namespace rl

template <>
struct matjson::Serialize<rl::RequestCacheEntry> {
    static geode::Result<rl::RequestCacheEntry> fromJson(const matjson::Value& value) {
        GEODE_UNWRAP_INTO(matjson::Value data, value.get("data"));
        if (!data.isObject()) [[unlikely]]
            return geode::Err("data is not an object!");
        GEODE_UNWRAP_INTO(rl::RequestTimestamp time, value["timestamp"].asInt());
        return geode::Ok(rl::RequestCacheEntry{std::move(data), time});
    }
    static matjson::Value toJson(const rl::RequestCacheEntry& entry) {
        return matjson::makeObject({{"data", entry.json},
                                    {"timestamp", entry.timestamp}});
    }
};
