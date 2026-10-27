#include "phonon.h"
#include <cstdlib>
#include <cstring>

static bool gFailHrtf = false;
static int gRate = 0;
extern "C" void mockSetFailHrtf(bool fail) { gFailHrtf = fail; }
extern "C" int mockLastRate() { return gRate; }

struct FakeHandle { int id; };
IPLerror iplContextCreate(IPLContextSettings*, IPLContext* c) { *c = reinterpret_cast<IPLContext>(new FakeHandle{1}); return IPL_STATUS_SUCCESS; }
void iplContextRelease(IPLContext* c) { delete reinterpret_cast<FakeHandle*>(*c); *c = nullptr; }
IPLerror iplHRTFCreate(IPLContext, IPLAudioSettings* a, IPLHRTFSettings*, IPLHRTF* h) {
    gRate = a->samplingRate;
    if (gFailHrtf) { *h = nullptr; return IPL_STATUS_INITIALIZATION; }
    *h = reinterpret_cast<IPLHRTF>(new FakeHandle{2}); return IPL_STATUS_SUCCESS;
}
void iplHRTFRelease(IPLHRTF* h) { delete reinterpret_cast<FakeHandle*>(*h); *h = nullptr; }
IPLerror iplAudioBufferAllocate(IPLContext, IPLint32 channels, IPLint32 frames, IPLAudioBuffer* b) {
    b->numChannels = channels; b->numSamples = frames;
    b->data = static_cast<IPLfloat32**>(std::calloc(channels, sizeof(IPLfloat32*)));
    for (int c = 0; c < channels; ++c) b->data[c] = static_cast<IPLfloat32*>(std::calloc(frames, sizeof(IPLfloat32)));
    return IPL_STATUS_SUCCESS;
}
void iplAudioBufferFree(IPLContext, IPLAudioBuffer* b) {
    if (b->data) { for (int c = 0; c < b->numChannels; ++c) std::free(b->data[c]); std::free(b->data); }
    std::memset(b, 0, sizeof(*b));
}
IPLerror iplBinauralEffectCreate(IPLContext, IPLAudioSettings*, IPLBinauralEffectSettings*, IPLBinauralEffect* e) {
    static int id = 10; *e = reinterpret_cast<IPLBinauralEffect>(new FakeHandle{id++}); return IPL_STATUS_SUCCESS;
}
void iplBinauralEffectRelease(IPLBinauralEffect* e) { delete reinterpret_cast<FakeHandle*>(*e); *e = nullptr; }
IPLAudioEffectState iplBinauralEffectApply(IPLBinauralEffect e, IPLBinauralEffectParams* p, IPLAudioBuffer* in, IPLAudioBuffer* out) {
    const float bias = p->direction.x * 0.15f + (reinterpret_cast<FakeHandle*>(e)->id % 2 ? 0.03f : -0.03f);
    for (int i = 0; i < in->numSamples; ++i) {
        const float x = in->data[0][i];
        out->data[0][i] = x * (0.80f + bias);
        out->data[1][i] = x * (0.20f - bias);
    }
    return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;
}
