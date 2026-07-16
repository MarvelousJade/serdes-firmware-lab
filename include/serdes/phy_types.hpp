#pragma once

#include "serdes/register_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace serdes {

using DfeTapCodes = std::array<std::int8_t, kDfeTapCount>;

struct Measurement {
    std::uint32_t errors{0};
    std::uint32_t symbols{0};
    double mean_squared_error{0.0};
    double mean_margin{0.0};
    std::array<std::int32_t, kDfeTapCount> error_correlations{};
    bool valid{false};

    [[nodiscard]] double ber() const noexcept {
        return symbols == 0U ? 1.0 : static_cast<double>(errors) / static_cast<double>(symbols);
    }
};

struct ChannelProfile {
    std::string_view name;
    std::array<double, 4> impulse_response;
    double noise_sigma;
    std::uint32_t pll_lock_delay_ticks;
    bool force_pll_fault{false};
};

[[nodiscard]] std::optional<ChannelProfile> find_channel_profile(std::string_view name);

}  // namespace serdes

