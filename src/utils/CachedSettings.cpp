#include "CachedSettings.hpp"
#include "utils/StartupFunctions.hpp"
#include <Geode/loader/ModEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/binding/GJAccountManager.hpp>

using namespace geode::prelude;
using namespace rl;

void rl::CachedSettings_init() {
    Mod* const mod = Mod::get();
    listenForAllSettingChanges(
        [](std::string_view key, std::shared_ptr<SettingV3> setting) {
            CachedSettings::get()->reload();
            log::info("Reloaded cached settings");
        },
        mod);
    // Load cached player data.
    auto* CS = CachedSettings::get();
    if (mod->hasSavedValue(Keys::USER_INFO)) {
        const int currId = GJAccountManager::get()->m_accountID;
        if (currId == mod->getSavedValue<int>(Keys::USER_INFO_ID)) {
            auto info = mod->getSavedValue<RLUserInfo>(Keys::USER_INFO);
            info.accountId = currId;
            CS->userData = info;
        }
    }
}

$on_game(Exiting) {
    RLUserInfo& userInfo = CachedSettings::get()->userData;
    if (userInfo.accountId > 0) {
        Mod::get()->setSavedValue(Keys::USER_INFO, userInfo);
        Mod::get()->setSavedValue<int>(Keys::USER_INFO_ID, userInfo.accountId);
    } else {
        // TODO: Clear stuff on exit
    }
}
