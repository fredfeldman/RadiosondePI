#pragma once

#include "sdr/RtlSdrDevice.hpp"
#include "dsp/DiversityCombiner.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace RadiosondePI::SDR {

struct DiversityConfig {
    bool enabled{false};
    DSP::DiversityMode mode{DSP::DiversityMode::MaximalRatioCombining};
    std::vector<SdrConfig> dongleConfigs;
};

class DiversityReceiver {
public:
    using CombinedCallback = std::function<void(const DSP::Complex32* samples, size_t count)>;

    explicit DiversityReceiver(const DiversityConfig& config = DiversityConfig{})
        : m_config(config)
        , m_combiner(config.mode, std::max(config.dongleConfigs.size(), size_t(1))) {}

    ~DiversityReceiver() {
        stop();
    }

    bool open() {
        if (m_config.dongleConfigs.empty()) {
            // Default to single device
            SdrConfig defaultCfg{};
            m_devices.push_back(std::make_unique<RtlSdrDevice>());
            return m_devices.back()->open(defaultCfg);
        }

        bool anySuccess = false;
        for (const auto& cfg : m_config.dongleConfigs) {
            auto dev = std::make_unique<RtlSdrDevice>();
            if (dev->open(cfg)) {
                m_devices.push_back(std::move(dev));
                anySuccess = true;
            }
        }
        return anySuccess;
    }

    void close() {
        stop();
        for (auto& dev : m_devices) {
            dev->close();
        }
        m_devices.clear();
    }

    bool start(CombinedCallback callback) {
        if (m_devices.empty()) return false;
        m_callback = std::move(callback);

        if (m_devices.size() == 1) {
            // Single dongle bypass
            return m_devices[0]->startAsync(m_callback);
        }

        // Multi-dongle diversity receiver
        m_channelBuffers.resize(m_devices.size());
        m_combinedBuffer.resize(131072);

        for (size_t i = 0; i < m_devices.size(); ++i) {
            size_t ch = i;
            m_devices[ch]->startAsync([this, ch](const DSP::Complex32* samples, size_t count) {
                onChannelSamples(ch, samples, count);
            });
        }
        return true;
    }

    void stop() {
        for (auto& dev : m_devices) {
            dev->stopAsync();
        }
    }

    bool setFrequency(uint32_t frequencyHz) {
        bool success = true;
        for (auto& dev : m_devices) {
            if (!dev->setFrequency(frequencyHz)) {
                success = false;
            }
        }
        return success;
    }

    [[nodiscard]] size_t getActiveChannelCount() const {
        return m_devices.size();
    }

    [[nodiscard]] DSP::DiversityMode getDiversityMode() const {
        return m_combiner.getMode();
    }

private:
    void onChannelSamples(size_t channel, const DSP::Complex32* samples, size_t count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_channelBuffers[channel].assign(samples, samples + count);

        // Check if all channels have delivered samples
        bool allReady = true;
        size_t minCount = count;
        std::vector<const DSP::Complex32*> ptrs(m_devices.size(), nullptr);
        std::vector<size_t> counts(m_devices.size(), 0);

        for (size_t i = 0; i < m_devices.size(); ++i) {
            if (m_channelBuffers[i].empty()) {
                allReady = false;
                break;
            }
            ptrs[i] = m_channelBuffers[i].data();
            counts[i] = m_channelBuffers[i].size();
            minCount = std::min(minCount, counts[i]);
        }

        if (allReady && minCount > 0) {
            size_t combined = m_combiner.combineBlocks(
                ptrs.data(), counts.data(), ptrs.size(),
                m_combinedBuffer.data(), m_combinedBuffer.size()
            );

            for (auto& buf : m_channelBuffers) {
                buf.clear();
            }

            if (m_callback && combined > 0) {
                m_callback(m_combinedBuffer.data(), combined);
            }
        }
    }

    DiversityConfig m_config;
    DSP::DiversityCombiner m_combiner;
    std::vector<std::unique_ptr<RtlSdrDevice>> m_devices;
    CombinedCallback m_callback;

    std::mutex m_mutex;
    std::vector<std::vector<DSP::Complex32>> m_channelBuffers;
    std::vector<DSP::Complex32> m_combinedBuffer;
};

} // namespace RadiosondePI::SDR
