#include "RLData.hpp"
#include <optional>
#include <string>
#include <unordered_map>
//#include <Geode/Geode.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/ModEvent.hpp>
#include <Geode/utils/file.hpp>
//#include <argon/argon.hpp>
//#include <arc/prelude.hpp>
#include <arc/sync/Mutex.hpp>
//#include <asp/time.hpp>
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/NoHashHasher.hpp"
#include "utils/RLArgon.hpp"
#include "utils/StartupFunctions.hpp"

using namespace geode::prelude;
using namespace rl;

// TODO: Merge timestamped entries into a single utility
// TODO: Merge async caches into one class

namespace {
enum { kDefaultCachePruningSize = 2048, kLocalEndpointRequestLifetime = 120 };
// TODO: Merge this with timestamp?
struct UserCacheEntry {
    RequestTimestamp timestamp = 0;
    RLUserInfo info;
};
}  // namespace

template <>
struct matjson::Serialize<UserCacheEntry> {
    static Result<UserCacheEntry> fromJson(const matjson::Value& value) {
        UserCacheEntry entry{};
        GEODE_UNWRAP_INTO(const RLUserId ID, value["accountId"].asInt());
        GEODE_UNWRAP_INTO(entry.timestamp, value["timestamp"].asInt());
        GEODE_UNWRAP_INTO(entry.info, value.as<RLUserInfo>());
        entry.info.accountId = ID;
        return geode::Ok(entry);
    }
    static matjson::Value toJson(const UserCacheEntry& entry) {
        matjson::Value out;
        auto save = [&out](std::string_view key, auto value) {
            if (value) out[key] = value;
        };
        // Make the cache smaller...
        save("points", entry.info.points);
        save("stars", entry.info.stars);
        save("planets", entry.info.planets);
        save("nameplate", entry.info.nameplate);
        save("coins", entry.info.coins);
        save("votes", entry.info.votes);
        save("isSupporter", entry.info.isSupporter);
        save("isBooster", entry.info.isBooster);
        save("isClassicMod", entry.info.isClassicMod);
        save("isClassicAdmin", entry.info.isClassicAdmin);
        save("isLeaderboardMod", entry.info.isLeaderboardMod);
        save("isLeaderboardAdmin", entry.info.isLeaderboardAdmin);
        save("isPlatMod", entry.info.isPlatMod);
        save("isPlatAdmin", entry.info.isPlatAdmin);
        save("isDeveloper", entry.info.isDeveloper);
        save("isOwner", entry.info.isOwner);
        out["accountId"] = entry.info.accountId;
        out["timestamp"] = entry.timestamp;
        return out;
    }
};

using IdHasher = NoHashHasher<RLUserId>;
using UserCacheType = std::unordered_map<RLUserId, UserCacheEntry, IdHasher>;
using LocalEndpointCacheType = std::unordered_map<std::string, RequestCacheEntry>;

static arc::Mutex<UserCacheType> UserCache;
static arc::Mutex<LocalEndpointCacheType> LocalEndpointCache;

static RL_ALWAYS_INLINE bool isStale(RequestTimestamp timestamp) { return rl::isRequestCacheValid(timestamp); }

static bool cacheMapNeedsPruning(size_t cacheSize, int64_t& maxItems) {
    if (maxItems <= 0) maxItems = kDefaultCachePruningSize;
    // Allow some extra entries to avoid constantly clearing cache (~1.75x).
    const size_t cacheMaxWithTolerance = size_t((maxItems * 7) / 4);
    return cacheSize > cacheMaxWithTolerance;
}

static void pruneCacheMap(UserCacheType& cache) {
    //int64_t maxItems = getRequestCacheMaxItems();
    int64_t maxItems = kDefaultCachePruningSize;
    if (!cacheMapNeedsPruning(cache.size(), maxItems)) return;

    std::vector<std::pair<int, std::time_t>> entries;
    entries.reserve(cache.size());
    for (auto const& [id, entry] : cache) {
        entries.emplace_back(id, entry.timestamp);
    }
    std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) { return a.second < b.second; });

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
    if (!RLArgon::hasToken()) {
        //RLArgon::wait();
        if (RLArgon::failed())
            return Err("Argon token not stored");
    }
    return Ok(RLArgon::token());
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
    if (error.code() == 404) return fmt::format("User info for '{}' not found on server", id);
    return fmt::format("User info for '{}' failed with code {}", id, error.code());
}

static RLUserInfo::ResFuture getUserInfoFromWeb(RLUserId id) {
    ARC_FRAME();
    auto token = getArgonToken();
    if (token.isErr()) {
        log::warn("Argon token missing, aborting role fetch for {}", id);
        co_return Err(std::move(token).unwrapErr());
    }

    auto response = co_await rl::createWebRequest({{"accountId", id}, {"argonToken", std::move(token).unwrap()}})
                        .post(rl::getAPIEndpoint("profile"));

    log::info("Received response from server");
    if (!response.ok()) co_return Err(parseServerError(response, id));

    // Get the actual json response.
    matjson::Value data = ARC_CO_UNWRAP(parseServerResponse(response));
    RLUserInfo out = ARC_CO_UNWRAP(data.as<RLUserInfo>());
    out.accountId = id;
    co_return Ok(std::move(out));
}

template <bool AlwaysEmplace = false>
static RLUserInfo::ResFuture getNewEntry(RLUserId id) noexcept {
    ARC_FRAME();
    RLUserInfo newInfo = ARC_CO_UNWRAP(co_await getUserInfoFromWeb(id));
    auto cache = co_await UserCache.lock();
    if (!cache->contains(id)) {
        pruneCacheMap(*cache);
        cache->emplace(id, UserCacheEntry{getCurrentTimestamp(), newInfo});
    } else if constexpr (AlwaysEmplace) {
        UserCacheEntry& curr = cache->at(id);
        if (isStale(curr.timestamp)) curr = {getCurrentTimestamp(), newInfo};
    }
    co_return Ok(newInfo);
}

static void fetchStaleUserInfoInTheBackground(RLUserId id) {
    async::spawn([id]() -> arc::Future<> {
        log::debug("Fetching user info for stale request '{}'", id);
        auto res = co_await getNewEntry</*AlwaysEmplace=*/true>(id);
        if (res.isOk())
            log::info("Fetched for stale user info '{}'", id);
        else
            log::error("Failed to fetch for stale user info '{}': {}", id, res.unwrapErr());
        co_return;
    });
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
            if (isStale(timestamp)) fetchStaleUserInfoInTheBackground(id);
            co_return Ok(info);
        }
    }

    co_return co_await getNewEntry(id);
}

////////////////////////////////////////////////////////////////////////////////
// LocalEndpoint

namespace {
struct LocalEndpointData {
    std::string_view name;
    std::string endpoint;
    std::string key;
    std::optional<matjson::Value> body;

public:
    LocalEndpointData(std::string_view name);
    LocalEndpointData(std::string_view name, matjson::Value&& body);
};
}  // namespace

LocalEndpointData::LocalEndpointData(std::string_view name) : endpoint(rl::getAPIEndpoint(name)), key(endpoint) {
    this->name = name.substr(0, name.find('?'));
}

LocalEndpointData::LocalEndpointData(std::string_view name, matjson::Value&& body_) : LocalEndpointData(name) {
    this->body = std::move(body_);
    this->key += body->dump(0);
}

static bool isLocalEndpointStale(RequestTimestamp timestamp) {
    return rl::isRequestCacheValid(timestamp, kLocalEndpointRequestLifetime);
}

static web::WebRequest setupRemote(LocalEndpointData const& data, bool withAuth) {
    web::WebRequest req;
    auto body = data.body;
    if (withAuth) {
        if (!body) body = matjson::Value::object();
        (*body)["accountId"] = CachedSettings::get()->userData.accountId;
        (*body)["argonToken"] = RLArgon::token();
    }
    if (body) req.bodyJSON(*body);
    return req;
}

static LocalEndpoint::ResFuture getRemoteEndpointCommon(web::WebResponse response,
                                                        LocalEndpointData data,
                                                        bool alwaysEmplace = false) {
    // TODO: split on "?"
    if (!response.ok()) {
        log::warn("/{} returned non-ok status: {}", data.name, response.code());
        co_return Err(fmt::format("Failed to fetch from /{}", data.name));
    }

    auto jsonOrErr = response.json();
    if (!jsonOrErr) {
        log::warn("Failed to parse /{} JSON", data.name);
        co_return Err(fmt::format("Invalid server response", data.name));
    }

    auto json = std::move(jsonOrErr).unwrap();
    auto cache = co_await LocalEndpointCache.lock();
    if (!cache->contains(data.key)) {
        cache->emplace(data.key, RequestCacheEntry{json, getCurrentTimestamp()});
    } else if (alwaysEmplace) {
        RequestCacheEntry& curr = cache->at(data.key);
        if (isLocalEndpointStale(curr.timestamp)) curr = {json, getCurrentTimestamp()};
    }
    co_return Ok(std::move(json));
}

template <bool WithAuth>
static LocalEndpoint::ResFuture getRemoteEndpoint(LocalEndpointData data, bool alwaysEmplace = false) {
    ARC_FRAME();
    web::WebRequest req = setupRemote(data, WithAuth);
    auto response = co_await req.get(data.endpoint);
    co_return co_await getRemoteEndpointCommon(std::move(response), std::move(data), alwaysEmplace);
}

template <bool WithAuth>
static void fetchRemoteEndpointInTheBackground(LocalEndpointData data) {
    async::spawn([data = std::move(data)]() -> arc::Future<> {
        std::string_view name = data.name;
        log::debug("Fetching for stale request at /{}", data.name);
        auto res = co_await getRemoteEndpoint<WithAuth>(std::move(data), /*alwaysEmplace=*/true);
        if (res.isOk())
            log::info("Fetched for stale request at /{}", name);
        else
            log::error("Failed to fetch for stale request: {}", res.unwrapErr());
        co_return;
    });
}

template <bool WithAuth>
static LocalEndpoint::ResFuture getLocalEndpointCommon(LocalEndpointData data) {
    /*Initial check*/ {
        auto cache = co_await LocalEndpointCache.lock();
        if (cache->contains(data.key)) {
            auto [response, timestamp] = cache->at(data.key);
            if (isLocalEndpointStale(timestamp)) fetchRemoteEndpointInTheBackground<WithAuth>(std::move(data));
            co_return Ok(std::move(response));
        }
    }

    co_return co_await getRemoteEndpoint<WithAuth>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::get(std::string name) {
    ARC_FRAME();
    LocalEndpointData data(name);
    co_return co_await getLocalEndpointCommon</*WithAuth=*/false>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::get(std::string name, matjson::Value body) {
    ARC_FRAME();
    LocalEndpointData data(name, std::move(body));
    co_return co_await getLocalEndpointCommon</*WithAuth=*/false>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::getWithAuth(std::string name) {
    ARC_FRAME();
    LocalEndpointData data(name);
    co_return co_await getLocalEndpointCommon</*WithAuth=*/true>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::getWithAuth(std::string name, matjson::Value body) {
    ARC_FRAME();
    LocalEndpointData data(name, std::move(body));
    co_return co_await getLocalEndpointCommon</*WithAuth=*/true>(std::move(data));
}

////////////////////////////////////////////////////////////////////////////////
// Miscellaneous

bool rl::hasRLDataCache() {
    /*UserInfo*/ {
        auto cache = UserCache.blockingLock();
        if (!cache->empty())
            return true;
    }
    /*LocalEndpoint*/ {
        auto cache = LocalEndpointCache.blockingLock();
        if (!cache->empty())
            return true;
    }
    return false;
}

void rl::clearRLDataCache() {
    async::spawn([]() -> arc::Future<> {
        auto cache = co_await UserCache.lock();
        cache->clear();
        log::info("Cleared UserCache");
    });
    async::spawn([]() -> arc::Future<> {
        auto cache = co_await LocalEndpointCache.lock();
        cache->clear();
        log::info("Cleared LocalEndpointCache");
    });
}

static Result<matjson::Value> loadDataCacheRootFromFile() {
    auto path = getDataCachePath();
    auto existing = utils::file::readString(path);
    if (!existing)
        return Err(
            fmt::format("failed to read from \"{}\": {}", utils::string::pathToString(path), existing.unwrapErr()));
    GEODE_UNWRAP_INTO(matjson::Value root, matjson::parse(existing.unwrap()));
    if (!root.isObject()) return Err("root is not an object!");
    return Ok(std::move(root));
}

static void saveDataCacheRootToFile(matjson::Value const& root) {
    auto path = getDataCachePath();
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeStringSafe(path, root.dump(0));
    if (writeRes.isOk())
        log::info("Saved user info to file: {}", utils::string::pathToString(path));
    else
        log::error("Failed to save user info: {}", writeRes.unwrapErr());
}

static arc::Future<> loadUserInfo(std::vector<matjson::Value> info) {
    std::vector<UserCacheEntry> entries;
    entries.reserve(info.size());
    for (auto const& val : info) {
        auto entry = val.as<UserCacheEntry>();
        if (entry.isErr()) continue;
        entries.emplace_back(std::move(entry).unwrap());
    }
    /*Load the entries*/ {
        auto cache = co_await UserCache.lock();
        cache->reserve(entries.size());
        for (auto const& entry : entries) {
            const RLUserId accountId = entry.info.accountId;
            if (cache->contains(accountId)) continue;
            cache->emplace(accountId, entry);
        }
    }
    log::info("Loaded {} user info entries from file", entries.size());
    co_return;
}

static std::vector<matjson::Value> saveUserInfo() {
    std::vector<matjson::Value> out;
    auto cache = UserCache.blockingLock();
    out.reserve(cache->size());
    for (auto const& [_, entry] : *cache) out.emplace_back(entry);
    cache->clear();
    return out;
}

static arc::Future<> loadLocalEndpoint(matjson::Value info) {
    std::vector<std::pair<std::string, RequestCacheEntry>> entries;
    entries.reserve(info.size());
    for (auto const& [endpoint, val] : info) {
        auto entry = val.as<RequestCacheEntry>();
        if (entry.isErr()) continue;
        entries.emplace_back(endpoint, std::move(entry).unwrap());
    }
    /*Load the entries*/ {
        auto cache = co_await LocalEndpointCache.lock();
        cache->reserve(entries.size());
        for (auto& [endpoint, entry] : entries) {
            if (cache->contains(endpoint)) continue;
            cache->emplace(std::move(endpoint), std::move(entry));
        }
    }
    log::info("Loaded {} user info entries from file", entries.size());
    co_return;
}

static matjson::Value saveLocalEndpoint() {
    matjson::Value out;
    auto cache = LocalEndpointCache.blockingLock();
    for (auto const& [endpoint, entry] : *cache) out[endpoint] = entry;
    cache->clear();
    return out;
}

void rl::RLData_init() {
    auto rootOrErr = loadDataCacheRootFromFile();
    if (rootOrErr.isErr()) {
        log::warn("Failed to load cache data: {}", rootOrErr.unwrapErr());
        return;
    }

    matjson::Value root = std::move(rootOrErr).unwrap();
    log::info("Loading cached data");
    //log::debug("{}", root.dump(0));
    if (root.contains("userInfo")) {
        auto& data = root["userInfo"];
        if (auto arr = data.asArray())
            async::spawn(loadUserInfo(std::move(arr.unwrap())));
        else
            log::warn("\"userInfo\" expected array, got {}", jsonTypeToString(data.type()));
    }
    if (root.contains("localEndpoint")) {
        auto& data = root["localEndpoint"];
        if (data.isObject())
            async::spawn(loadLocalEndpoint(std::move(data)));
        else
            log::warn("\"localEndpoint\" expected object, got {}", jsonTypeToString(data.type()));
    }
}

$on_game(Exiting) {
    matjson::Value out;
    out["userInfo"] = saveUserInfo();
    out["localEndpoint"] = saveLocalEndpoint();
    saveDataCacheRootToFile(out);
}
