#pragma once

#include <string>
#include <arc/future/Future.hpp>
#include <arc/sync/Notify.hpp>
#include <Geode/Result.hpp>
#include <Geode/utils/async.hpp>
//#include <argon/argon.hpp>

namespace rl {

struct RLArgon {
    using ArgonTaskType = geode::async::TaskHolder<geode::Result<std::string>>;

    /// Handles authorization.
    /// @returns Notifies when validation is complete.
    static void authorize(bool forceStrong = false);

    /// Wait for auth to complete.
    static void wait();
    /// Wait for auth to complete asynchronously.
    static arc::Future<> waitAsync();
    /// Clear the cached token.
    static void clear();
    /// Gets the cached token.
    static std::string token();
    /// If the token is valid.
    static bool hasToken();

    /// If auth failed.
    static bool failed();
    /// Notifies if failure occurred.
    static bool notifyFailed();
};

}  // namespace rl
