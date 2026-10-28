#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stddef.h>
#include <vector>
#include "essential.h"

extern "C" int32_t EffectCreate(const effect_uuid_t*, int32_t, int32_t, effect_handle_t*);
extern "C" int32_t EffectRelease(effect_handle_t);
extern "C" void testSetConfigReplyStatus(int32_t);
extern "C" void testSetProcessStatus(int32_t);

namespace {
void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void put32(uint8_t* bytes, size_t offset, int32_t value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}

int32_t get32(const uint8_t* bytes, size_t offset) {
    int32_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

int32_t command(effect_handle_t effect, uint32_t code, uint32_t size,
                void* data, uint32_t* replySize, void* reply) {
    return (*effect)->command(effect, code, size, data, replySize, reply);
}

int32_t process(effect_handle_t effect, const std::vector<float>& input,
                std::vector<float>& output) {
    audio_buffer_t in{};
    audio_buffer_t out{};
    in.frameCount = input.size() / 2;
    in.f32 = const_cast<float*>(input.data());
    out.frameCount = output.size() / 2;
    out.f32 = output.data();
    return (*effect)->process(effect, &in, &out);
}

effect_config_t validConfig() {
    effect_config_t config{};
    const uint16_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS |
                          EFFECT_CONFIG_FORMAT;
    config.inputCfg.mask = mask;
    config.outputCfg.mask = mask;
    config.inputCfg.samplingRate = config.outputCfg.samplingRate = 48000;
    config.inputCfg.channels = config.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    config.inputCfg.format = config.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
    return config;
}

void setOrbitParam(effect_handle_t effect, int32_t id, int32_t value) {
    uint8_t packet[20]{};
    put32(packet, 4, 4); // psize
    put32(packet, 8, 4); // vsize
    put32(packet, 12, id);
    put32(packet, 16, value);
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -999;
    const int32_t result = command(effect, EFFECT_CMD_SET_PARAM, sizeof(packet),
                                   packet, &replySize, &reply);
    require(result == 0 && replySize == sizeof(int32_t) && reply == 0,
            "Orbit SET_PARAM reaches native bridge and returns success");
}

int32_t getOrbitParam(effect_handle_t effect, int32_t id) {
    uint8_t request[16]{};
    put32(request, 4, 4); // psize
    put32(request, 8, 4); // vsize expected by the legacy JamesDSP ABI
    put32(request, 12, id);
    uint8_t reply[20]{};
    uint32_t replySize = sizeof(reply);
    const int32_t result = command(effect, EFFECT_CMD_GET_PARAM, sizeof(request),
                                   request, &replySize, reply);
    require(result == 0 && replySize == sizeof(reply),
            "Orbit GET_PARAM returns the full legacy effect_param_t response");
    require(get32(reply, 0) == 0 && get32(reply, 4) == 4 &&
            get32(reply, 8) == 4 && get32(reply, 12) == id,
            "Orbit GET_PARAM response header is intact");
    return get32(reply, 16);
}

void expectBypassAfterConfig(effect_handle_t effect, effect_config_t config,
                             const std::vector<float>& input, const char* message) {
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    const int32_t status = command(effect, EFFECT_CMD_SET_CONFIG, sizeof(config),
                                   &config, &replySize, &reply);
    require(status == 0 && reply == 0, "test JamesDSP accepts synthetic config");
    std::vector<float> output(input.size());
    require(process(effect, input, output) == 0 && output == input, message);
}

std::vector<float> signal(size_t frames) {
    std::vector<float> samples(frames * 2);
    constexpr float pi = 3.14159265358979323846f;
    for (size_t frame = 0; frame < frames; ++frame) {
        samples[2 * frame] = 0.2f * std::sin(2.0f * pi * 997.0f * frame / 48000.0f);
        samples[2 * frame + 1] = 0.13f * std::sin(2.0f * pi * 733.0f * frame / 48000.0f);
    }
    return samples;
}
}

int main() {
    const effect_uuid_t implementation = {
        0xf27317f4, 0xc984, 0x4de6, 0x9a90,
        {0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2}
    };
    effect_handle_t effect = nullptr;
    require(EffectCreate(&implementation, 7, 0, &effect) == 0 && effect,
            "JamesDSP effect instance is created");
    setOrbitParam(effect, 0x1001, 225);
    require(getOrbitParam(effect, 0x1001) == 200,
            "Orbit parameter values are clamped and read back");
    setOrbitParam(effect, 0x1003, 70);
    setOrbitParam(effect, 0x1000, 1);

    auto config = validConfig();
    uint32_t replySize = sizeof(int32_t);
    int32_t configReply = 0;
    testSetConfigReplyStatus(-EINVAL);
    require(command(effect, EFFECT_CMD_SET_CONFIG, sizeof(config), &config,
                    &replySize, &configReply) == 0 && configReply == -EINVAL,
            "JamesDSP config rejection is reported in the command reply");
    const auto input = signal(1024);
    std::vector<float> output(input.size());
    require(process(effect, input, output) == 0 && output == input,
            "Orbit remains bypassed when JamesDSP rejects SET_CONFIG");

    testSetConfigReplyStatus(0);
    replySize = sizeof(int32_t);
    configReply = -1;
    require(command(effect, EFFECT_CMD_SET_CONFIG, sizeof(config), &config,
                    &replySize, &configReply) == 0 && configReply == 0,
            "valid stereo-float JamesDSP configuration is accepted");
    require(process(effect, input, output) == 0,
            "JamesDSP processing succeeds before Orbit post-processing");
    bool changed = false;
    for (size_t i = 0; i < output.size(); ++i) {
        require(std::isfinite(output[i]), "Orbit output remains finite");
        changed = changed || std::fabs(output[i] - input[i]) > 1e-5f;
    }
    require(changed, "enabled Orbit modifies JamesDSP's processed output");

    config = validConfig();
    config.inputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_FORMAT);
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed when input format is absent from config mask");

    config = validConfig();
    config.outputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_CHANNELS);
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed when output channels are absent from config mask");

    config = validConfig();
    config.outputCfg.samplingRate = 44100;
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed for mismatched input/output rates");

    config = validConfig();
    config.inputCfg.channels = AUDIO_CHANNEL_OUT_MONO;
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed for a mono input route");

    config = validConfig();
    config.outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed for a non-float output route");

    config = validConfig();
    config.inputCfg.samplingRate = config.outputCfg.samplingRate = 0;
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed when the configured sample rate is zero");

    config = validConfig();
    config.outputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_SMP_RATE);
    expectBypassAfterConfig(effect, config, input,
        "Orbit stays bypassed when output sample rate is absent from config mask");

    config = validConfig();
    replySize = sizeof(int32_t);
    require(command(effect, EFFECT_CMD_SET_CONFIG, sizeof(config), &config,
                    &replySize, &configReply) == 0,
            "valid config restores the effect path");
    testSetProcessStatus(-ENODATA);
    require(process(effect, input, output) == -ENODATA && output == input,
            "Orbit does not process after JamesDSP reports a disabled effect");
    testSetProcessStatus(0);

    require(EffectRelease(effect) == 0, "JamesDSP effect instance is released");
    std::puts("PASS JamesDSP effect connection, Orbit parameters, config guards, and post-DSP routing");
    return 0;
}
