#include <stddef.h>
#include <string.h>
#include "jdsp_impl.h"

static int32_t config_reply_status;
static int32_t process_status;

void testSetConfigReplyStatus(int32_t status) { config_reply_status = status; }
void testSetProcessStatus(int32_t status) { process_status = status; }

void EffectDSPMainConstructor(EffectDSPMain* effect) {
    memset(effect, 0, sizeof(*effect));
    effect->formatFloatModeInt32Mode = 1;
    effect->mEnable = 1;
}

void EffectDSPMainDestructor(EffectDSPMain* effect) { (void)effect; }

int32_t EffectDSPMainCommand(EffectDSPMain* effect, uint32_t code, uint32_t size,
                             void* data, uint32_t* reply_size, void* reply) {
    (void)effect; (void)size; (void)data; (void)reply_size;
    if (code == EFFECT_CMD_SET_CONFIG && reply)
        memcpy(reply, &config_reply_status, sizeof(config_reply_status));
    return 0; /* API status: command was dispatched; operation status is in reply. */
}

int32_t EffectDSPMainProcess(EffectDSPMain* effect, audio_buffer_t* in,
                             audio_buffer_t* out) {
    (void)effect;
    if (!in || !out || !in->f32 || !out->f32 || in->frameCount != out->frameCount)
        return -EINVAL;
    memmove(out->f32, in->f32, in->frameCount * 2 * sizeof(float));
    return process_status;
}
