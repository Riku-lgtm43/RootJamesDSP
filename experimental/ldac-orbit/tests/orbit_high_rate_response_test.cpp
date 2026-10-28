#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "SteamSpatial.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

double fundamental(const std::vector<float>& signal, int channel, int rate,
                   double frequency, int start, int end) {
    double real = 0.0;
    double imag = 0.0;
    for (int frame = start; frame < end; ++frame) {
        const double angle = 2.0 * kPi * frequency * frame / rate;
        const double sample = signal[2 * frame + channel];
        real += sample * std::cos(angle);
        imag -= sample * std::sin(angle);
    }
    return 2.0 * std::hypot(real, imag) / static_cast<double>(end - start);
}

std::array<double, 2> render(int rate, double frequency, int sourceChannel) {
    constexpr float kInputAmplitude = 0.05f;
    const int frames = rate / 4;
    std::vector<float> signal(static_cast<size_t>(frames) * 2, 0.0f);
    for (int frame = 0; frame < frames; ++frame) {
        signal[2 * frame + sourceChannel] = kInputAmplitude *
            static_cast<float>(std::sin(2.0 * kPi * frequency * frame / rate));
    }

    axionfx::SteamSpatial spatial;
    spatial.configure(rate);
    spatial.setWidth(100);
    spatial.setBlend(70);
    spatial.setBassPreservation(100);
    spatial.setOrbitSeconds(60);
    spatial.setEnabled(true);
    if (!spatial.isInitialized()) {
        std::fprintf(stderr, "FAIL HRTF initialization at %d Hz\n", rate);
        std::exit(1);
    }

    for (int offset = 0; offset < frames; offset += 1920) {
        spatial.process(signal.data() + 2 * offset, std::min(1920, frames - offset));
    }

    const int start = rate / 10;
    std::array<double, 2> response{};
    for (int channel = 0; channel < 2; ++channel) {
        const double output = fundamental(signal, channel, rate, frequency, start, frames);
        response[channel] = 20.0 * std::log10(std::max(output, 1e-12) / kInputAmplitude);
    }
    return response;
}
}

int main() {
    // These probes stay below the 48 kHz profile's Nyquist frequency.
    for (const int sourceChannel : {0, 1}) {
        for (const double frequency : {8000.0, 12000.0, 16000.0, 18000.0}) {
            const auto reference = render(48000, frequency, sourceChannel);
            for (const int testRate : {192000}) {
                const auto highRate = render(testRate, frequency, sourceChannel);
                for (int outputChannel = 0; outputChannel < 2; ++outputChannel) {
                    const double difference = std::fabs(reference[outputChannel] -
                                                         highRate[outputChannel]);
                    std::printf("source=%c output=%c freq=%.0fHz 48k=%.2fdB %dk=%.2fdB delta=%.2fdB\n",
                        sourceChannel == 0 ? 'L' : 'R', outputChannel == 0 ? 'L' : 'R',
                        frequency, reference[outputChannel], testRate / 1000,
                        highRate[outputChannel], difference);
                    if (difference > 1.5) {
                        std::fprintf(stderr, "FAIL high-rate HRTF level mismatch %.2f dB\n", difference);
                        return 1;
                    }
                }
            }
        }
    }
    std::puts("PASS 192 kHz HRTF response matches the 48 kHz reference within 1.5 dB");
    return 0;
}
