#include "serdes/phy_types.hpp"

#include <array>

namespace serdes {
namespace {

constexpr std::array<ChannelProfile, 3> kProfiles{{
    {
        .name = "short",
        .impulse_response = {1.0, 0.24, -0.06, 0.02},
        .noise_sigma = 0.11,
        .pll_lock_delay_ticks = 4,
    },
    {
        .name = "medium",
        .impulse_response = {1.0, 0.62, -0.30, 0.14},
        .noise_sigma = 0.16,
        .pll_lock_delay_ticks = 7,
    },
    {
        .name = "long",
        .impulse_response = {1.0, 0.82, -0.48, 0.26},
        .noise_sigma = 0.20,
        .pll_lock_delay_ticks = 10,
    },
}};

}  // namespace

std::optional<ChannelProfile> find_channel_profile(const std::string_view name) {
    for (const auto& profile : kProfiles) {
        if (profile.name == name) {
            return profile;
        }
    }
    return std::nullopt;
}

}  // namespace serdes

