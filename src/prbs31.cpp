#include "serdes/prbs31.hpp"

namespace serdes {

Prbs31::Prbs31(const std::uint32_t seed) {
    reset(seed);
}

void Prbs31::reset(const std::uint32_t seed) {
    const auto masked_seed = seed & kMask;
    state_ = masked_seed == 0U ? kDefaultSeed : masked_seed;
}

bool Prbs31::next_bit() noexcept {
    const bool output = ((state_ >> 30U) & 1U) != 0U;
    const auto feedback = ((state_ >> 30U) ^ (state_ >> 27U)) & 1U;
    state_ = ((state_ << 1U) & kMask) | feedback;
    return output;
}

int Prbs31::next_nrz_symbol() noexcept {
    return next_bit() ? 1 : -1;
}

}  // namespace serdes

