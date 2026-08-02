#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

constexpr std::size_t MAX_RX_CHAINS = 4;

inline double map_range(double input, double input_min, double input_max, double output_min, double output_max) {
    // 1. Clamp the input value first
    const double clamped_input = std::clamp(input, input_min, input_max);

    // 2. Map the clamped value
    return output_min + (clamped_input - input_min) * (output_max - output_min) / (input_max - input_min);
}

class SignalQualityCalculator {
public:
    struct SignalQuality {
        int lost_last_second{};
        int recovered_last_second{};
        int total_last_second{};
        std::array<int, MAX_RX_CHAINS> rssi{};       // Received signal strength indicator
        std::array<int, MAX_RX_CHAINS> snr{};        // Signal-to-noise ratio
        std::array<int, MAX_RX_CHAINS> link_score{}; // Based on RSSI and SNR [1000, 2000]
        std::string idr_code;
    };

    SignalQualityCalculator() = default;
    ~SignalQualityCalculator() = default;

    /// Add a new RSSI entry with current timestamp
    void add_rssi(const std::array<uint8_t, MAX_RX_CHAINS> &rssi);

    /// Add a new SNR entry with current timestamp
    void add_snr(const std::array<int8_t, MAX_RX_CHAINS> &snr);

    /// Add new FEC entry with current timestamp
    void add_fec(uint32_t p_all, uint32_t p_recovered, uint32_t p_lost);

    template <class T>
    std::array<float, MAX_RX_CHAINS> get_average(const T &entries) {
        std::lock_guard lock(mutex_);

        std::array<float, MAX_RX_CHAINS> sums{};
        const int count = static_cast<int>(entries.size());

        if (count > 0) {
            for (const auto &entry : entries) {
                for (std::size_t chain = 0; chain < MAX_RX_CHAINS; ++chain) {
                    sums[chain] += entry.values[chain];
                }
            }
            for (auto &sum : sums) {
                sum /= count;
            }
        }

        return sums;
    }

    /// Calculate signal quality over the averaging window
    SignalQuality calculate_signal_quality();

private:
    /// Sum up FEC data over the averaging window
    std::tuple<uint32_t, uint32_t, uint32_t> get_accumulated_fec_data() const;

    // Helper methods to remove old entries
    void cleanup_old_rssi_data();
    void cleanup_old_snr_data();
    void cleanup_old_fec_data();

    // We store a timestamp for each RSSI entry
    struct RssiEntry {
        std::chrono::steady_clock::time_point timestamp;
        std::array<uint8_t, MAX_RX_CHAINS> values{};
    };

    // We store a timestamp for each RSSI entry
    struct SnrEntry {
        std::chrono::steady_clock::time_point timestamp;
        std::array<int8_t, MAX_RX_CHAINS> values{};
    };

    // We store a timestamp for each FEC entry
    struct FecEntry {
        std::chrono::steady_clock::time_point timestamp;
        uint32_t all{};
        uint32_t recovered{};
        uint32_t lost{};
    };

private:
    const std::chrono::seconds averaging_window_{std::chrono::seconds(1)};

    mutable std::recursive_mutex mutex_;

    std::vector<RssiEntry> rssi_data_;

    std::vector<SnrEntry> snr_data_;

    std::vector<FecEntry> fec_data_;

    // 4-character random string
    std::string idr_code_{"aaaa"};
};
