#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "jdsp_impl.h"

static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static effect_config_t configFor(uint32_t rate) {
    effect_config_t cfg{};
    constexpr uint16_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS |
                              EFFECT_CONFIG_FORMAT;
    cfg.inputCfg.mask = cfg.outputCfg.mask = mask;
    cfg.inputCfg.samplingRate = cfg.outputCfg.samplingRate = rate;
    cfg.inputCfg.channels = cfg.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    cfg.inputCfg.format = cfg.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
    return cfg;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s sample-rate iterations\n", argv[0]);
        return 2;
    }
    const uint32_t rate = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10));
    const size_t iterations = static_cast<size_t>(std::strtoull(argv[2], nullptr, 10));
    require(rate > 0 && iterations > 0, "benchmark inputs are positive");
    constexpr std::array<size_t, 16> frameSizes = {
        1, 64, 96, 127, 128, 129, 192, 240,
        256, 480, 512, 882, 960, 1024, 1920, 2048
    };

    EffectDSPMain effect{};
    EffectDSPMainConstructor(&effect);
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_ENABLE, 0, nullptr,
                                 &replySize, &reply) == 0 && reply == 0,
            "effect enable succeeds");
    auto cfg = configFor(rate);
    replySize = sizeof(reply); reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_SET_CONFIG, sizeof(cfg), &cfg,
                                 &replySize, &reply) == 0 && reply == 0,
            "benchmark config succeeds");

    std::array<std::vector<float>, frameSizes.size()> inputs;
    std::array<std::vector<float>, frameSizes.size()> outputs;
    std::array<audio_buffer_t, frameSizes.size()> inputBuffers{};
    std::array<audio_buffer_t, frameSizes.size()> outputBuffers{};
    for (size_t index = 0; index < frameSizes.size(); ++index) {
        const size_t frames = frameSizes[index];
        inputs[index].resize(frames * 2);
        outputs[index].resize(frames * 2);
        for (size_t frame = 0; frame < frames; ++frame) {
            const double t = static_cast<double>(frame) / rate;
            inputs[index][2 * frame] = 0.2f * static_cast<float>(
                std::sin(2.0 * 3.141592653589793 * 997.0 * t));
            inputs[index][2 * frame + 1] = 0.2f * static_cast<float>(
                std::sin(2.0 * 3.141592653589793 * 733.0 * t));
        }
        inputBuffers[index].frameCount = outputBuffers[index].frameCount = frames;
        inputBuffers[index].f32 = inputs[index].data();
        outputBuffers[index].f32 = outputs[index].data();
    }

    auto process = [&](size_t index) {
        require(EffectDSPMainProcess(&effect, &inputBuffers[index],
                                     &outputBuffers[index]) == 0,
                "benchmark callback succeeds");
    };
    for (size_t cycle = 0; cycle < 4; ++cycle)
        for (size_t index = 0; index < frameSizes.size(); ++index)
            process(index);

    std::array<std::vector<double>, frameSizes.size()> samples;
    for (auto &sampleList : samples) sampleList.reserve(iterations);
    for (size_t cycle = 0; cycle < iterations; ++cycle) {
        for (size_t index = 0; index < frameSizes.size(); ++index) {
            const auto begin = std::chrono::steady_clock::now();
            process(index);
            samples[index].push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - begin).count());
        }
    }
    std::puts("rate,frames,mean_us,p50_us,p95_us,p99_us,max_us");
    for (size_t index = 0; index < frameSizes.size(); ++index) {
        auto &times = samples[index];
        std::sort(times.begin(), times.end());
        const size_t p50 = (times.size() - 1) * 50 / 100;
        const size_t p95 = (times.size() - 1) * 95 / 100;
        const size_t p99 = (times.size() - 1) * 99 / 100;
        double mean = 0.0;
        for (double sample : times) mean += sample;
        mean /= static_cast<double>(times.size());
        std::printf("%u,%zu,%.3f,%.3f,%.3f,%.3f,%.3f\n", rate,
                    frameSizes[index], mean, times[p50], times[p95],
                    times[p99], times.back());
    }
    EffectDSPMainDestructor(&effect);
    return 0;
}
