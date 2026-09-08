#include "serdes/firmware_controller.hpp"

#include "serdes/register_map.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace serdes {
namespace {

// Each stage has a repeatable starting point. Every sweep candidate reuses kSweepSeedMask.
constexpr std::uint32_t kBaselineSeedMask = 0x1357'2468U;
constexpr std::uint32_t kSweepSeedMask = 0x5A5A'1234U;
constexpr std::uint32_t kTrainingSeedMask = 0x2468'ACE1U;
constexpr std::uint32_t kVerificationSeedMask = 0x6C8E'9CF5U;

}  // namespace

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
    case FaultReason::TrainingNotConverged:
        return "training_not_converged";
    case FaultReason::BerTargetMissed:
        return "ber_target_missed";
    }
    return "unknown";
}

FirmwareController::FirmwareController(PhyDriver& phy, const FirmwareConfig config)
    : phy_(phy), config_(config) {}

BringupReport FirmwareController::bring_up(const std::uint32_t seed) {
    BringupReport report{};
    consecutive_bad_windows_ = 0U;

    set_state(LinkState::Reset, &report);
    phy_.reset();
    phy_.clear_dfe_taps();
    phy_.set_ctle_code(0U);

    set_state(LinkState::WaitForPll, &report);
    if (!wait_for_pll_lock()) {
        report.fault = FaultReason::PllTimeout;
        set_state(LinkState::Fault, &report);
        return report;
    }

    phy_.restart_test_sequence(seed ^ kBaselineSeedMask);
    report.baseline = phy_.measure(config_.verify_symbols, MeasurementMode::Verification);
    if (!report.baseline.valid) {
        report.fault = FaultReason::MeasurementFault;
        set_state(LinkState::Fault, &report);
        return report;
    }

    set_state(LinkState::CtleSweep, &report);
    Measurement best_measurement{};
    std::uint8_t best_code = 0U;

    for (std::uint8_t code = 0U; code <= kMaxCtleCode; ++code) {
        phy_.set_ctle_code(code);
        phy_.clear_dfe_taps();
        phy_.restart_test_sequence(seed ^ kSweepSeedMask);
        const auto candidate = phy_.measure(config_.sweep_symbols, MeasurementMode::Training);
        if (!candidate.valid) {
            report.fault = FaultReason::MeasurementFault;
            set_state(LinkState::Fault, &report);
            return report;
        }

        const bool fewer_errors = candidate.errors < best_measurement.errors;
        const bool equal_errors_lower_mse =
            candidate.errors == best_measurement.errors &&
            candidate.mean_squared_error < best_measurement.mean_squared_error;
        // The first candidate establishes the baseline for the remaining comparisons.
        if (code == 0U || fewer_errors || equal_errors_lower_mse) {
            best_measurement = candidate;
            best_code = code;
        }
    }

    report.selected_ctle_code = best_code;
    phy_.set_ctle_code(best_code);
    phy_.clear_dfe_taps();

    set_state(LinkState::DfeTraining, &report);
    phy_.restart_test_sequence(seed ^ kTrainingSeedMask);
    std::uint32_t unchanged_windows = 0U;
    bool training_converged = false;
    for (std::uint32_t window = 0U; window < config_.max_training_windows; ++window) {
        const auto measurement =
            phy_.measure(config_.training_symbols_per_window, MeasurementMode::Training);
        report.training_windows = window + 1U;
        if (!measurement.valid) {
            report.fault = FaultReason::MeasurementFault;
            set_state(LinkState::Fault, &report);
            return report;
        }

        bool taps_changed = false;
        for (std::size_t tap = 0; tap < kDfeTapCount; ++tap) {
            const double normalized_correlation =
                static_cast<double>(measurement.error_correlations[tap]) /
                static_cast<double>(measurement.symbols);
            if (std::abs(normalized_correlation) <= config_.correlation_deadband) {
                continue;
            }

            const int old_code = static_cast<int>(phy_.dfe_tap_code(tap));
            const int new_code = old_code + dfe_tap_step(normalized_correlation);
            phy_.set_dfe_tap_code(tap, new_code);
            taps_changed = taps_changed || static_cast<int>(phy_.dfe_tap_code(tap)) != old_code;
        }

        unchanged_windows = taps_changed ? 0U : unchanged_windows + 1U;
        if (unchanged_windows >= config_.stable_training_windows) {
            training_converged = true;
            break;
        }
    }

    if (!training_converged) {
        report.fault = FaultReason::TrainingNotConverged;
        set_state(LinkState::Fault, &report);
        return report;
    }

    report.trained_dfe_taps = phy_.dfe_tap_codes();
    set_state(LinkState::Verify, &report);
    phy_.restart_test_sequence(seed ^ kVerificationSeedMask);
    report.trained = phy_.measure(config_.verify_symbols, MeasurementMode::Verification);
    if (!report.trained.valid) {
        report.fault = FaultReason::MeasurementFault;
        set_state(LinkState::Fault, &report);
        return report;
    }

    if (ber_upper_bound_95(report.trained) > config_.maximum_ber) {
        report.fault = FaultReason::BerTargetMissed;
        set_state(LinkState::Fault, &report);
        return report;
    }

    report.success = true;
    set_state(LinkState::LinkUp, &report);
    return report;
}

HealthAction FirmwareController::check_link_health(
    const std::uint32_t symbols,
    const std::uint32_t seed) {
    if (state_ != LinkState::LinkUp) {
        return HealthAction::NotLinkUp;
    }

    if (!phy_.is_pll_locked()) {
        set_state(LinkState::Fault);
        return HealthAction::RetrainRequired;
    }

    phy_.restart_test_sequence(seed);
    last_health_measurement_ = phy_.measure(symbols, MeasurementMode::Verification);
    if (!last_health_measurement_.valid) {
        set_state(LinkState::Fault);
        return HealthAction::RetrainRequired;
    }

    if (ber_upper_bound_95(last_health_measurement_) <= config_.maximum_ber) {
        consecutive_bad_windows_ = 0U;
        return HealthAction::Healthy;
    }

    ++consecutive_bad_windows_;
    if (consecutive_bad_windows_ < config_.degraded_windows_before_retrain) {
        return HealthAction::Observe;
    }

    set_state(LinkState::Degraded);
    return HealthAction::RetrainRequired;
}

void FirmwareController::set_state(const LinkState next, BringupReport* const report) {
    state_ = next;
    if (report != nullptr && report->state_trace_size < report->state_trace.size()) {
        report->state_trace[report->state_trace_size] = next;
        ++report->state_trace_size;
    }
}

bool FirmwareController::wait_for_pll_lock() {
    for (std::uint32_t elapsed = 0U; elapsed < config_.pll_timeout_ticks; ++elapsed) {
        if (phy_.is_pll_locked()) {
            return true;
        }
        phy_.tick();
    }
    return phy_.is_pll_locked();
}

int FirmwareController::dfe_tap_step(const double normalized_correlation) noexcept {
    const double magnitude = std::abs(normalized_correlation);
    int step_size = 1;
    if (magnitude > 0.50) {
        step_size = 3;
    } else if (magnitude > 0.20) {
        step_size = 2;
    }
    return normalized_correlation > 0.0 ? -step_size : step_size;
}

}  // namespace serdes
