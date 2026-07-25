#pragma once

#include <span>
#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class RLLeaderboardLayer : public CCLayer {
protected:
    cue::ListNode* m_userListNode = nullptr;
    ScrollLayer* m_scrollLayer;
    LoadingSpinner* m_spinner;
    TabButton* m_starsTab;
    TabButton* m_planetsTab;
    TabButton* m_creatorTab;
    TabButton* m_coinsTab;
    TabButton* m_votesTab;
    CCMenuItemSpriteExtra* m_creatorTypeToggleBtn = nullptr;
    bool m_creatorType6 = false;
    CCMenuItemSpriteExtra* m_refreshBtn;
    CCMenuItemSpriteExtra* m_accountRefreshBtn;
    

    bool init() override;
    void keyBackClicked() override;
    void onLeaderboardTypeButton(CCObject* sender);
    void onCreatorTypeToggle(CCObject* sender);
    void onAccountClicked(CCObject* sender);
    void onAccountRefreshButton(CCObject* sender);
    void fetchLeaderboard(int type, int = 100);
    void populateLeaderboardStaggered(std::vector<matjson::Value> users, unsigned by = 10);
    bool populateLeaderboard(std::span<matjson::Value> users, int rank = 1);
    void onInfoButton(CCObject* sender);
    void onRefreshButton(CCObject* sender);

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_fetchTask;

private:
    template <bool ClearElts>
    inline bool populateLeaderboardImpl(std::span<matjson::Value> users, int rank = 1);

    void setUpdates(bool state) {
        if (m_userListNode) {
            //m_userListNode->setMouseEnabled(state);
            m_userListNode->setAutoUpdate(state);
            if (state)
                m_userListNode->updateLayout();
        }
    }

    geode::Function<void()> m_refreshFn;

public:
    static RLLeaderboardLayer* create();
};
