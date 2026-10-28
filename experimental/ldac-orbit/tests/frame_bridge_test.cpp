#include "SteamSpatial.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

extern "C" void mockSetFailHrtf(bool fail);
extern "C" void mockResetHrtfCreateCalls();
extern "C" int mockHrtfCreateCalls();
extern "C" int mockLastRate();
extern "C" IPLHRTFType mockLastHrtfType();
extern "C" IPLHRTFNormType mockLastHrtfNormType();
extern "C" const char* mockLastSofaPath();
extern "C" void mockSetBinauralGain(float gain);

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
    require(mockLastHrtfNormType() == IPL_HRTFNORMTYPE_RMS,
            "HRTF loudness is normalized across directions");
    if (rate == 88200 || rate == 96000) {
        require(mockLastHrtfType() == IPL_HRTFTYPE_SOFA, "SOFA selected above embedded default rates");
        require(std::strcmp(mockLastSofaPath(), "/vendor/etc/axionfx/hrtf/cipic_124.sofa") == 0,
                "bundled CIPIC SOFA path selected at high rates");
    } else {
        require(mockLastHrtfType() == IPL_HRTFTYPE_DEFAULT, "embedded default retained at 44.1/48 kHz");
    }
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
    for (int rate : {44100, 48000, 88200, 96000}) {
        auto a = render(rate, oneBlock);
        auto b = render(rate, irregular);
        require(a.size() == b.size(), "render lengths match");
        float maxDiff = 0;
        for (size_t i = 0; i < a.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(a[i] - b[i]));
        require(maxDiff < 1e-6f, "callback segmentation does not change rendered samples");
        std::printf("PASS rate=%d callbacks=128 and irregular maxSegmentationDiff=%g\n", rate, maxDiff);
    }

    // Model a high-gain HRTF response and verify that Orbit keeps the stereo
    // block below the float output ceiling without clipping individual samples.
    mockSetBinauralGain(5.0f);
    axionfx::SteamSpatial limiter;
    limiter.configure(48000);
    limiter.setBlend(100);
    limiter.setBassPreservation(0);
    limiter.setEnabled(true);
    std::vector<float> hotSignal(48000 * 2);
    constexpr float limiterPi = 3.14159265358979323846f;
    for (int i = 0; i < 48000; ++i) {
        const float sample = 0.80f * std::sin(2.0f * limiterPi * 997.0f * i / 48000.0f);
        hotSignal[2 * i] = sample;
        hotSignal[2 * i + 1] = sample;
    }
    for (int pos = 0; pos < 48000;) {
        const int count = std::min(1920, 48000 - pos);
        limiter.process(hotSignal.data() + 2 * pos, count);
        pos += count;
    }
    float limitedPeak = 0.0f;
    for (float sample : hotSignal) {
        require(std::isfinite(sample), "high-gain HRTF output remains finite");
        limitedPeak = std::max(limitedPeak, std::fabs(sample));
    }
    std::printf("METRICS syntheticHighGainPeak=%g\n", limitedPeak);
    require(limitedPeak <= 0.95001f, "linked Orbit limiter preserves float output headroom");
    require(limitedPeak > 0.80f, "peak control preserves audible signal level");
    mockSetBinauralGain(1.0f);
    std::puts("PASS linked peak control synthetic high-gain signal");

    mockSetBinauralGain(std::numeric_limits<float>::quiet_NaN());
    axionfx::SteamSpatial finiteGuard;
    finiteGuard.configure(48000);
    finiteGuard.setBlend(100);
    finiteGuard.setBassPreservation(0);
    finiteGuard.setEnabled(true);
    std::vector<float> invalidHrtfInput(1024, 0.125f);
    finiteGuard.process(invalidHrtfInput.data(), 512);
    for (float sample : invalidHrtfInput)
        require(std::isfinite(sample), "non-finite HRTF output falls back to dry audio");
    mockSetBinauralGain(1.0f);
    constexpr float recoveryPi = 3.14159265358979323846f;
    std::vector<float> recoveryInput(4096 * 2), recoveryOutput;
    for (int i = 0; i < 4096; ++i) {
        const float sample = 0.20f * std::sin(2.0f * recoveryPi * 997.0f * i / 48000.0f);
        recoveryInput[2 * i] = recoveryInput[2 * i + 1] = sample;
    }
    recoveryOutput = recoveryInput;
    finiteGuard.process(recoveryOutput.data(), 4096);
    float recoveryChange = 0.0f;
    for (int i = 256; i < 4096; ++i) {
        require(std::isfinite(recoveryOutput[2 * i]) && std::isfinite(recoveryOutput[2 * i + 1]),
                "finite HRTF output recovers after a non-finite block");
        recoveryChange += std::fabs(recoveryOutput[2 * i] - recoveryInput[2 * (i - 128)]) +
                          std::fabs(recoveryOutput[2 * i + 1] - recoveryInput[2 * (i - 128) + 1]);
    }
    require(recoveryChange > 0.1f, "HRTF filter state recovers after a non-finite block");
    finiteGuard.setEnabled(false);
    std::puts("PASS non-finite HRTF fallback and recovery");

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

    spatial.configure(96000);
    mockSetFailHrtf(true);
    mockResetHrtfCreateCalls();
    spatial.setEnabled(true);
    require(!spatial.isInitialized(), "failed high-rate SOFA init stays disabled");
    require(mockHrtfCreateCalls() == 1 && mockLastHrtfType() == IPL_HRTFTYPE_SOFA,
            "high-rate SOFA failure does not retry known-unsupported default HRTF");
    std::vector<float> highRateDry(512, -0.0625f), highRateOriginal = highRateDry;
    spatial.process(highRateDry.data(), 256);
    require(highRateDry == highRateOriginal, "high-rate HRTF failure leaves input unchanged");
    spatial.setEnabled(false);
    mockSetFailHrtf(false);
    spatial.setEnabled(true);
    require(spatial.isInitialized(), "high-rate SOFA init can recover after retry");
    spatial.setEnabled(false);
    std::puts("PASS high-rate init failure bypass and recovery");

    spatial.configure(48000);
    spatial.setEnabled(true);
    std::vector<float> startupPrime(256, 0.125f);
    spatial.process(startupPrime.data(), 128);
    for (float x : startupPrime) require(x == 0.125f, "bridge startup remains dry until first full block");
    spatial.setEnabled(false);
    std::vector<float> immediateDry(256, 0.125f);
    spatial.process(immediateDry.data(), 128);
    for (float x : immediateDry) require(x == 0.125f, "disable before first wet frame stays dry");
    std::puts("PASS disable before first bridge wet frame");

    spatial.configure(48000);
    spatial.setEnabled(true);
    std::vector<float> transitionPrime(256, 0.125f);
    spatial.process(transitionPrime.data(), 128);
    std::vector<float> fadeIn(128, 0.125f);
    spatial.process(fadeIn.data(), 64);
    spatial.setEnabled(false);
    std::vector<float> fadeOut(128, 0.125f);
    spatial.process(fadeOut.data(), 64);
    const float toggleStepL = std::fabs(fadeOut[0] - fadeIn[126]);
    const float toggleStepR = std::fabs(fadeOut[1] - fadeIn[127]);
    require(toggleStepL < 0.02f && toggleStepR < 0.02f,
            "disable during startup fade does not jump to full wet level");
    std::puts("PASS disable during startup fade continuity");

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
    std::vector<float> afterReset(128, 0.125f);
    spatial.process(afterReset.data(), 64);
    for (float x : afterReset) require(x == 0.125f, "reset clears buffered wet output");
    spatial.setEnabled(false);
    std::puts("PASS disable transition, reset bridge state, and rate reconfigure");
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
