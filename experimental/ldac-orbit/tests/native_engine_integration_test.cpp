#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "essential.h"

extern "C" int32_t EffectCreate(const effect_uuid_t *, int32_t, int32_t, effect_handle_t *);
extern "C" int32_t EffectRelease(effect_handle_t);

namespace {
void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int32_t command(effect_handle_t effect, uint32_t code, uint32_t size,
                void *data, uint32_t *replySize, void *reply) {
    return (*effect)->command(effect, code, size, data, replySize, reply);
}

void setOrbit(effect_handle_t effect, bool enabled) {
    const uint32_t ids[] = {0x1001, 0x1003, 0x1000};
    const int32_t values[] = {150, 70, enabled ? 1 : 0};
    for (size_t i = 0; i < 3; ++i) {
        uint8_t packet[20]{};
        const int32_t psize = 4, vsize = 4;
        std::memcpy(packet + 4, &psize, sizeof(psize));
        std::memcpy(packet + 8, &vsize, sizeof(vsize));
        std::memcpy(packet + 12, &ids[i], sizeof(ids[i]));
        std::memcpy(packet + 16, &values[i], sizeof(values[i]));
        uint32_t replySize = sizeof(int32_t);
        int32_t reply = -1;
        require(command(effect, EFFECT_CMD_SET_PARAM, sizeof(packet), packet,
                        &replySize, &reply) == 0 && replySize == sizeof(reply) && reply == 0,
                "Orbit command reaches the live native effect");
    }
}

effect_handle_t createEffect(bool orbitEnabled, uint32_t rate) {
    const effect_uuid_t implementation = {
        0xf27317f4, 0xc984, 0x4de6, 0x9a90,
        {0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2}
    };
    effect_handle_t effect = nullptr;
    require(EffectCreate(&implementation, 11, 0, &effect) == 0 && effect,
            "effect wrapper creates a real JamesDSP engine instance");
    setOrbit(effect, orbitEnabled);

    effect_config_t config{};
    const uint16_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT;
    config.inputCfg.mask = config.outputCfg.mask = mask;
    config.inputCfg.samplingRate = config.outputCfg.samplingRate = rate;
    config.inputCfg.channels = config.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    config.inputCfg.format = config.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    require(command(effect, EFFECT_CMD_SET_CONFIG, sizeof(config), &config,
                    &replySize, &reply) == 0 && reply == 0,
            "real engine accepts the wrapper's stereo float route");
    replySize = sizeof(reply);
    reply = -1;
    require(command(effect, EFFECT_CMD_ENABLE, 0, nullptr, &replySize, &reply) == 0 && reply == 0,
            "real engine is enabled through the AudioEffect command path");
    return effect;
}

std::vector<float> render(effect_handle_t effect, uint32_t rate, size_t frames) {
    std::vector<float> input(frames * 2), output(frames * 2, 0.0f);
    for (size_t i = 0; i < frames; ++i) {
        const double time = static_cast<double>(i) / rate;
        input[2 * i] = 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 997.0 * time));
        input[2 * i + 1] = 0.13f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 733.0 * time));
    }
    audio_buffer_t in{}, out{};
    in.frameCount = out.frameCount = frames;
    in.f32 = input.data();
    out.f32 = output.data();
    require((*effect)->process(effect, &in, &out) == 0,
            "AudioEffect process path runs the core DSP and Orbit stage");
    for (float sample : output)
        require(std::isfinite(sample), "full native output is finite");
    return output;
}

effect_config_t routeConfig(uint32_t inputRate, uint32_t outputRate) {
    effect_config_t config{};
    const uint16_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS |
                          EFFECT_CONFIG_FORMAT;
    config.inputCfg.mask = config.outputCfg.mask = mask;
    config.inputCfg.samplingRate = inputRate;
    config.outputCfg.samplingRate = outputRate;
    config.inputCfg.channels = config.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    config.inputCfg.format = config.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
    return config;
}
}

int main() {
    for (uint32_t rate : {44100u, 48000u, 88200u, 96000u}) {
        effect_handle_t plain = createEffect(false, rate);
        effect_handle_t orbit = createEffect(true, rate);
        const auto plainOutput = render(plain, rate, 2048);
        const auto orbitOutput = render(orbit, rate, 2048);
        double maxDelta = 0.0;
        for (size_t i = 0; i < plainOutput.size(); ++i)
            maxDelta = std::max(maxDelta,
                std::fabs(static_cast<double>(plainOutput[i]) - orbitOutput[i]));
        std::printf("wrapper rate=%u orbit_max_delta=%.9g\n", rate, maxDelta);
        require(maxDelta > 1e-5,
                "enabled Orbit post-processes the real JamesDSP output at every tested rate");
        auto rejected = routeConfig(rate, rate == 44100 ? 48000 : 44100);
        uint32_t replySize = sizeof(int32_t);
        int32_t reply = 0;
        require(command(orbit, EFFECT_CMD_SET_CONFIG, sizeof(rejected), &rejected,
                        &replySize, &reply) == 0 && reply == -EINVAL,
                "real engine rejects a mismatched input/output route");
        const auto plainAfterReject = render(plain, rate, 2048);
        const auto orbitAfterReject = render(orbit, rate, 2048);
        double rejectedRouteDelta = 0.0;
        for (size_t i = 0; i < plainAfterReject.size(); ++i)
            rejectedRouteDelta = std::max(rejectedRouteDelta,
                std::fabs(static_cast<double>(plainAfterReject[i]) - orbitAfterReject[i]));
        require(rejectedRouteDelta < 1e-6,
                "Orbit bypasses audio after a rejected route while JamesDSP keeps its last valid config");
        require(EffectRelease(orbit) == 0 && EffectRelease(plain) == 0,
                "effect instances release cleanly");
    }
    std::puts("PASS real JamesDSP and Orbit AudioEffect connection matrix (Steam Audio API mocked)");
    return 0;
}
