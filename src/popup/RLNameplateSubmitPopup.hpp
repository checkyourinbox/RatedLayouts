#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>
#include "Geode/ui/Popup.hpp"
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

class RLNameplateSubmitPopup : public Popup {
public:
    static RLNameplateSubmitPopup* create();

private:
    bool init() override;
    void onPickImage(CCObject* sender);
    void onSubmit(CCObject* sender);
    void onViewPending(CCObject* sender);
    void onInfo(CCObject* sender);
    arc::Future<void> pickAndLoadPng();

    cue::ListNode* m_leaderboardListNode;
    LazySprite* m_nameplateLazy = nullptr;
    std::filesystem::path m_pickedPath;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_fetchTask;
    geode::TextInput* m_priceInput;
};