#pragma once

#include <Geode/loader/Mod.hpp>
#include "CachedSettings.def"

#define DECL_SETTING(NAME, TYPE, SETTING) \
    TYPE NAME = geode::Mod::get()->getSettingValue<TYPE>(SETTING);

namespace rl {

/// Holds all the settings in the mod itself.
struct CachedSettingsBase {
    RL_SAVED_SETTINGS(DECL_SETTING)
public:
    void reload() {
        *this = CachedSettingsBase{};
    }
};

/// Holds all the settings in the mod, as well as some for the running program.
struct CachedSettings : public CachedSettingsBase {
    bool isBadgifyLoaded = false;

public:
    using CachedSettingsBase::reload;

    static CachedSettings* get() {
        static CachedSettings settings;
        return &settings;
    }
};

}  // namespace rl

#undef DECL_SETTING
