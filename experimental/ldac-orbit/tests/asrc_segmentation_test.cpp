#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "jdsp_impl.h"

static void require(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
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

static std::vector<float> render(uint32_t rate, size_t totalFrames,
                                const std::vector<size_t>& sizes) {
    constexpr double pi = 3.14159265358979323846;
    EffectDSPMain effect{};
    EffectDSPMainConstructor(&effect);
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_ENABLE, 0, nullptr,
                                 &replySize, &reply) == 0 && reply == 0,
            "enable succeeds");
    auto cfg = configFor(rate);
    replySize = sizeof(reply); reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_SET_CONFIG, sizeof(cfg),
                                 &cfg, &replySize, &reply) == 0 && reply == 0,
            "stereo float configuration succeeds");

    std::vector<float> result(totalFrames * 2);
    size_t cursor = 0;
    size_t sizeIndex = 0;
    while (cursor < totalFrames) {
        const size_t requested = sizes[sizeIndex++ % sizes.size()];
        const size_t remaining = totalFrames - cursor;
        const size_t frames = requested < remaining ? requested : remaining;
        std::vector<float> in(frames * 2), out(frames * 2);
        for (size_t i = 0; i < frames; ++i) {
            const double t = static_cast<double>(cursor + i) / rate;
            in[2 * i] = 0.17f * static_cast<float>(std::sin(2.0 * pi * 997.0 * t));
            in[2 * i + 1] = 0.11f * static_cast<float>(std::sin(2.0 * pi * 733.0 * t + 0.2));
        }
        audio_buffer_t input{}; audio_buffer_t output{};
        input.frameCount = output.frameCount = frames;
        input.f32 = in.data(); output.f32 = out.data();
        require(EffectDSPMainProcess(&effect, &input, &output) == 0,
                "process callback succeeds");
        std::copy(out.begin(), out.end(), result.begin() + cursor * 2);
        cursor += frames;
    }
    EffectDSPMainDestructor(&effect);
    return result;
}

int main(int argc, char** argv) {
    require(argc == 2, "pass expected mode: equivalent or divergence");
    const std::string expected = argv[1];
    constexpr size_t totalFrames = 16384;
    const std::vector<size_t> fixed{128};
    const std::vector<size_t> irregular{1, 63, 96, 127, 129, 240, 480, 512,
                                        882, 960, 1024, 2048};
    bool sawDivergence = false;
    for (uint32_t rate : {44100u, 48000u, 88200u, 96000u}) {
        const auto reference = render(rate, totalFrames, fixed);
        const auto segmented = render(rate, totalFrames, irregular);
        double maxError = 0.0;
        double errorEnergy = 0.0;
        for (size_t i = 0; i < reference.size(); ++i) {
            const double error = std::fabs(static_cast<double>(reference[i]) - segmented[i]);
            if (error > maxError) maxError = error;
            errorEnergy += error * error;
            require(std::isfinite(segmented[i]), "output remains finite");
        }
        const double rmsError = std::sqrt(errorEnergy / reference.size());
        std::printf("rate=%u max_abs_diff=%.9g rms_diff=%.9g\n", rate,
                    maxError, rmsError);
        sawDivergence |= maxError > 2e-4;
    }
    if (expected == "equivalent")
        require(!sawDivergence, "callback segmentation matches for patched engine");
    else if (expected == "divergence")
        require(sawDivergence, "pristine engine reproduces segmentation divergence");
    else
        require(false, "expected mode is equivalent or divergence");
    return 0;
}
