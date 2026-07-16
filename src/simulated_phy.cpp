#include "serdes/simulated_phy.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace serdes {
namespace {

constexpr double kQ16Scale = 65'536.0;
constexpr std::array<double, 8> kCtleFeedbackCodebook{
    0.00, 0.12, 0.22, 0.32, 0.42, 0.50, 0.58, 0.65,
};

[[nodiscard]] std::uint32_t encode_unsigned_q16(const double value) noexcept {
    const auto scaled = std::llround(std::clamp(value, 0.0, 65'535.0) * kQ16Scale);
    return static_cast<std::uint32_t>(std::min<long long>(
        scaled, static_cast<long long>(std::numeric_limits<std::uint32_t>::max())));
}

[[nodiscard]] std::uint32_t encode_signed_q16(const double value) noexcept {
    const auto scaled = std::llround(std::clamp(value, -32'768.0, 32'767.0) * kQ16Scale);
    const auto signed_value = static_cast<std::int32_t>(std::clamp<long long>(
        scaled,
        static_cast<long long>(std::numeric_limits<std::int32_t>::min()),
        static_cast<long long>(std::numeric_limits<std::int32_t>::max())));
    return std::bit_cast<std::uint32_t>(signed_value);
}

[[nodiscard]] std::uint32_t encode_signed_integer(const std::int32_t value) noexcept {
    return std::bit_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t encode_tap(const std::int8_t value) noexcept {
    return std::bit_cast<std::uint8_t>(value);
}

}  // namespace

SimulatedPhy::SimulatedPhy(ChannelProfile profile)
    : profile_(profile), prbs_(Prbs31::kDefaultSeed) {
    reset_model();
}

std::uint32_t SimulatedPhy::read32(const Register address) const {
    switch (address) {
    case Register::Control:
    case Register::MeasureControl:
        return 0U;
    case Register::Status: {
        std::uint32_t status = 0U;
        if (pll_locked_) {
            status |= status_bits::kPllLocked;
        }
        if (measurement_done_) {
            status |= status_bits::kMeasurementDone;
        }
        if (measurement_fault_) {
            status |= status_bits::kMeasurementFault;
        }
        return status;
    }
    case Register::CtleCode:
        return ctle_code_;
    case Register::DfeTap0:
        return encode_tap(dfe_tap_codes_[0]);
    case Register::DfeTap1:
        return encode_tap(dfe_tap_codes_[1]);
    case Register::DfeTap2:
        return encode_tap(dfe_tap_codes_[2]);
    case Register::PatternSeed:
        return prbs_.state();
    case Register::MeasureSymbols:
        return requested_symbols_;
    case Register::ErrorCount:
        return last_measurement_.errors;
    case Register::SymbolCount:
        return last_measurement_.symbols;
    case Register::MeanSquaredErrorQ16:
        return encode_unsigned_q16(last_measurement_.mean_squared_error);
    case Register::MeanMarginQ16:
        return encode_signed_q16(last_measurement_.mean_margin);
    case Register::ErrorCorrelation0:
        return encode_signed_integer(last_measurement_.error_correlations[0]);
    case Register::ErrorCorrelation1:
        return encode_signed_integer(last_measurement_.error_correlations[1]);
    case Register::ErrorCorrelation2:
        return encode_signed_integer(last_measurement_.error_correlations[2]);
    }
    return 0U;
}

void SimulatedPhy::write32(const Register address, const std::uint32_t value) {
    switch (address) {
    case Register::Control:
        if ((value & control_bits::kReset) != 0U) {
            reset_model();
        }
        break;
    case Register::CtleCode:
        ctle_code_ = static_cast<std::uint8_t>(std::min<std::uint32_t>(value, kMaxCtleCode));
        break;
    case Register::DfeTap0:
        dfe_tap_codes_[0] = clamp_tap_code(std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(value & 0xFFU)));
        break;
    case Register::DfeTap1:
        dfe_tap_codes_[1] = clamp_tap_code(std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(value & 0xFFU)));
        break;
    case Register::DfeTap2:
        dfe_tap_codes_[2] = clamp_tap_code(std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(value & 0xFFU)));
        break;
    case Register::PatternSeed:
        restart_pattern(value);
        break;
    case Register::MeasureSymbols:
        requested_symbols_ = value;
        break;
    case Register::MeasureControl:
        if ((value & measure_bits::kStart) != 0U) {
            measurement_done_ = false;
            measurement_fault_ = false;
            run_measurement((value & measure_bits::kTrainingMode) != 0U);
        }
        break;
    case Register::Status:
    case Register::ErrorCount:
    case Register::SymbolCount:
    case Register::MeanSquaredErrorQ16:
    case Register::MeanMarginQ16:
    case Register::ErrorCorrelation0:
    case Register::ErrorCorrelation1:
    case Register::ErrorCorrelation2:
        break;
    }
}

void SimulatedPhy::tick() {
    if (pll_locked_ || profile_.force_pll_fault) {
        return;
    }
    ++ticks_since_reset_;
    if (ticks_since_reset_ >= profile_.pll_lock_delay_ticks) {
        pll_locked_ = true;
    }
}

void SimulatedPhy::set_channel_profile(const ChannelProfile profile) {
    profile_ = profile;
}

void SimulatedPhy::reset_model() {
    ticks_since_reset_ = 0U;
    pll_locked_ = false;
    measurement_done_ = false;
    measurement_fault_ = false;
    ctle_code_ = 0U;
    dfe_tap_codes_.fill(0);
    requested_symbols_ = 0U;
    last_measurement_ = {};
    restart_pattern(Prbs31::kDefaultSeed);
}

void SimulatedPhy::restart_pattern(const std::uint32_t seed) {
    const auto valid_seed = (seed & Prbs31::kMask) == 0U ? Prbs31::kDefaultSeed : seed;
    prbs_.reset(valid_seed);
    noise_state_ = (static_cast<std::uint64_t>(valid_seed) << 32U) ^
                   0xA076'1D64'78BD'642FULL;
    if (noise_state_ == 0U) {
        noise_state_ = 0xE703'7ED1'A0B4'28DBULL;
    }
    spare_gaussian_noise_ = 0.0;
    has_spare_gaussian_noise_ = false;
    symbol_history_.fill(0.0);
    feedback_history_.fill(0.0);
    previous_raw_sample_ = 0.0;
    warmup_required_ = true;
}

void SimulatedPhy::run_measurement(const bool training_mode) {
    if (!pll_locked_ || requested_symbols_ == 0U) {
        last_measurement_ = {};
        measurement_fault_ = true;
        measurement_done_ = true;
        return;
    }

    if (warmup_required_) {
        constexpr std::uint32_t kWarmupSymbols = 32;
        for (std::uint32_t index = 0; index < kWarmupSymbols; ++index) {
            static_cast<void>(step_symbol(true));
        }
        warmup_required_ = false;
    }

    Measurement accumulated{};
    accumulated.symbols = requested_symbols_;
    for (std::uint32_t index = 0; index < requested_symbols_; ++index) {
        const auto observation = step_symbol(training_mode);
        accumulated.errors += observation.bit_error ? 1U : 0U;
        accumulated.mean_squared_error += observation.squared_error;
        accumulated.mean_margin += observation.signed_margin;
        for (std::size_t tap = 0; tap < kDfeTapCount; ++tap) {
            accumulated.error_correlations[tap] += observation.correlation_signs[tap];
        }
    }

    const auto denominator = static_cast<double>(requested_symbols_);
    accumulated.mean_squared_error /= denominator;
    accumulated.mean_margin /= denominator;
    accumulated.valid = true;
    last_measurement_ = accumulated;
    measurement_done_ = true;
}

SimulatedPhy::SymbolObservation SimulatedPhy::step_symbol(const bool training_mode) {
    for (std::size_t index = symbol_history_.size() - 1U; index > 0U; --index) {
        symbol_history_[index] = symbol_history_[index - 1U];
    }
    const auto target_symbol = static_cast<double>(prbs_.next_nrz_symbol());
    symbol_history_[0] = target_symbol;

    double raw_sample = profile_.noise_sigma * gaussian_noise();
    for (std::size_t index = 0; index < symbol_history_.size(); ++index) {
        raw_sample += profile_.impulse_response[index] * symbol_history_[index];
    }

    const double front_end_sample =
        raw_sample - ctle_feedback(ctle_code_) * previous_raw_sample_;
    double equalized_sample = front_end_sample;
    for (std::size_t tap = 0; tap < kDfeTapCount; ++tap) {
        equalized_sample -= static_cast<double>(dfe_tap_codes_[tap]) * kDfeTapLsb *
                            feedback_history_[tap];
    }

    const double decision = equalized_sample >= 0.0 ? 1.0 : -1.0;
    const double analog_error = target_symbol - equalized_sample;

    SymbolObservation observation{};
    observation.bit_error = decision != target_symbol;
    observation.squared_error = analog_error * analog_error;
    observation.signed_margin = target_symbol * equalized_sample;

    const int error_sign = analog_error > 1.0e-12 ? 1 : (analog_error < -1.0e-12 ? -1 : 0);
    for (std::size_t tap = 0; tap < kDfeTapCount; ++tap) {
        const int history_sign = feedback_history_[tap] > 0.0
                                     ? 1
                                     : (feedback_history_[tap] < 0.0 ? -1 : 0);
        observation.correlation_signs[tap] = error_sign * history_sign;
    }

    for (std::size_t tap = feedback_history_.size() - 1U; tap > 0U; --tap) {
        feedback_history_[tap] = feedback_history_[tap - 1U];
    }
    feedback_history_[0] = training_mode ? target_symbol : decision;
    previous_raw_sample_ = raw_sample;
    return observation;
}

double SimulatedPhy::gaussian_noise() noexcept {
    if (has_spare_gaussian_noise_) {
        has_spare_gaussian_noise_ = false;
        return spare_gaussian_noise_;
    }

    const double first_uniform = std::max(uniform01(), std::numeric_limits<double>::min());
    const double second_uniform = uniform01();
    const double radius = std::sqrt(-2.0 * std::log(first_uniform));
    const double angle = 2.0 * std::numbers::pi * second_uniform;
    spare_gaussian_noise_ = radius * std::sin(angle);
    has_spare_gaussian_noise_ = true;
    return radius * std::cos(angle);
}

double SimulatedPhy::uniform01() noexcept {
    std::uint64_t value = noise_state_;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    noise_state_ = value;
    const auto scrambled = value * 0x2545'F491'4F6C'DD1DULL;
    return static_cast<double>(scrambled >> 11U) * (1.0 / 9'007'199'254'740'992.0);
}

double SimulatedPhy::ctle_feedback(const std::uint8_t code) noexcept {
    return kCtleFeedbackCodebook[std::min<std::size_t>(code, kCtleFeedbackCodebook.size() - 1U)];
}

std::int8_t SimulatedPhy::clamp_tap_code(const int value) noexcept {
    return static_cast<std::int8_t>(
        std::clamp(value, static_cast<int>(kMinDfeTapCode), static_cast<int>(kMaxDfeTapCode)));
}

}  // namespace serdes
