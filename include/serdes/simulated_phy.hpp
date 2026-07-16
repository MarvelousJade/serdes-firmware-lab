#pragma once

#include "serdes/phy_types.hpp"
#include "serdes/prbs31.hpp"
#include "serdes/register_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace serdes {

class SimulatedPhy final : public IRegisterIo {
public:
    explicit SimulatedPhy(ChannelProfile profile);

    [[nodiscard]] std::uint32_t read32(Register address) const override;
    void write32(Register address, std::uint32_t value) override;
    void tick() override;

    void set_channel_profile(ChannelProfile profile);
    [[nodiscard]] const ChannelProfile& channel_profile() const noexcept { return profile_; }

private:
    struct SymbolObservation {
        bool bit_error{false};
        double squared_error{0.0};
        double signed_margin{0.0};
        std::array<std::int32_t, kDfeTapCount> correlation_signs{};
    };

    void reset_model();
    void restart_pattern(std::uint32_t seed);
    void run_measurement(bool training_mode);
    [[nodiscard]] SymbolObservation step_symbol(bool training_mode);
    [[nodiscard]] double gaussian_noise() noexcept;
    [[nodiscard]] double uniform01() noexcept;
    [[nodiscard]] static double ctle_feedback(std::uint8_t code) noexcept;
    [[nodiscard]] static std::int8_t clamp_tap_code(int value) noexcept;

    ChannelProfile profile_;
    Prbs31 prbs_;
    std::uint64_t noise_state_{0xA076'1D64'78BD'642FULL};
    double spare_gaussian_noise_{0.0};
    bool has_spare_gaussian_noise_{false};

    std::array<double, 4> symbol_history_{};
    std::array<double, kDfeTapCount> feedback_history_{};
    double previous_raw_sample_{0.0};
    bool warmup_required_{true};

    std::uint32_t ticks_since_reset_{0};
    bool pll_locked_{false};
    bool measurement_done_{false};
    bool measurement_fault_{false};

    std::uint8_t ctle_code_{0};
    DfeTapCodes dfe_tap_codes_{};
    std::uint32_t requested_symbols_{0};
    Measurement last_measurement_{};
};

}  // namespace serdes
