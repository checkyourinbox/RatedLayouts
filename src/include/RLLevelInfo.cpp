#include "RLLevelInfo.hpp"

using namespace rl;

std::string const& rl::getEpicPString() {
    static const std::string epicPString =
        "30,2065,2,435,3,75,155,1,156,2,145,30a-1a2a0."
        "3a36a90a40a12a0a15a15a0a0a0a0a0a0a5a3a0a0a0.741176a0a0."
        "74902a0a1a0a1a0a3a1a0a0a0.258824a0a0.87451a0a1a0a1a0a0.3a0a0."
        "2a0a0a0a0a0a0a0a0a2a1a0a0a1a27a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    return epicPString;
}

std::string const& rl::getLegendaryPString() {
    static const std::string legendaryPString =
        "30,2065,2,345,3,75,155,1,156,2,145,30a-1a2a0."
        "3a13a90a40a10a0a15a15a0a0a0a0a0a0a6a3a0a0a0.313726a0a0."
        "615686a0a1a0a1a0a2a1a0a0a0.882353a0a0.878431a0a1a0a1a0a0.3a0a0."
        "2a0a0a0a0a0a0a0a0a2a1a0a0a1a138a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    return legendaryPString;
}
