#include "serdes/firmware_controller.hpp"
#include "serdes/phy_driver.hpp"
#include "serdes/phy_types.hpp"
#include "serdes/simulated_phy.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct Arguments {
    std::string profile{"medium"};
    std::uint32_t seed{1U};
    std::uint32_t verify_symbols{200'000U};
    bool json{false};
    bool help{false};
};

[[nodiscard]] bool parse_u32(const std::string_view text, std::uint32_t& value) {
    const auto* const begin = text.data();
    const auto* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool parse_arguments(const int argc, char** argv, Arguments& args) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--json") {
            args.json = true;
        } else if (argument == "--help" || argument == "-h") {
            args.help = true;
        } else if ((argument == "--profile" || argument == "--seed" ||
                    argument == "--verify-symbols") &&
                   index + 1 < argc) {
            const std::string_view value{argv[++index]};
            if (argument == "--profile") {
                args.profile = value;
            } else if (argument == "--seed") {
                if (!parse_u32(value, args.seed)) {
                    return false;
                }
            } else if (!parse_u32(value, args.verify_symbols) || args.verify_symbols == 0U) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

void print_usage(const char* const program) {
    std::cout << "Usage: " << program
              << " [--profile short|medium|long] [--seed N]"
                 " [--verify-symbols N] [--json]\n";
}

void print_json(
    const Arguments& args,
    const serdes::BringupReport& report) {
    std::cout << std::setprecision(10)
              << "{\"profile\":\"" << args.profile << "\""
              << ",\"seed\":" << args.seed
              << ",\"success\":" << (report.success ? "true" : "false")
              << ",\"fault\":\"" << serdes::to_string(report.fault) << "\""
              << ",\"baseline_errors\":" << report.baseline.errors
              << ",\"baseline_symbols\":" << report.baseline.symbols
              << ",\"baseline_ber\":" << report.baseline.ber()
              << ",\"trained_errors\":" << report.trained.errors
              << ",\"trained_symbols\":" << report.trained.symbols
              << ",\"trained_ber\":" << report.trained.ber()
              << ",\"trained_ber_95_upper\":" << serdes::ber_upper_bound_95(report.trained)
              << ",\"ctle_code\":" << static_cast<int>(report.selected_ctle_code)
              << ",\"dfe_tap_codes\":[";
    for (std::size_t tap = 0; tap < report.trained_dfe_taps.size(); ++tap) {
        if (tap != 0U) {
            std::cout << ',';
        }
        std::cout << static_cast<int>(report.trained_dfe_taps[tap]);
    }
    std::cout << "]"
              << ",\"training_windows\":" << report.training_windows
              << ",\"state_trace\":[";
    for (std::size_t index = 0; index < report.state_trace_size; ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << serdes::to_string(report.state_trace[index]) << '\"';
    }
    std::cout << "]}\n";
}

void print_human(const Arguments& args, const serdes::BringupReport& report) {
    std::cout << "SERDES firmware bring-up: " << args.profile << " channel\n";
    std::cout << "State trace: ";
    for (std::size_t index = 0; index < report.state_trace_size; ++index) {
        if (index != 0U) {
            std::cout << " -> ";
        }
        std::cout << serdes::to_string(report.state_trace[index]);
    }
    std::cout << '\n';
    std::cout << std::scientific << std::setprecision(3)
              << "Baseline BER: " << report.baseline.ber() << " ("
              << report.baseline.errors << '/' << report.baseline.symbols << ")\n"
              << "Trained BER:  " << report.trained.ber() << " ("
              << report.trained.errors << '/' << report.trained.symbols << ")\n";
    std::cout << "Approximate one-sided 95% BER upper estimate: "
              << serdes::ber_upper_bound_95(report.trained) << '\n';
    std::cout << "CTLE code: " << static_cast<int>(report.selected_ctle_code) << '\n'
              << "DFE tap codes: ["
              << static_cast<int>(report.trained_dfe_taps[0]) << ", "
              << static_cast<int>(report.trained_dfe_taps[1]) << ", "
              << static_cast<int>(report.trained_dfe_taps[2]) << "]\n"
              << "Training windows: " << report.training_windows << '\n'
              << "Result: " << (report.success ? "PASS" : "FAIL")
              << " (" << serdes::to_string(report.fault) << ")\n";
}

}  // namespace

int main(const int argc, char** argv) {
    Arguments args{};
    if (!parse_arguments(argc, argv, args)) {
        print_usage(argv[0]);
        return 1;
    }
    if (args.help) {
        print_usage(argv[0]);
        return 0;
    }

    const auto profile = serdes::find_channel_profile(args.profile);
    if (!profile.has_value()) {
        std::cerr << "Unknown channel profile: " << args.profile << '\n';
        print_usage(argv[0]);
        return 1;
    }

    serdes::SimulatedPhy model{*profile};
    serdes::PhyDriver phy{model};
    serdes::FirmwareConfig config{};
    config.verify_symbols = args.verify_symbols;
    serdes::FirmwareController firmware{phy, config};
    const auto report = firmware.bring_up(args.seed);

    if (args.json) {
        print_json(args, report);
    } else {
        print_human(args, report);
    }
    return report.success ? 0 : 2;
}
