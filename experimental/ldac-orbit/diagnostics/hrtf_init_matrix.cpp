/*
 * Steam Audio HRTF initialization matrix diagnostic.
 * Copyright 2017-2023 Valve Corporation; Apache-2.0.
 * Test harness copyright 2026 JamesDSP contributors; Apache-2.0.
 *
 * Uses the exact libphonon.so extracted from the known Stage 31 package.
 * It only creates/releases in-process Steam Audio objects and never writes
 * system or vendor files. Run from a private Termux directory.
 */
#include <phonon.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {
constexpr int kRates[] = {44100, 48000, 88200, 96000};
constexpr int kFrames[] = {128, 256, 512, 1024};

const char* statusName(IPLerror error) {
    switch (error) {
        case IPL_STATUS_SUCCESS: return "SUCCESS";
        case IPL_STATUS_FAILURE: return "FAILURE";
        case IPL_STATUS_OUTOFMEMORY: return "OUTOFMEMORY";
        case IPL_STATUS_INITIALIZATION: return "INITIALIZATION";
        default: return "UNKNOWN";
    }
}

void runOne(int rate, int frameSize, bool sofa, const char* sofaPath) {
    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;
    IPLContext context = nullptr;
    const IPLerror contextError = iplContextCreate(&contextSettings, &context);
    if (contextError != IPL_STATUS_SUCCESS || !context) {
        std::printf("rate=%d frame=%d profile=%s context=%d(%s) hrtf=SKIP\n",
                    rate, frameSize, sofa ? "sofa" : "default", contextError,
                    statusName(contextError));
        if (context) iplContextRelease(&context);
        return;
    }

    IPLAudioSettings audio{};
    audio.samplingRate = rate;
    audio.frameSize = frameSize;

    IPLHRTFSettings settings{};
    settings.type = sofa ? IPL_HRTFTYPE_SOFA : IPL_HRTFTYPE_DEFAULT;
    settings.volume = 1.0f;
    if (sofa) settings.sofaFileName = sofaPath;

    IPLHRTF hrtf = nullptr;
    const IPLerror error = iplHRTFCreate(context, &audio, &settings, &hrtf);
    std::printf("rate=%d frame=%d profile=%s context=0(SUCCESS) hrtf=%d(%s) handle=%s\n",
                rate, frameSize, sofa ? "sofa" : "default", error,
                statusName(error), hrtf ? "created" : "null");
    if (hrtf) iplHRTFRelease(&hrtf);
    iplContextRelease(&context);
}
}  // namespace

int main(int argc, char** argv) {
    const char* sofaPath = argc > 1 ? argv[1] : "/vendor/etc/axionfx/hrtf/sadie_h12.sofa";
    std::printf("Steam Audio API header version=%u.%u.%u (%u); SOFA=%s\n",
                STEAMAUDIO_VERSION_MAJOR, STEAMAUDIO_VERSION_MINOR,
                STEAMAUDIO_VERSION_PATCH, STEAMAUDIO_VERSION, sofaPath);
    for (int rate : kRates) {
        for (int frameSize : kFrames) runOne(rate, frameSize, false, nullptr);
    }
    if (access(sofaPath, R_OK) == 0) {
        std::puts("SOFA matrix follows (same rate/frame matrix; exact path readable).");
        for (int rate : kRates) {
            for (int frameSize : kFrames) runOne(rate, frameSize, true, sofaPath);
        }
    } else {
        std::printf("SOFA matrix skipped: path is not readable: %s\n", sofaPath);
    }
    return 0;
}
