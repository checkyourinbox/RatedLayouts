#include "RLArgon.hpp"
#include <atomic>
#include <optional>
#include <Geode/Result.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/async.hpp>
#include <argon/argon.hpp>
#include <arc/task/Yield.hpp>
#include <arc/time/Sleep.hpp>
#include <asp/sync/SpinLock.hpp>
#include <asp/time/sleep.hpp>

using namespace geode::prelude;
using namespace rl;

static std::optional<argon::AccountData> ArgonData;
static std::string ArgonToken;
static async::TaskHolder<Result<std::string>> ArgonTask;
static asp::SpinLock LowContentionLock;

// TODO: Use memory_order?
static std::atomic<bool> DidFail = false;
static std::optional<std::string> FailureMessage;

void RLArgon::authorize(bool forceStrong) {
    ArgonTask.cancel();
    auto guard = LowContentionLock.lock();
    if (!ArgonToken.empty() && ArgonData && ArgonData->valid()) {
        log::info("Already authorized!");
        return;
    }
    if (!argon::signedIn()) {
        log::error("Auth failed, not signed in.");
        DidFail = true;
        FailureMessage = "Not signed in";
        return;
    }
    // Set up our info
    ArgonData = argon::getGameAccountData();
    argon::AuthOptions opts {
        .account = *ArgonData,
        .forceStrong = true,
    };
    ArgonTask.spawn(
        argon::startAuth(std::move(opts)),
        [](Result<std::string> res) {
            DidFail.store(res.isErr());
            if (res.isErr()) {
                argon::clearToken();
                FailureMessage = res.unwrapErr();
                auto err = res.unwrapErr();
                log::warn("Auth failed: {}", err);
                //Notification::create(err, NotificationIcon::Error)->show();
                RLArgon::clear();
                return;
            }
            log::info("Auth successful, got token: {}", res.unwrap());
            auto guard = LowContentionLock.lock();
            FailureMessage.reset();
            ArgonToken = std::move(res).unwrap();
        });
}

void RLArgon::wait() {
    if (!ArgonTask.isPending())
        return;
    asp::yield();
    int tries = 0;
    while (ArgonTask.isPending()) {
        ++tries;
        const auto waitTime = asp::Duration::fromMillis(tries * 250);
        if (waitTime.seconds() > 2) {
            log::error("ArgonTask failed to complete in time");
            return;
        }
        asp::sleep(waitTime);
    }
    log::info("ArgonTask completed in {} tries", tries);
}

arc::Future<> RLArgon::waitAsync() {
    if (!ArgonTask.isPending())
        co_return;
    co_await arc::yield();
    int tries = 0;
    while (ArgonTask.isPending()) {
        ++tries;
        const auto waitTime = asp::Duration::fromMillis(tries * 250);
        if (waitTime.seconds() > 2) {
            log::error("ArgonTask failed to complete in time");
            co_return;
        }
        co_await arc::sleepFor(waitTime);
    }
    log::info("ArgonTask completed in {} tries", tries);
}

void RLArgon::clear() {
    auto guard = LowContentionLock.lock();
    ArgonTask.cancel();
    ArgonData.reset();
    ArgonToken.clear();
}

std::string RLArgon::token() {
    auto guard = LowContentionLock.lock();
    return ArgonToken;
}

bool RLArgon::hasToken() {
    auto guard = LowContentionLock.lock();
    return !ArgonToken.empty();
}

bool RLArgon::failed() {
    return !DidFail.load();
}

bool RLArgon::notifyFailed() {
    if (!DidFail.load())
        return false;
    auto guard = LowContentionLock.lock();
    if (FailureMessage)
        Notification::create(*FailureMessage, NotificationIcon::Error)->show();
    else
        Notification::create("Argon validation failed...", NotificationIcon::Error)->show();
    return true;
}
