#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "SteamSpatial.h"

namespace {
struct Metrics {
    float peak = 0.0f;
    double rmsLeft = 0.0;
    double rmsRight = 0.0;
    size_t clippedSamples = 0;
};

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

std::vector<float> makeSignal(int rate, bool broadband) {
    constexpr float pi = 3.14159265358979323846f;
    constexpr float amplitude = 0.95f;
    std::vector<float> signal(static_cast<size_t>(rate) * 2 * 2);
    uint32_t state = 0x13579bdfu;
    for (int frame = 0; frame < rate * 2; ++frame) {
        float left;
        float right;
        if (broadband) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            left = amplitude * (static_cast<float>(state & 0xffffu) / 32767.5f - 1.0f);
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            right = amplitude * (static_cast<float>(state & 0xffffu) / 32767.5f - 1.0f);
        } else {
            left = right = amplitude * std::sin(2.0f * pi * 997.0f * frame / rate);
        }
        signal[2 * frame] = left;
        signal[2 * frame + 1] = right;
    }
    return signal;
}

std::vector<float> render(int rate, bool broadband, const std::vector<int>& chunks) {
    auto output = makeSignal(rate, broadband);
    axionfx::SteamSpatial spatial;
    spatial.configure(rate);
    spatial.setWidth(100);
    spatial.setBlend(70);
    spatial.setBassPreservation(100);
    spatial.setEnabled(true);
    require(spatial.isInitialized(), "real Steam Audio HRTF initialization");

    int position = 0;
    size_t chunkIndex = 0;
    const int frames = rate * 2;
    while (position < frames) {
        const int count = std::min(chunks[chunkIndex++ % chunks.size()], frames - position);
        spatial.process(output.data() + 2 * position, count);
        position += count;
    }
    return output;
}

Metrics measure(const std::vector<float>& signal, int rate) {
    Metrics result;
    const int frames = static_cast<int>(signal.size() / 2);
    const int startFrame = std::min(256, frames - 1);
    for (int frame = 0; frame < frames; ++frame) {
        const float left = signal[2 * frame];
        const float right = signal[2 * frame + 1];
        require(std::isfinite(left) && std::isfinite(right), "real HRTF output remains finite");
        result.peak = std::max(result.peak, std::max(std::fabs(left), std::fabs(right)));
        if (std::fabs(left) >= 1.0f || std::fabs(right) >= 1.0f)
            ++result.clippedSamples;
        if (frame >= startFrame) {
            result.rmsLeft += static_cast<double>(left) * left;
            result.rmsRight += static_cast<double>(right) * right;
        }
    }
    const double count = frames - startFrame;
    result.rmsLeft = std::sqrt(result.rmsLeft / count);
    result.rmsRight = std::sqrt(result.rmsRight / count);
    require(result.peak <= 0.9501f, "linked limiter holds real HRTF output below headroom ceiling");
    require(result.clippedSamples == 0, "real HRTF output has no full-scale samples");
    require(result.rmsLeft > 0.01 && result.rmsRight > 0.01,
            "real HRTF output retains audible signal in both channels");
    (void) rate;
    return result;
}
}

int main() {
    const std::vector<int> regularCallbacks{1920};
    const std::vector<int> irregularCallbacks{1, 64, 127, 128, 129, 256, 480, 882, 1024, 1920, 2048};
    for (const int rate : {44100, 48000, 96000, 192000}) {
        for (const bool broadband : {false, true}) {
            const auto regular = render(rate, broadband, regularCallbacks);
            const auto irregular = render(rate, broadband, irregularCallbacks);
            require(regular.size() == irregular.size(), "render lengths match");
            float segmentationDifference = 0.0f;
            for (size_t i = 0; i < regular.size(); ++i)
                segmentationDifference = std::max(segmentationDifference,
                        std::fabs(regular[i] - irregular[i]));
            require(segmentationDifference < 1e-5f,
                    "real HRTF audio is independent of Android callback segmentation");

            const Metrics metrics = measure(regular, rate);
            std::printf("PASS rate=%d signal=%s peak=%.6f clipSamples=%zu rmsL=%.6f rmsR=%.6f segmentationDiff=%.8f\n",
                    rate, broadband ? "broadband" : "tone", metrics.peak,
                    metrics.clippedSamples, metrics.rmsLeft, metrics.rmsRight,
                    segmentationDifference);
        }
    }
    return 0;
}
