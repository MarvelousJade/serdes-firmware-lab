#include "serdes/firmware_controller.hpp"

#include "serdes/register_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace serdes {

std::string_view to_string(const LinkState state) noexcept {
    switch (state) {
    case LinkState::Idle:
        return "IDLE";
    case LinkState::Reset:
        return "RESET";
    case LinkState::WaitForPll:
        return "WAIT_FOR_PLL";
    case LinkState::CtleSweep:
        return "CTLE_SWEEP";
    case LinkState::DfeTraining:
        return "DFE_TRAINING";
    case LinkState::Verify:
        return "VERIFY";
    case LinkState::LinkUp:
        return "LINK_UP";
    case LinkState::Degraded:
        return "DEGRADED";
    case LinkState::Fault:
        return "FAULT";
    }
    return "UNKNOWN";
}

std::string_view to_string(const FaultReason reason) noexcept {
    switch (reason) {
    case FaultReason::None:
        return "none";
    case FaultReason::PllTimeout:
        return "pll_timeout";
    case FaultReason::MeasurementFault:
        return "measurement_fault";
    case FaultReason::BerTargetMissed:
        return "ber_target_missed";
    }
    return "unknown";
}

FirmwareController::FirmwareController(PhyDriver& phy, const FirmwareConfig config)
    : phy_(phy), config_(config) {}

BringupReport FirmwareController::bring_up(const std::uint32_t seed) {
    BringupReport report{};
    degraded_window_count_ = 0U;

    transition(LinkState::Reset, &report);
    phy_.reset();
    phy_.clear_dfe_taps();
    phy_.set_ctle_code(0U);

    transition(LinkState::WaitForPll, &report);
    if (!wait_for_pll()) {
        report.fault = FaultReason::PllTimeout;
        transition(LinkState::Fault, &report);
        return report;
    }

    phy_.restart_pattern(seed ^ 0x1357'2468U);
    report.baseline = phy_.measure(config_.verify_symbols, false);
    if (!report.baseline.valid) {
        report.fault = FaultReason::MeasurementFault;
        transition(LinkState::Fault, &report);
        return report;
    }

    transition(LinkState::CtleSweep, &report);
    Measurement best_measurement{};
    best_measurement.errors = std::numeric_limits<std::uint32_t>::max();
    best_measurement.mean_squared_error = std::numeric_limits<double>::infinity();
    std::uint8_t best_code = 0U;

    for (std::uint8_t code = 0U; code <= kMaxCtleCode; ++code) {
        phy_.set_ctle_code(code);
        phy_.clear_dfe_taps();
        phy_.restart_pattern(seed ^ 0x5A5A'1234U);
        const auto candidate = phy_.measure(config_.sweep_symbols, true);
        if (!candidate.valid) {
            report.fault = FaultReason::MeasurementFault;
            transition(LinkState::Fault, &report);
            return report;
        }

        const bool fewer_errors = candidate.errors < best_measurement.errors;
        const bool equal_errors_lower_mse =
            candidate.errors == best_measurement.errors &&
            candidate.mean_squared_error < best_measurement.mean_squared_error;
        if (fewer_errors || equal_errors_lower_mse) {
            best_measurement = candidate;
            best_code = code;
        }
    }

    report.selected_ctle_code = best_code;
    phy_.set_ctle_code(best_code);
    phy_.clear_dfe_taps();

    transition(LinkState::DfeTraining, &report);
    phy_.restart_pattern(seed ^ 0x2468'ACE1U);
    std::uint32_t stable_windows = 0U;
    for (std::uint32_t window = 0U; window < config_.max_training_windows; ++window) {
        const auto measurement =
            phy_.measure(config_.training_symbols_per_window, true);
        report.training_windows = window + 1U;
        if (!measurement.valid) {
            report.fault = FaultReason::MeasurementFault;
            transition(LinkState::Fault, &report);
            return report;
        }

        bool changed = false;
        for (std::size_t tap = 0; tap < kDfeTapCount; ++tap) {
            const double normalized = static_cast<double>(measurement.error_correlations[tap]) /
                                      static_cast<double>(measurement.symbols);
            if (std::abs(normalized) <= config_.correlation_deadband) {
                continue;
            }

            const int current = static_cast<int>(phy_.dfe_tap_code(tap));
            const int requested = current + adaptation_step(normalized);
            phy_.set_dfe_tap_code(tap, requested);
            changed = changed || static_cast<int>(phy_.dfe_tap_code(tap)) != current;
        }

        stable_windows = changed ? 0U : stable_windows + 1U;
        if (stable_windows >= config_.stable_training_windows) {
            break;
        }
    }

    report.trained_dfe_taps = phy_.dfe_tap_codes();
    transition(LinkState::Verify, &report);
    phy_.restart_pattern(seed ^ 0x6C8E'9CF5U);
    report.trained = phy_.measure(config_.verify_symbols, false);
    if (!report.trained.valid) {
        report.fault = FaultReason::MeasurementFault;
        transition(LinkState::Fault, &report);
        return report;
    }

    if (report.trained.ber() > config_.maximum_ber) {
        report.fault = FaultReason::BerTargetMissed;
        transition(LinkState::Fault, &report);
        return report;
    }

    report.success = true;
    transition(LinkState::LinkUp, &report);
    return report;
}

HealthAction FirmwareController::monitor_once(
    const std::uint32_t symbols,
    const std::uint32_t seed) {
    if (state_ != LinkState::LinkUp) {
        return HealthAction::NotLinkUp;
    }

    phy_.restart_pattern(seed);
    last_health_measurement_ = phy_.measure(symbols, false);
    if (!last_health_measurement_.valid) {
        transition(LinkState::Fault);
        return HealthAction::RetrainRequired;
    }

    if (last_health_measurement_.ber() <= config_.maximum_ber) {
        degraded_window_count_ = 0U;
        return HealthAction::Healthy;
    }

    ++degraded_window_count_;
    if (degraded_window_count_ < config_.degraded_windows_before_retrain) {
        return HealthAction::Observe;
    }

    transition(LinkState::Degraded);
    return HealthAction::RetrainRequired;
}

void FirmwareController::transition(const LinkState next, BringupReport* const report) {
    state_ = next;
    if (report != nullptr && report->state_trace_size < report->state_trace.size()) {
        report->state_trace[report->state_trace_size] = next;
        ++report->state_trace_size;
    }
}

bool FirmwareController::wait_for_pll() {
    for (std::uint32_t elapsed = 0U; elapsed < config_.pll_timeout_ticks; ++elapsed) {
        if (phy_.pll_locked()) {
            return true;
        }
        phy_.tick();
    }
    return phy_.pll_locked();
}

int FirmwareController::adaptation_step(const double normalized_correlation) noexcept {
    const double magnitude = std::abs(normalized_correlation);
    const int code_step = magnitude > 0.50 ? 3 : (magnitude > 0.20 ? 2 : 1);
    return normalized_correlation > 0.0 ? -code_step : code_step;
}

}  // namespace serdes

