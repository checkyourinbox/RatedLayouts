#include "RLConstants.hpp"
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/RLData.hpp"
#include <Geode/DefaultInclude.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/modify/SupportLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/terminate.hpp>
#include <arc/future/Future.hpp>
#include <argon/argon.hpp>
#include <alphalaneous.badgify/include/Badgify.hpp>

using namespace geode::prelude;
using namespace rl;

// TODO: Check on random performance issues

static bool doesUserHaveBadge(RLBadgeKind K, RLUserInfo const& info) {
    using enum RLBadgeKind;
    switch (K) {
        case Supporter:
            return info.isSupporter;
        case Booster:
            return info.isBooster;
        case ClassicAdmin:
            return info.isClassicAdmin;
        case ClassicMod:
            return info.isClassicMod;
        case PlatAdmin:
            return info.isPlatAdmin;
        case PlatMod:
            return info.isPlatMod;
        case LeaderboardMod:
            return info.isLeaderboardMod;
        case Owner:
            return info.isOwner;
        case LeaderboardAdmin:
            return info.isLeaderboardAdmin;
        case Developer:
            return info.isDeveloper;
        default:
            return false;
    }
}

static void isUserInBadge(RLBadgeKind K, alpha::badgify::Badge badge) {
    const RLUserId accountId = badge.user->m_accountID;
    async::spawn(
        RLUserInfo::get(accountId),
        [K, badge = std::move(badge)](Result<RLUserInfo> info) {
            if (info.isErr()) [[unlikely]] {
                log::warn("Could not load user info: {}", info.unwrapErr());
                return;
            }
            if (!doesUserHaveBadge(K, info.unwrap()))
                return;
            // Show badge
            Loader::get()->queueInMainThread([K, badge = std::move(badge)]() {
                const char* spriteName = getBadgeInfo(K)->sprite;
                alpha::badgify::showBadge(badge, CCSprite::createWithSpriteFrameName(spriteName));
            });
        });
}

template <RLBadgeKind K>
static void badgeCallback(const alpha::badgify::Badge& badge) {
    using enum alpha::badgify::Location;
    if (badge.location == Profile || badge.location == Comment)
        isUserInBadge(K, badge);
}

template <RLBadgeKind K>
RL_NO_INLINE static void registerRLBadge() {
    constexpr const RLBadgeInfo* info = rl::getBadgeInfo(K);
    static_assert(info && info->isValid(), "Invalid badge type!");
    alpha::badgify::registerBadge(info->id, info->title, info->desc, &badgeCallback<K>);
}

// TODO: Add larger, high quality versions of each badge.
static void initRLBadges() {
    registerRLBadge<RLBadgeKind::Owner>();
    registerRLBadge<RLBadgeKind::Developer>();
    registerRLBadge<RLBadgeKind::ClassicAdmin>();
    registerRLBadge<RLBadgeKind::PlatAdmin>();
    registerRLBadge<RLBadgeKind::LeaderboardAdmin>();
    registerRLBadge<RLBadgeKind::ClassicMod>();
    registerRLBadge<RLBadgeKind::PlatMod>();
    registerRLBadge<RLBadgeKind::LeaderboardMod>();
    registerRLBadge<RLBadgeKind::Supporter>();
    registerRLBadge<RLBadgeKind::Booster>();
    log::info("Set up badges!");
    CachedSettings::get()->isBadgifyLoaded = true;
}

$on_mod(Loaded) {
    GJUserScore* userScore = GJUserScore::create();
    userScore->m_accountID = GJAccountManager::get()->m_accountID;
    if (userScore->m_modBadge != 0) {
        log::debug("User has mod badge: {}", userScore->m_modBadge);
        // TODO: Change this?
        Mod::get()->setSettingValue<bool>("disableRLReq", true);
    } else {
        log::debug("User does not have a mod badge");
    }

    // disable rl req if is a gdps
    if (rl::isGDPS()) {
        log::debug("Running on GDPS, disabling Rated Layouts requests");
        Mod::get()->setSettingValue<bool>("disableRLReq", true);
    }

    if (Loader::get()->getInstalledMod("alphalaneous.badgify"))
        // Should be loaded, buuuut just in case...
        alpha::badgify::waitForBadgify(&initRLBadges);
    else
        log::info("alphalaneous.badgify not installed, using defaults");
};

class $modify(RLSupportLayer, SupportLayer) {
    struct Fields {
        async::TaskHolder<web::WebResponse> m_getAccessTask;
        async::TaskHolder<Result<std::string>> m_authTask;
        CCMenu* m_argonMenu = nullptr;
        ~Fields() {
            m_getAccessTask.cancel();
            m_authTask.cancel();
        }
    };

    void customSetup() {
        if (!CachedSettings::get()->disableRLReq) {
            CCMenu* argonMenu = CCMenu::create();
            m_fields->m_argonMenu = argonMenu;
            argonMenu->setPosition({0, 0});
            m_listLayer->addChild(argonMenu, 10);
            // show the argon button for user with a role
            auto keyBtnSpr = ButtonSprite::create("Key", 25, true, "bigFont.fnt", "GJ_button_04.png", 25.f, 1.f);
            auto keyBtn = CCMenuItemSpriteExtra::create(
                keyBtnSpr, this, menu_selector(RLSupportLayer::onImportKey));
            keyBtn->setPosition({328, 55});
            argonMenu->addChild(keyBtn);

            if (rl::isUserHasPerms() || rl::isUserOwner() || rl::isUserDeveloper()) {
                auto argonBtnSpr = ButtonSprite::create("Argon", 25, true, "bigFont.fnt", "GJ_button_04.png", 25.f, 1.f);
                auto argonBtn = CCMenuItemSpriteExtra::create(
                    argonBtnSpr, this, menu_selector(RLSupportLayer::onGetArgon));
                argonBtn->setPosition({328, 85});
                argonMenu->addChild(argonBtn);
            }
        }

        SupportLayer::customSetup();
    }

    void onGetArgon(CCObject* sender) {
        m_uploadPopup = UploadActionPopup::create(nullptr, "Obtaining Token...");
        m_uploadPopup->show();
        auto accountData = argon::getGameAccountData();
        Ref<UploadActionPopup> popupRef = m_uploadPopup;
        m_fields->m_authTask.spawn(
            argon::startAuth(std::move(accountData)),
            [this, popupRef](Result<std::string> res) {
                if (!popupRef) return;
                if (res.isOk()) {
                    std::string token = std::move(res).unwrap();
                    utils::clipboard::write(token);
                    popupRef->showSuccessMessage(
                        "Token copied to clipboard.");
                } else {
                    std::string err = res.unwrapErr();
                    log::warn("Auth failed: {}", err);
                    popupRef->showFailMessage(err);
                    argon::clearToken();
                }
            });
    }

    void onImportKey(CCObject* sender) {
        m_uploadPopup = UploadActionPopup::create(nullptr, "Importing key...");
        m_uploadPopup->show();

        Ref<RLSupportLayer> self = this;
        Ref<UploadActionPopup> popupRef = m_uploadPopup;
        async::spawn([self, popupRef]() -> arc::Future<void> {
            if (!self || !popupRef) co_return;

            geode::utils::file::FilePickOptions options;
            options.filters.push_back({"Private Key", {"*.ppk", "*.pem"}});
            auto result = co_await geode::utils::file::pick(
                geode::utils::file::PickMode::OpenFile, options);

            if (!self) co_return;

            auto failMessage = [&] (gd::string message) {
                Loader::get()->queueInMainThread([self, popupRef, &message]() {
                    if (self && popupRef) {
                        popupRef->showFailMessage(std::move(message));
                    }
                });
            };

            if (!result) {
                failMessage("Failed to open file picker");
                co_return;
            }

            auto maybePath = result.unwrap();
            if (!maybePath) {
                failMessage("No file selected");
                co_return;
            }

            auto path = *maybePath;
            auto textRes = utils::file::readString(utils::string::pathToString(path));
            if (!textRes) {
                failMessage("Failed to read key file");
                co_return;
            }

            auto key = textRes.unwrap();
            if (key.empty()) {
                failMessage("Selected key file is empty");
                co_return;
            }

            geode::ByteVector keyBytes(key.begin(), key.end());
            web::MultipartForm form;
            form.file("privateKey", keyBytes, path.filename().string(), "application/octet-stream");

            auto postReq = web::WebRequest();
            postReq.bodyMultipart(form);

            auto response = co_await postReq.post(std::string(rl::BASE_API_URL) + "/masterKey");
            if (!self)
                co_return;

            if (!response.ok()) {
                std::string errorMsg = rl::getResponseFailMessage(response, "Failed to upload private key");
                failMessage(std::move(errorMsg));
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                if (auto optExtraCtx = jsonRes.err())
                    failMessage("Invalid server response: " + *optExtraCtx);
                else
                    failMessage("Invalid server response");
                co_return;
            }

            auto json = std::move(jsonRes).unwrap();
            std::string returnedKey = json["masterKey"].asString().unwrapOr("");
            if (returnedKey.empty()) {
                std::string errorMsg = json["message"].asString()
                                           .unwrapOr("Private key rejected or missing master key");
                failMessage(std::move(errorMsg));
                co_return;
            }

            Loader::get()->queueInMainThread([self, popupRef, returnedKey = std::move(returnedKey)]() {
                if (!self) return;
                Mod::get()->setSavedValue("masterKey", returnedKey);
                if (popupRef) {
                    popupRef->showSuccessMessage("Private key accepted.");
                    clipboard::write(returnedKey);
                }
            });
            co_return;
        });
    }

    // TODO: Handle cases if someone DOES get mod fsr?
    void onRequestAccess(CCObject* sender) {  // i assume that no one will ever get gd mod xddd
        if (CachedSettings::get()->disableRLReq) {
            SupportLayer::onRequestAccess(sender);
            return;
        }

        m_uploadPopup = UploadActionPopup::create(nullptr, "Loading...");
        m_uploadPopup->show();
        // argon my beloved <3
        auto accountData = argon::getGameAccountData();

        Ref<RLSupportLayer> self = this;
        Ref<UploadActionPopup> popupRef = m_uploadPopup;

        m_fields->m_authTask.spawn(
            argon::startAuth(std::move(accountData)),
            [self, this, popupRef](Result<std::string> res) {
                if (!self || !popupRef) return;

                if (res.isErr()) {
                    auto err = res.unwrapErr();
                    log::warn("Auth failed: {}", err);
                    Notification::create(err, NotificationIcon::Error)->show();
                    argon::clearToken();
                    return;
                }

                auto token = std::move(res).unwrap();
                log::info("token obtained: {}", token);
                Mod::get()->setSavedValue("argon_token", token);

                // json body
                matjson::Value jsonBody = matjson::Value::object();
                jsonBody["argonToken"] = token;
                jsonBody["accountId"] = GJAccountManager::get()->m_accountID;
                auto masterKey = Mod::get()->getSavedValue<std::string>("masterKey");
                if (!masterKey.empty()) {
                    jsonBody["masterKey"] = masterKey;
                }

                // verify the user's role
                auto postReq = web::WebRequest();
                postReq.bodyJSON(jsonBody);

                m_fields->m_getAccessTask.spawn(
                    postReq.post(std::string(rl::BASE_API_URL) + "/getAccess"),
                    [self, this, popupRef](web::WebResponse response) {
                        if (!self || !popupRef)
                            return;
                        log::info("Received response from server");
                        if (!response.ok()) {
                            log::warn("Server returned non-ok status: {}",
                                response.code());
                            popupRef->showFailMessage(rl::getResponseFailMessage(
                                response, "Failed! Try again later."));
                            return;
                        }

                        auto jsonRes = response.json();
                        if (!jsonRes) {
                            log::warn("Failed to parse JSON response");
                            popupRef->showFailMessage(
                                "Failed to parse JSON response");
                            return;
                        }

                        auto json = std::move(jsonRes).unwrap();
                        auto info = json.as<RLUserInfo>().unwrapOrDefault();
                        Mod::get()->setSavedValue<bool>("isClassicMod", info.isClassicMod);
                        Mod::get()->setSavedValue<bool>("isClassicAdmin", info.isClassicAdmin);
                        Mod::get()->setSavedValue<bool>("isLeaderboardMod", info.isLeaderboardMod);
                        Mod::get()->setSavedValue<bool>("isLeaderboardAdmin", info.isLeaderboardAdmin);
                        Mod::get()->setSavedValue<bool>("isPlatMod", info.isPlatMod);
                        Mod::get()->setSavedValue<bool>("isPlatAdmin", info.isPlatAdmin);
                        Mod::get()->setSavedValue<bool>("isOwner", info.isOwner);
                        Mod::get()->setSavedValue<bool>("isDeveloper", info.isDeveloper);

                        int roleCount = 0;
                        roleCount += info.isClassicMod ? 1 : 0;
                        roleCount += info.isClassicAdmin ? 1 : 0;
                        roleCount += info.isLeaderboardMod ? 1 : 0;
                        roleCount += info.isLeaderboardAdmin ? 1 : 0;
                        roleCount += info.isPlatMod ? 1 : 0;
                        roleCount += info.isPlatAdmin ? 1 : 0;
                        roleCount += info.isDeveloper ? 1 : 0;
                        roleCount += info.isOwner ? 1 : 0;

                        if (roleCount > 1) {
                            log::info("Granted Multiple roles");
                            popupRef->showSuccessMessage(
                                "Granted Layout roles.");
                        } else if (info.isClassicMod) {
                            log::info("Granted Layout Mod role");
                            popupRef->showSuccessMessage(
                                "Granted Classic Layout Mod.");
                        } else if (info.isClassicAdmin) {
                            log::info("Granted Layout Admin role");
                            popupRef->showSuccessMessage(
                                "Granted Classic Layout Admin.");
                        } else if (info.isLeaderboardMod) {
                            log::info("Granted Leaderboard Layout Mod role");
                            popupRef->showSuccessMessage(
                                "Granted Leaderboard Layout Mod.");
                        } else if (info.isLeaderboardAdmin) {
                            log::info("Granted Leaderboard Layout Admin role");
                            popupRef->showSuccessMessage(
                                "Granted Leaderboard Layout Admin.");
                        } else if (info.isPlatMod) {
                            log::info("Granted Platformer Layout Mod role");
                            popupRef->showSuccessMessage(
                                "Granted Platformer Layout Mod.");
                        } else if (info.isPlatAdmin) {
                            log::info("Granted Platformer Admin role");
                            popupRef->showSuccessMessage(
                                "Granted Platformer Layout Admin.");
                        } else if (info.isOwner) {
                            log::info("Granted Owner role");
                            popupRef->showSuccessMessage(
                                "Granted Owner role.");
                        } else if (info.isDeveloper) {
                            log::info("Granted Developer role");
                            popupRef->showSuccessMessage(
                                "Granted Developer role.");
                        } else {
                            popupRef->showFailMessage("Failed! Nothing found.");
                        }
                    });
            });
    }
};
