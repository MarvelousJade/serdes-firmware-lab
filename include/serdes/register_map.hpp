#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace serdes {

inline constexpr std::size_t kDfeTapCount = 3;
inline constexpr std::uint8_t kMaxCtleCode = 7;
inline constexpr std::int8_t kMinDfeTapCode = -63;
inline constexpr std::int8_t kMaxDfeTapCode = 63;
inline constexpr double kDfeTapLsb = 1.0 / 64.0;

enum class Register : std::uint16_t {
    Control = 0x0000,
    Status = 0x0004,
    CtleCode = 0x0008,
    DfeTap0 = 0x000C,
    DfeTap1 = 0x0010,
    DfeTap2 = 0x0014,
    PatternSeed = 0x0018,
    MeasureSymbols = 0x001C,
    MeasureControl = 0x0020,
    ErrorCount = 0x0024,
    SymbolCount = 0x0028,
    MeanSquaredErrorQ16 = 0x002C,
    MeanMarginQ16 = 0x0030,
    ErrorCorrelation0 = 0x0034,
    ErrorCorrelation1 = 0x0038,
    ErrorCorrelation2 = 0x003C,
};

inline constexpr std::array<Register, kDfeTapCount> kDfeTapRegisters{
    Register::DfeTap0,
    Register::DfeTap1,
    Register::DfeTap2,
};

inline constexpr std::array<Register, kDfeTapCount> kCorrelationRegisters{
    Register::ErrorCorrelation0,
    Register::ErrorCorrelation1,
    Register::ErrorCorrelation2,
};

namespace control_bits {
inline constexpr std::uint32_t kReset = 1U << 0U;
}

namespace status_bits {
inline constexpr std::uint32_t kPllLocked = 1U << 0U;
inline constexpr std::uint32_t kMeasurementDone = 1U << 1U;
inline constexpr std::uint32_t kMeasurementFault = 1U << 2U;
}

namespace measure_bits {
inline constexpr std::uint32_t kStart = 1U << 0U;
inline constexpr std::uint32_t kTrainingMode = 1U << 1U;
}

class IRegisterIo {
public:
    virtual ~IRegisterIo() = default;
    [[nodiscard]] virtual std::uint32_t read32(Register address) const = 0;
    virtual void write32(Register address, std::uint32_t value) = 0;
    virtual void tick() = 0;
};

}  // namespace serdes

