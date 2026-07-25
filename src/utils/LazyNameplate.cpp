#include "LazyNameplate.hpp"
#include <string_view>
#include <arc/future/Future.hpp>
#include "RLNetworkUtils.hpp"
#include "utils/RLData.hpp"

using namespace geode::prelude;
using namespace rl;

// TODO: Switch format over to CCImage::kFmtWebp
static constexpr bool kUsingWebp = false;
static constexpr LazySprite::Format kFormat = kUsingWebp ? CCImage::kFmtWebp : CCImage::kFmtPng;
static constexpr std::string_view kFormatExt = kUsingWebp ? "webp" : "png";

static std::string getNameplateURL(int nameplateId) {
    return fmt::format("{}/nameplates/banner/nameplate_{}.{}", rl::BASE_API_URL, nameplateId, kFormatExt);
}

std::filesystem::path rl::getNameplateCachePath(int nameplateId) {
    return rl::getNameplateCacheDir() / fmt::format("nameplate_{}.{}", nameplateId, kFormatExt);
}

static LazySprite* initLazyNameplate(LazySprite* lazy, int id) {
    if (rl::hasNameplateCache(id)) {
        async::spawn([lazy = Ref(lazy), id] () -> arc::Future<> {
            auto cachePath = rl::getNameplateCachePath(id);
            lazy->loadFromFile(cachePath, kFormat, true);
            co_return;
        });
        return lazy;
    }
    // We need to load it from the source...
    async::spawn([lazy = WeakRef(lazy), id] () -> arc::Future<> {
        std::string url = getNameplateURL(id);
        auto response = co_await web::WebRequest().get(url);
        if (!response.ok()) {
            log::error("Failed to fetch nameplate '{}': {}", id, response.errorMessage());
            co_return;
        }
        if (auto body = response.string()) {
            Loader::get()->queueInMainThread([lazy, data = body.unwrap()] () {
                if (auto spr = lazy.lock()) {
                    auto* ptr = reinterpret_cast<const uint8_t*>(data.c_str());
                    spr->loadFromData(ptr, data.size(), kFormat);
                }
            });
            if (rl::saveNameplateCache(id, body.unwrap()))
                log::info("Saved nameplate '{}' to disk", id);
            else
                log::warn("Failed to save nameplate '{}' to disk", id);
        }
        co_return;
    });
    return lazy;
}

LazySprite* LazyNameplate::create(cocos2d::CCSize size, int id, LazyNameplate::Opts const& opts) {
    LazySprite* lazy = LazySprite::create(size, opts.loadingCircle);
    if (opts.autoResize)
        lazy->setAutoResize(true);
    if (auto pos = opts.position)
        lazy->setPosition(*pos);
    // Run the initialization logic.
    return initLazyNameplate(lazy, id);
}

LazySprite* LazyNameplate::create(cocos2d::CCSize size, int id, bool loadingCircle) {
    LazySprite* lazy = LazySprite::create(size, loadingCircle);
    // Run the initialization logic.
    return initLazyNameplate(lazy, id);
}
