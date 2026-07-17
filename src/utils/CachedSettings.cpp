#include "CachedSettings.hpp"
#include <Geode/loader/ModEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;
using namespace rl;

$on_mod(Loaded) {
    listenForAllSettingChanges(
        [](std::string_view key, std::shared_ptr<SettingV3> setting) {
            CachedSettings::get()->reload();
            log::info("Reloaded cached settings");
        },
        Mod::get());
}
