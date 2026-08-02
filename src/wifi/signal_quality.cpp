#include "signal_quality.h"

#include <chrono>
#include <cmath>
#include <random>

namespace {

std::string generate_random_string(size_t length) {
    const std::string characters = "abcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, characters.size() - 1);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += characters[distrib(gen)];
    }
    return result;
}

} // namespace

// Remove RSSI samples older than 1 second
void SignalQualityCalculator::cleanup_old_rssi_data() {
    const auto now = std::chrono::steady_clock::now();
    const auto cutoff = now - averaging_window_;

    // Erase-remove idiom for data older than cutoff
    std::erase_if(rssi_data_, [&](const RssiEntry &entry) { return entry.timestamp < cutoff; });
}

void SignalQualityCalculator::cleanup_old_snr_data() {
    const auto now = std::chrono::steady_clock::now();
    const auto cutoff = now - averaging_window_;

    // Erase-remove idiom for data older than cutoff
    std::erase_if(snr_data_, [&](const SnrEntry &entry) { return entry.timestamp < cutoff; });
}

void SignalQualityCalculator::cleanup_old_fec_data() {
    const auto now = std::chrono::steady_clock::now();
    const auto cutoff = now - averaging_window_;

    std::erase_if(fec_data_, [&](const FecEntry &entry) { return entry.timestamp < cutoff; });
}

void SignalQualityCalculator::add_rssi(const std::array<uint8_t, MAX_RX_CHAINS> &rssi) {
    std::lock_guard lock(mutex_);

    RssiEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.values = rssi;
    rssi_data_.push_back(entry);
}

void SignalQualityCalculator::add_snr(const std::array<int8_t, MAX_RX_CHAINS> &snr) {
    std::lock_guard lock(mutex_);

    SnrEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.values = snr;
    snr_data_.push_back(entry);
}

SignalQualityCalculator::SignalQuality SignalQualityCalculator::calculate_signal_quality() {
    SignalQuality ret{};
    std::lock_guard lock(mutex_);

    // Make sure we clean up old data first
    cleanup_old_rssi_data();
    cleanup_old_snr_data();
    cleanup_old_fec_data();

    auto avg_rssi = get_average(rssi_data_);
    auto avg_snr = get_average(snr_data_);

    auto [p_recovered, p_lost, p_total] = get_accumulated_fec_data();

    ret.lost_last_second = p_lost;
    ret.recovered_last_second = p_recovered;
    ret.total_last_second = p_total;

    ret.idr_code = idr_code_;

    for (std::size_t chain = 0; chain < MAX_RX_CHAINS; ++chain) {
        ret.rssi[chain] = std::lround(avg_rssi[chain]);
        ret.snr[chain] = std::lround(avg_snr[chain]);

        // RSSI falls in range [0, 126], and SNR falls in range [0, 60].
        const float mapped_rssi = map_range(avg_rssi[chain], 50.f, 110.f, 1000.f, 2000.f);
        const float mapped_snr = map_range(avg_snr[chain], 20.f, 50.f, 1000.f, 2000.f);

        // Link Score = (weight1 * RSSI) + (weight2 * SNR)
        // See https://github.com/OpenIPC/adaptive-link
        ret.link_score[chain] = std::lround(0.5f * mapped_rssi + 0.5f * mapped_snr);
    }

    return ret;
}

std::tuple<uint32_t, uint32_t, uint32_t> SignalQualityCalculator::get_accumulated_fec_data() const {
    uint32_t p_recovered = 0;
    uint32_t p_all = 0;
    uint32_t p_lost = 0;

    for (const auto &data : fec_data_) {
        p_all += data.all;
        p_recovered += data.recovered;
        p_lost += data.lost;
    }

    return {p_recovered, p_lost, p_all};
}

void SignalQualityCalculator::add_fec(uint32_t p_all, uint32_t p_recovered, uint32_t p_lost) {
    std::lock_guard lock(mutex_);

    FecEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.all = p_all;
    entry.recovered = p_recovered;
    entry.lost = p_lost;

    if (p_lost > 0) {
        idr_code_ = generate_random_string(4);
    }

    fec_data_.push_back(entry);
}
