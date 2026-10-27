#include "SteamSpatial.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" void mockSetFailHrtf(bool fail);
extern "C" int mockLastRate();

static void require(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

static std::vector<float> render(int rate, const std::vector<int>& chunks) {
    constexpr float pi = 3.14159265358979323846f;
    const int frames = rate / 4;
    std::vector<float> input(static_cast<size_t>(frames) * 2);
    for (int i = 0; i < frames; ++i) {
        input[2 * i] = 0.22f * std::sin(2.0f * pi * 1000.0f * i / rate) + 0.03f * std::sin(2.0f * pi * 237.0f * i / rate);
        input[2 * i + 1] = 0.13f * std::sin(2.0f * pi * 700.0f * i / rate + 0.3f);
    }
    axionfx::SteamSpatial spatial;
    spatial.configure(rate);
    spatial.setBassPreservation(0);
    spatial.setEnabled(true);
    require(spatial.isInitialized(), "mock Steam Audio init");
    require(mockLastRate() == rate, "sample rate passed to HRTF");
    std::vector<float> output(input.size());
    int pos = 0, n = 0;
    while (pos < frames) {
        int count = std::min(chunks[n++ % chunks.size()], frames - pos);
        // Guard words around every callback detect writes beyond the provided frame span.
        std::vector<float> guarded(static_cast<size_t>(count) * 2 + 4, 77.0f);
        std::copy_n(input.data() + 2 * pos, count * 2, guarded.data() + 2);
        spatial.process(guarded.data() + 2, count);
        require(guarded[0] == 77.0f && guarded[1] == 77.0f &&
                guarded[guarded.size() - 1] == 77.0f && guarded[guarded.size() - 2] == 77.0f,
                "callback canaries unchanged");
        std::copy_n(guarded.data() + 2, count * 2, output.data() + 2 * pos);
        pos += count;
    }
    for (float x : output) require(std::isfinite(x), "finite output");
    float leftEnergy = 0, rightEnergy = 0, stereoDifference = 0, wetDifference = 0;
    float peak = 0, maxStep = 0;
    for (int i = 256; i < frames; ++i) {
        float l = output[2 * i], r = output[2 * i + 1];
        leftEnergy += l * l; rightEnergy += r * r;
        stereoDifference += (l - r) * (l - r);
        wetDifference += std::fabs(l - input[2 * (i - 128)]) + std::fabs(r - input[2 * (i - 128) + 1]);
        peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
        maxStep = std::max(maxStep, std::max(std::fabs(l - output[2 * (i - 1)]), std::fabs(r - output[2 * (i - 1) + 1])));
    }
    for (int start = 256; start + 128 <= frames; start += 128) {
        float blockDelta = 0.0f;
        for (int i = start; i < start + 128; ++i)
            blockDelta += std::fabs(output[2 * i] - input[2 * (i - 128)]) +
                          std::fabs(output[2 * i + 1] - input[2 * (i - 128) + 1]);
        require(blockDelta > 0.01f, "no repeated dry callback-sized block after bridge prime");
    }
    require(leftEnergy > 0.01f && rightEnergy > 0.01f, "both stereo channels contain signal");
    require(stereoDifference > 0.01f, "stereo remains distinct");
    require(wetDifference > 0.1f, "spatial processing produces nonzero change");
    require(peak < 1.0f, "peak remains bounded below full scale for test signal");
    require(maxStep < 0.2f, "no large sample discontinuity");
    const float count = static_cast<float>(frames - 256);
    std::printf("METRICS rate=%d peak=%g maxStep=%g rmsL=%g rmsR=%g stereoDeltaRms=%g alignedChangeMean=%g\n",
                rate, peak, maxStep, std::sqrt(leftEnergy / count), std::sqrt(rightEnergy / count),
                std::sqrt(stereoDifference / count), wetDifference / (2.0f * count));
    return output;
}

int main() {
    const std::vector<int> oneBlock{128};
    const std::vector<int> irregular{1,64,96,127,128,129,192,240,256,480,512,882,960,1024,1920,2048};
    for (int rate : {44100, 48000, 96000}) {
        auto a = render(rate, oneBlock);
        auto b = render(rate, irregular);
        require(a.size() == b.size(), "render lengths match");
        float maxDiff = 0;
        for (size_t i = 0; i < a.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(a[i] - b[i]));
        require(maxDiff < 1e-6f, "callback segmentation does not change rendered samples");
        std::printf("PASS rate=%d callbacks=128 and irregular maxSegmentationDiff=%g\n", rate, maxDiff);
    }

    axionfx::SteamSpatial spatial;
    spatial.configure(48000);
    mockSetFailHrtf(true);
    spatial.setEnabled(true);
    require(!spatial.isInitialized(), "failed init remains bypass-safe");
    std::vector<float> dry(512, 0.125f), original = dry;
    spatial.process(dry.data(), 256);
    require(dry == original, "failed init leaves input unchanged");
    spatial.setEnabled(false);
    mockSetFailHrtf(false);
    spatial.setEnabled(true);
    require(spatial.isInitialized(), "failed init can recover after retry");
    spatial.setEnabled(false);
    std::vector<float> bypass(512, -0.2f), bypassOriginal = bypass;
    spatial.process(bypass.data(), 256);
    require(bypass == bypassOriginal, "Orbit OFF is bit-exact bypass");
    std::puts("PASS init failure bypass and recovery");

    spatial.setEnabled(true);
    std::vector<float> signal(4096, 0.1f);
    spatial.process(signal.data(), 513);
    spatial.setEnabled(false);
    spatial.process(signal.data() + 1026, 1024);
    for (float x : signal) require(std::isfinite(x), "finite during rapid disable transition");
    spatial.configure(96000);
    spatial.setEnabled(true);
    require(spatial.isInitialized() && mockLastRate() == 96000, "rate reconfigure updates Steam Audio");
    spatial.reset();
    spatial.setEnabled(false);
    std::puts("PASS disable transition, reset, and rate reconfigure");
    for (int i = 0; i < 24; ++i) {
        spatial.setEnabled((i & 1) == 0);
        std::vector<float> block(static_cast<size_t>((i % 5 + 1) * 127) * 2, 0.04f);
        spatial.process(block.data(), static_cast<int>(block.size() / 2));
        for (float x : block) require(std::isfinite(x), "finite during repeated enable/disable");
    }
    spatial.setEnabled(false);
    std::puts("PASS 24 repeated enable/disable transitions");
    return 0;
}
