#pragma once

#include "serdes/phy_driver.hpp"
#include "serdes/phy_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace serdes {

enum class LinkState : std::uint8_t {
    Idle,
    Reset,
    WaitForPll,
    CtleSweep,
    DfeTraining,
    Verify,
    LinkUp,
    Degraded,
    Fault,
};

enum class FaultReason : std::uint8_t {
    None,
    PllTimeout,
    MeasurementFault,
    BerTargetMissed,
};

[[nodiscard]] std::string_view to_string(LinkState state) noexcept;
[[nodiscard]] std::string_view to_string(FaultReason reason) noexcept;

struct FirmwareConfig {
    std::uint32_t pll_timeout_ticks{32};
    std::uint32_t sweep_symbols{16'384};
    std::uint32_t training_symbols_per_window{4'096};
    std::uint32_t max_training_windows{64};
    std::uint32_t verify_symbols{200'000};
    double correlation_deadband{0.055};
    std::uint32_t stable_training_windows{3};
    double maximum_ber{1.0e-3};
    std::uint32_t degraded_windows_before_retrain{3};
};

struct BringupReport {
    static constexpr std::size_t kMaxTraceStates = 16;

    bool success{false};
    FaultReason fault{FaultReason::None};
    Measurement baseline{};
    Measurement trained{};
    std::uint8_t selected_ctle_code{0};
    DfeTapCodes trained_dfe_taps{};
    std::uint32_t training_windows{0};
    std::array<LinkState, kMaxTraceStates> state_trace{};
    std::size_t state_trace_size{0};
};

enum class HealthAction : std::uint8_t {
    Healthy,
    Observe,
    RetrainRequired,
    NotLinkUp,
};

class FirmwareController {
public:
    explicit FirmwareController(PhyDriver& phy, FirmwareConfig config = {});

    [[nodiscard]] BringupReport bring_up(std::uint32_t seed);
    [[nodiscard]] HealthAction monitor_once(std::uint32_t symbols, std::uint32_t seed);
    [[nodiscard]] LinkState state() const noexcept { return state_; }
    [[nodiscard]] const Measurement& last_health_measurement() const noexcept {
        return last_health_measurement_;
    }

private:
    void transition(LinkState next, BringupReport* report = nullptr);
    [[nodiscard]] bool wait_for_pll();
    [[nodiscard]] static int adaptation_step(double normalized_correlation) noexcept;

    PhyDriver& phy_;
    FirmwareConfig config_;
    LinkState state_{LinkState::Idle};
    std::uint32_t degraded_window_count_{0};
    Measurement last_health_measurement_{};
};

}  // namespace serdes

