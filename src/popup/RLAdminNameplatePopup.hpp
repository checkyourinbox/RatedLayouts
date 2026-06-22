#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/ui/LoadingSpinner.hpp>

using namespace geode::prelude;

class RLAdminNameplatePopup : public Popup {
public:
    static RLAdminNameplatePopup* create();

private:
    bool init() override;
    void fetchPending();
    void onRefresh(CCObject* sender);
    void onView(CCObject* sender);
    void onApprove(CCObject* sender);
    void onReject(CCObject* sender);
    void onOpenProfile(CCObject* sender);

    cue::ListNode* m_listNode;
    LoadingSpinner* m_loadingCircle = nullptr;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_fetchTask;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_actionTask;
    
    struct PendingItem {
        int id;
        int accountId;
        std::string username;
        std::string path;
        int price;
        std::string createdAt;
    };
    std::vector<PendingItem> m_items;
};
