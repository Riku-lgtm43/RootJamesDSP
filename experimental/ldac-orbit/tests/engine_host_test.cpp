#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <vector>
#include "jdsp_impl.h"

extern "C" int32_t i32_from_p24_little_endian(const uint8_t *packed24);
extern "C" int32_t i32_from_p24_big_endian(const uint8_t *packed24);
extern "C" void p24_from_i32_little_endian(int32_t value, uint8_t *packed24);
extern "C" void p24_from_i32_big_endian(int32_t value, uint8_t *packed24);
extern "C" void genC(unsigned int bands, unsigned int length, int8_t *coefficients);

static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static effect_config_t configFor(uint32_t rate, uint32_t format = AUDIO_FORMAT_PCM_FLOAT) {
    effect_config_t cfg{};
    constexpr uint16_t mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS |
                              EFFECT_CONFIG_FORMAT;
    cfg.inputCfg.mask = cfg.outputCfg.mask = mask;
    cfg.inputCfg.samplingRate = cfg.outputCfg.samplingRate = rate;
    cfg.inputCfg.channels = cfg.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    cfg.inputCfg.format = cfg.outputCfg.format = format;
    return cfg;
}

static int32_t setConfig(EffectDSPMain &effect, effect_config_t &cfg) {
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    const int transport = EffectDSPMainCommand(&effect, EFFECT_CMD_SET_CONFIG,
        sizeof(cfg), &cfg, &replySize, &reply);
    require(transport == 0 && replySize == sizeof(reply),
            "SET_CONFIG uses the expected transport and reply size");
    return reply;
}

static void write32(uint8_t *data, size_t offset, uint32_t value) {
    std::memcpy(data + offset, &value, sizeof(value));
}

static void setShortParameter(EffectDSPMain &effect, int32_t command, int16_t value) {
    uint8_t packet[18]{};
    const int32_t parameterSize = sizeof(command);
    const int32_t valueSize = sizeof(value);
    std::memcpy(packet + 4, &parameterSize, sizeof(parameterSize));
    std::memcpy(packet + 8, &valueSize, sizeof(valueSize));
    std::memcpy(packet + 12, &command, sizeof(command));
    std::memcpy(packet + 16, &value, sizeof(value));
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_SET_PARAM, sizeof(packet),
        packet, &replySize, &reply) == 0 && reply == 0,
        "legacy JamesDSP short parameter command succeeds");
}

static void processFloat(EffectDSPMain &effect, uint32_t rate, size_t frames,
                         uint64_t startFrame, size_t &allocFrames) {
    std::vector<float> input(frames * 2), output(frames * 2, -99.0f);
    for (size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(startFrame + i) / rate;
        input[2 * i] = 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 997.0 * t));
        input[2 * i + 1] = 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 733.0 * t));
    }
    audio_buffer_t in{};
    audio_buffer_t out{};
    in.frameCount = out.frameCount = frames;
    in.f32 = input.data();
    out.f32 = output.data();
    const auto start = std::chrono::steady_clock::now();
    const int result = EffectDSPMainProcess(&effect, &in, &out);
    const double callbackUs = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("callback rate=%u frames=%zu us=%.3f\n", rate, frames, callbackUs);
    require(result == 0, "float processing returns enabled status");
    for (float sample : output) require(std::isfinite(sample), "output has no NaN or infinity");
    if (allocFrames < effect.jdsp.blockSizeMax) allocFrames = effect.jdsp.blockSizeMax;
}

static void checkFormatRoundTrip(EffectDSPMain &effect, uint32_t format) {
    auto cfg = configFor(48000, format);
    require(setConfig(effect, cfg) == 0, "supported PCM format config succeeds");
    constexpr size_t frames = 257;
    std::vector<int32_t> input32(frames * 2), output32(frames * 2, 0);
    for (size_t i = 0; i < input32.size(); ++i) {
        const int32_t sample = static_cast<int32_t>((static_cast<int>(i % 97) - 48) * 60000);
        input32[i] = sample;
    }
    if (format == AUDIO_FORMAT_PCM_16_BIT) {
        std::vector<int16_t> input(frames * 2), output(frames * 2, -1);
        for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<int16_t>(input32[i] / 4);
        audio_buffer_t in{}; audio_buffer_t out{};
        in.frameCount = out.frameCount = frames; in.s16 = input.data(); out.s16 = output.data();
        require(EffectDSPMainProcess(&effect, &in, &out) == 0, "PCM16 process succeeds");
        for (size_t i = 0; i < input.size(); ++i)
            require(std::abs(static_cast<int>(output[i]) - static_cast<int>(input[i])) <= 1,
                    "PCM16 quantization stays within one LSB");
    } else if (format == AUDIO_FORMAT_PCM_32_BIT || format == AUDIO_FORMAT_PCM_8_24_BIT) {
        audio_buffer_t in{}; audio_buffer_t out{};
        in.frameCount = out.frameCount = frames; in.s32 = input32.data(); out.s32 = output32.data();
        require(EffectDSPMainProcess(&effect, &in, &out) == 0, "32-bit PCM process succeeds");
        for (size_t i = 0; i < input32.size(); ++i)
            require(std::abs(static_cast<int64_t>(output32[i]) - input32[i]) <= 1024,
                    "32-bit PCM float conversion stays within 1024 LSB");
    } else if (format == AUDIO_FORMAT_PCM_24_BIT_PACKED) {
        std::vector<uint8_t> input(frames * 2 * 3), output(frames * 2 * 3, 0);
        for (size_t i = 0; i < frames * 2; ++i) {
            const int32_t sample = input32[i] / 16;
            const uint32_t bits = static_cast<uint32_t>(sample);
            input[3 * i] = static_cast<uint8_t>(bits);
            input[3 * i + 1] = static_cast<uint8_t>(bits >> 8);
            input[3 * i + 2] = static_cast<uint8_t>(bits >> 16);
        }
        audio_buffer_t in{}; audio_buffer_t out{};
        in.frameCount = out.frameCount = frames; in.raw = input.data(); out.raw = output.data();
        require(EffectDSPMainProcess(&effect, &in, &out) == 0, "packed PCM24 process succeeds");
        require(output == input, "packed PCM24 bypass round-trip preserves all samples");
    }
}

static void testBypassIdentity(uint32_t rate) {
    auto *effect = static_cast<EffectDSPMain*>(std::calloc(1, sizeof(EffectDSPMain)));
    require(effect != nullptr, "allocate bypass engine state");
    EffectDSPMainConstructor(effect);
    uint32_t replySize = sizeof(int32_t); int32_t reply = -1;
    require(EffectDSPMainCommand(effect, EFFECT_CMD_ENABLE, 0, nullptr,
                                 &replySize, &reply) == 0 && reply == 0,
            "bypass effect enable succeeds");
    auto cfg = configFor(rate);
    require(setConfig(*effect, cfg) == 0, "bypass float config succeeds");
    constexpr size_t frames = 128;
    std::vector<float> input(frames * 2, 0.0f);
    std::vector<float> output(frames * 2, 0.0f);
    constexpr double pi = 3.14159265358979323846;
    for (size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        input[2 * i] = 0.12f * static_cast<float>(std::sin(2.0 * pi * 997.0 * t));
        input[2 * i + 1] = 0.09f * static_cast<float>(std::sin(2.0 * pi * 733.0 * t + 0.3));
    }
    input[0] = 0.25f;
    input[2 * 9 + 1] = -0.125f;
    audio_buffer_t in{}; audio_buffer_t out{};
    in.frameCount = out.frameCount = frames;
    in.f32 = input.data(); out.f32 = output.data();
    require(EffectDSPMainProcess(effect, &in, &out) == 0,
            "bypass process succeeds");
    double maxError = 0.0;
    double errorEnergy = 0.0;
    for (size_t i = 0; i < input.size(); ++i) {
        const double error = std::fabs(static_cast<double>(input[i]) - output[i]);
        if (error > maxError) maxError = error;
        errorEnergy += error * error;
    }
    const double rmsError = std::sqrt(errorEnergy / input.size());
    std::printf("bypass rate=%u max_abs_error=%.9g rms_error=%.9g\n",
                rate, maxError, rmsError);
    require(maxError <= 1e-7,
            "unmodified stereo float bypass is numerically transparent");
    EffectDSPMainDestructor(effect);
    std::free(effect);
}

static std::vector<float> renderStream(EffectDSPMain &effect, uint32_t rate,
                                       size_t totalFrames,
                                       const std::vector<size_t> &chunks) {
    std::vector<float> rendered(totalFrames * 2);
    size_t cursor = 0;
    size_t chunkIndex = 0;
    while (cursor < totalFrames) {
        const size_t requested = chunks[chunkIndex++ % chunks.size()];
        const size_t remaining = totalFrames - cursor;
        const size_t frames = requested < remaining ? requested : remaining;
        std::vector<float> input(frames * 2), output(frames * 2);
        for (size_t i = 0; i < frames; ++i) {
            const double t = static_cast<double>(cursor + i) / rate;
            input[2 * i] = 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 997.0 * t));
            input[2 * i + 1] = 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 733.0 * t));
        }
        audio_buffer_t in{}; audio_buffer_t out{};
        in.frameCount = out.frameCount = frames; in.f32 = input.data(); out.f32 = output.data();
        require(EffectDSPMainProcess(&effect, &in, &out) == 0,
                "segmented real-engine stream callback succeeds");
        std::copy(output.begin(), output.end(), rendered.begin() + cursor * 2);
        cursor += frames;
    }
    return rendered;
}

static void testSegmentationInvariance(uint32_t rate) {
    EffectDSPMain regular{};
    EffectDSPMain irregular{};
    EffectDSPMainConstructor(&regular);
    EffectDSPMainConstructor(&irregular);
    uint32_t replySize = sizeof(int32_t); int32_t reply = 0;
    EffectDSPMainCommand(&regular, EFFECT_CMD_ENABLE, 0, nullptr, &replySize, &reply);
    EffectDSPMainCommand(&irregular, EFFECT_CMD_ENABLE, 0, nullptr, &replySize, &reply);
    auto regularConfig = configFor(rate);
    auto irregularConfig = configFor(rate);
    require(setConfig(regular, regularConfig) == 0 && setConfig(irregular, irregularConfig) == 0,
            "segmentation comparison configs succeed");
    constexpr size_t totalFrames = 16384;
    const auto reference = renderStream(regular, rate, totalFrames, {128});
    const auto segmented = renderStream(irregular, rate, totalFrames,
        {1, 63, 96, 127, 129, 240, 480, 512, 882, 960, 1024, 2048});
    double maxError = 0.0;
    double errorEnergy = 0.0;
    for (size_t i = 0; i < reference.size(); ++i) {
        const double error = std::fabs(static_cast<double>(reference[i]) - segmented[i]);
        maxError = maxError < error ? error : maxError;
        errorEnergy += error * error;
    }
    const double rmsError = std::sqrt(errorEnergy / reference.size());
    std::printf("segmentation rate=%u max_abs_error=%.9g rms_error=%.9g\n",
                rate, maxError, rmsError);
    require(maxError < 2e-4, "callback segmentation preserves the resampled waveform");
    EffectDSPMainDestructor(&regular);
    EffectDSPMainDestructor(&irregular);
}

int main() {
    testBypassIdentity(44100);
    testBypassIdentity(48000);
    int8_t pfbCoefficients[64]{};
    genC(2, 8, pfbCoefficients);
    require(std::any_of(std::begin(pfbCoefficients), std::end(pfbCoefficients),
                        [](int8_t value) { return value < 0; }) &&
            std::all_of(std::begin(pfbCoefficients), std::end(pfbCoefficients),
                        [](int8_t value) { return value >= -2 && value <= 2; }),
            "polyphase filter design retains signed coefficients on targets with unsigned plain char");
    const uint8_t minPcm24Le[] = {0x00, 0x00, 0x80};
    const uint8_t maxPcm24Le[] = {0xff, 0xff, 0x7f};
    const uint8_t minPcm24Be[] = {0x80, 0x00, 0x00};
    const uint8_t maxPcm24Be[] = {0x7f, 0xff, 0xff};
    require(i32_from_p24_little_endian(minPcm24Le) == INT32_MIN &&
            i32_from_p24_little_endian(maxPcm24Le) == 0x7fffff00,
            "packed PCM24 sign extension preserves its signed full-scale endpoints");
    require(i32_from_p24_big_endian(minPcm24Be) == INT32_MIN &&
            i32_from_p24_big_endian(maxPcm24Be) == 0x7fffff00,
            "big-endian packed PCM24 sign extension preserves signed full scale");
    uint8_t packedRoundTrip[3]{};
    p24_from_i32_little_endian(-0x800000, packedRoundTrip);
    require(std::memcmp(packedRoundTrip, minPcm24Le, sizeof(minPcm24Le)) == 0,
            "packed PCM24 encoding preserves the negative full-scale endpoint");
    p24_from_i32_big_endian(-0x800000, packedRoundTrip);
    require(std::memcmp(packedRoundTrip, minPcm24Be, sizeof(minPcm24Be)) == 0,
            "big-endian packed PCM24 encoding preserves negative full scale");
    EffectDSPMain effect;
    std::memset(&effect, 0xa5, sizeof(effect));
    EffectDSPMainConstructor(&effect);
    require(effect.mEnable == 0 && effect.configInitialized == 0 &&
            effect.stringEq == nullptr && effect.tempImpulseIncoming == nullptr,
            "engine constructor initializes fresh caller storage deterministically");
    uint32_t replySize = sizeof(int32_t);
    int32_t reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_ENABLE, 0, nullptr,
                                 &replySize, &reply) == 0 && reply == 0,
            "effect enable succeeds");

    auto malformed = configFor(48000);
    malformed.outputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_SMP_RATE);
    require(setConfig(effect, malformed) == -EINVAL,
            "initial config missing one sample-rate side is rejected");
    require(effect.jdsp.trueSampleRate == 48000.0f && !effect.configInitialized,
            "rejected config does not mutate the live engine rate or state");
    malformed = configFor(48000);
    malformed.inputCfg.format = AUDIO_FORMAT_PCM_8_BIT;
    malformed.outputCfg.format = AUDIO_FORMAT_PCM_8_BIT;
    require(setConfig(effect, malformed) == -EINVAL,
            "unsupported PCM format is rejected");
    uint8_t malformedParam[16]{};
    write32(malformedParam, 4, 4);
    write32(malformedParam, 8, 12);
    write32(malformedParam, 12, 1500);
    replySize = sizeof(int32_t);
    reply = -1;
    require(EffectDSPMainCommand(&effect, EFFECT_CMD_SET_PARAM, sizeof(malformedParam),
        malformedParam, &replySize, &reply) == -EINVAL,
        "parameter payload shorter than declared value is rejected safely");

    for (uint32_t rate : {44100u, 48000u, 88200u, 96000u}) {
        auto cfg = configFor(rate);
        require(setConfig(effect, cfg) == 0,
                "valid config succeeds");
        if (rate == 48000) {
            auto partial = configFor(48000);
            partial.inputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_SMP_RATE);
            partial.outputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_SMP_RATE);
            require(setConfig(effect, partial) == 0 && effect.jdsp.trueSampleRate == 48000.0f,
                    "paired partial config retains the last valid sample rate");
            auto asymmetric = configFor(44100);
            asymmetric.outputCfg.mask &= static_cast<uint16_t>(~EFFECT_CONFIG_FORMAT);
            require(setConfig(effect, asymmetric) == -EINVAL &&
                    effect.jdsp.trueSampleRate == 48000.0f,
                    "one-sided partial format update is rejected without state mutation");
            auto mismatched = configFor(48000);
            mismatched.outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
            require(setConfig(effect, mismatched) == -EINVAL &&
                    effect.formatFloatModeInt32Mode == 1,
                    "mismatched input/output formats are rejected without mode mutation");
            for (uint32_t format : {AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_32_BIT,
                                    AUDIO_FORMAT_PCM_24_BIT_PACKED, AUDIO_FORMAT_PCM_8_24_BIT,
                                    AUDIO_FORMAT_PCM_FLOAT})
                checkFormatRoundTrip(effect, format);
            cfg = configFor(48000);
            require(setConfig(effect, cfg) == 0, "float config restores after PCM matrix");
            std::vector<float> inData(64 * 2, 0.125f), outData(63 * 2, -7.0f);
            audio_buffer_t in{}; audio_buffer_t out{};
            in.frameCount = 64; out.frameCount = 63;
            in.f32 = inData.data(); out.f32 = outData.data();
            require(EffectDSPMainProcess(&effect, &in, &out) == -EINVAL &&
                    std::all_of(outData.begin(), outData.end(), [](float v) { return v == -7.0f; }),
                    "mismatched frame counts are rejected without writing output");
        }
        size_t allocFrames = 0;
        uint64_t cursor = 0;
        for (size_t frames : {size_t(1), size_t(64), size_t(96), size_t(127),
                              size_t(128), size_t(129), size_t(192), size_t(240),
                              size_t(256), size_t(480), size_t(512), size_t(882),
                              size_t(960), size_t(1024), size_t(1920), size_t(2048)}) {
            processFloat(effect, rate, frames, cursor, allocFrames);
            cursor += frames;
        }
        std::printf("rate=%u frames=%llu retained_capacity=%zu asrc=%d core_rate=%.0f\n",
                    rate, static_cast<unsigned long long>(cursor), allocFrames,
                    effect.jdsp.enableASRC, effect.jdsp.fs);
    }
    for (uint32_t rate : {44100u, 48000u, 88200u, 96000u})
        testSegmentationInvariance(rate);
    auto stableRateConfig = configFor(48000);
    require(setConfig(effect, stableRateConfig) == 0,
            "crossfeed test restores 48 kHz stereo float config");
    require(!effect.jdsp.crossfeedEnabled && !effect.jdsp.advXF.conv[0] &&
            !effect.jdsp.advXF.conv[1] && !effect.jdsp.advXF.conv[2],
            "disabled crossfeed stays unallocated after repeated callback size changes");
    setShortParameter(effect, 188, 2);
    require(!effect.jdsp.advXF.conv[0] && !effect.jdsp.advXF.convLong_S_S &&
            !effect.jdsp.advXF.convLong_T_S,
            "selecting advanced crossfeed mode does not build it while disabled");
    setShortParameter(effect, 1208, 1);
    require(effect.jdsp.crossfeedEnabled && effect.jdsp.advXF.conv[0] &&
            effect.jdsp.advXF.conv[1] && effect.jdsp.advXF.conv[2],
            "advanced crossfeed initializes when the user enables it");
    size_t retainedCapacity = effect.jdsp.blockSizeMax;
    processFloat(effect, 48000, 128, 0, retainedCapacity);
    setShortParameter(effect, 1208, 0);
    require(!effect.jdsp.crossfeedEnabled,
            "advanced crossfeed disables cleanly after processing");
    setShortParameter(effect, 137, 50);
    setShortParameter(effect, 1204, 1);
    processFloat(effect, 48000, 128, 128, retainedCapacity);
    setShortParameter(effect, 1204, 0);
    require(!effect.jdsp.sterEnhEnabled,
            "stereo enhancement disables cleanly after the active PFB processing path");
    EffectDSPMainDestructor(&effect);
    std::puts("PASS real JamesDSP engine host stream matrix");
}
