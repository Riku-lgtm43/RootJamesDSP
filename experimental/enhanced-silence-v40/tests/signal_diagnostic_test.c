#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the diagnostic translation unit so the test can inspect its counters
// without changing the Android build's exported effect API.
#include "../source/jamesdsp.c"

static int core_mute;
static int orbit_mute;
static atomic_uint report_count;
static pthread_mutex_t report_mutex = PTHREAD_MUTEX_INITIALIZER;
static char last_report[512];

int __android_log_print(int priority, const char *tag, const char *format, ...)
{
    (void)priority;
    (void)tag;
    char record[512];
    va_list args;
    va_start(args, format);
    const int count = vsnprintf(record, sizeof(record), format, args);
    va_end(args);
    if (count > 0 && strstr(record, " calls=")) {
        pthread_mutex_lock(&report_mutex);
        snprintf(last_report, sizeof(last_report), "%s", record);
        atomic_fetch_add(&report_count, 1);
        pthread_mutex_unlock(&report_mutex);
    }
    return 0;
}

void EffectDSPMainConstructor(EffectDSPMain *effect)
{
    memset(effect, 0, sizeof(*effect));
    effect->formatFloatModeInt32Mode = 1;
}

void EffectDSPMainDestructor(EffectDSPMain *effect) { (void)effect; }

int32_t EffectDSPMainCommand(EffectDSPMain *effect, uint32_t code, uint32_t size,
                             void *data, uint32_t *reply_size, void *reply)
{
    (void)size;
    (void)data;
    (void)reply_size;
    if (code == EFFECT_CMD_ENABLE) effect->mEnable = 1;
    if (code == EFFECT_CMD_DISABLE) effect->mEnable = 0;
    if (reply) *(int32_t *)reply = 0;
    return 0;
}

int32_t EffectDSPMainProcess(EffectDSPMain *effect, audio_buffer_t *in,
                             audio_buffer_t *out)
{
    if (core_mute) memset(out->f32, 0, in->frameCount * 2 * sizeof(float));
    else memcpy(out->f32, in->f32, in->frameCount * 2 * sizeof(float));
    return effect->mEnable ? 0 : -ENODATA;
}

void *jdsp_legacy_orbit_create(void) { return malloc(1); }
void jdsp_legacy_orbit_destroy(void *ctx) { free(ctx); }
void jdsp_legacy_orbit_configure(void *ctx, const void *raw, int accepted)
{
    (void)ctx;
    (void)raw;
    (void)accepted;
}
void jdsp_legacy_orbit_process(void *ctx, float *stereo, size_t frames)
{
    (void)ctx;
    if (orbit_mute) memset(stereo, 0, frames * 2 * sizeof(float));
}
int jdsp_legacy_orbit_command(void *ctx, uint32_t cmd, uint32_t size, void *data,
                              uint32_t *reply_size, void *reply)
{
    (void)ctx; (void)cmd; (void)size; (void)data; (void)reply_size; (void)reply;
    return 1;
}

static void reset_counters(struct effect_diagnostic *diag)
{
    atomic_store(&diag->callbacks, 0);
    atomic_store(&diag->frames, 0);
    atomic_store(&diag->input_nonzero, 0);
    atomic_store(&diag->core_nonzero, 0);
    atomic_store(&diag->final_nonzero, 0);
    atomic_store(&diag->nonfinite, 0);
    atomic_store(&diag->unmetered, 0);
    atomic_store(&diag->input_peak, 0);
    atomic_store(&diag->core_peak, 0);
    atomic_store(&diag->final_peak, 0);
}

static void run_case(struct effect_module_s *effect, int mute_core, int mute_orbit,
                     uint64_t expected_core, uint64_t expected_final)
{
    const float source[] = {0.25f, -0.125f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f};
    float result[8] = {0};
    audio_buffer_t in = {.frameCount = 4, .f32 = (float *)source};
    audio_buffer_t out = {.frameCount = 4, .f32 = result};
    core_mute = mute_core;
    orbit_mute = mute_orbit;
    reset_counters(&effect->diagnostic);
    assert(generic_process((effect_handle_t)effect, &in, &out) == 0);
    assert(atomic_load(&effect->diagnostic.callbacks) == 1);
    assert(atomic_load(&effect->diagnostic.frames) == 4);
    assert(atomic_load(&effect->diagnostic.input_nonzero) == 2);
    assert(atomic_load(&effect->diagnostic.core_nonzero) == expected_core);
    assert(atomic_load(&effect->diagnostic.final_nonzero) == expected_final);
    assert(atomic_load(&effect->diagnostic.input_peak) == 250000);
    assert(atomic_load(&effect->diagnostic.core_peak) ==
           (expected_core ? 250000 : 0));
    assert(atomic_load(&effect->diagnostic.final_peak) ==
           (expected_final ? 250000 : 0));
}

int main(void)
{
    struct effect_module_s effect = {0};
    EffectDSPMainConstructor(&effect.effect);
    effect.orbit_context = jdsp_legacy_orbit_create();
    diagnostic_init(&effect.diagnostic, 329, 21);

    effect_config_t config = {0};
    config.inputCfg.samplingRate = config.outputCfg.samplingRate = 48000;
    config.inputCfg.format = config.outputCfg.format = AUDIO_FORMAT_PCM_FLOAT;
    uint32_t reply_size = sizeof(int32_t);
    int32_t reply = -1;
    assert(generic_command((effect_handle_t)&effect, EFFECT_CMD_SET_CONFIG,
                           sizeof(config), &config, &reply_size, &reply) == 0);
    assert(reply == 0);
    assert(atomic_load(&effect.diagnostic.sample_rate) == 48000);
    assert(atomic_load(&effect.diagnostic.format) == AUDIO_FORMAT_PCM_FLOAT);
    assert(generic_command((effect_handle_t)&effect, EFFECT_CMD_ENABLE,
                           0, NULL, &reply_size, &reply) == 0);
    assert(atomic_load(&effect.diagnostic.enabled) == 1);

    run_case(&effect, 0, 0, 2, 2);
    run_case(&effect, 1, 0, 0, 0);
    run_case(&effect, 0, 1, 2, 0);

    const struct timespec brief_wait = {.tv_sec = 0, .tv_nsec = 50000000};
    for (int i = 0; i < 40 && atomic_load(&report_count) == 0; ++i)
        nanosleep(&brief_wait, NULL);
    assert(atomic_load(&report_count) > 0);
    pthread_mutex_lock(&report_mutex);
    assert(strstr(last_report, "sid=329 io=21 enabled=1 rate=48000 format=5"));
    assert(strstr(last_report, " calls=1 frames=4 nz=2/2/0"));
    assert(strstr(last_report, " peak_u=250000/250000/0"));
    pthread_mutex_unlock(&report_mutex);

    assert(generic_command((effect_handle_t)&effect, EFFECT_CMD_DISABLE,
                           0, NULL, &reply_size, &reply) == 0);
    assert(atomic_load(&effect.diagnostic.enabled) == 0);
    diagnostic_destroy(&effect.diagnostic);
    jdsp_legacy_orbit_destroy(effect.orbit_context);
    EffectDSPMainDestructor(&effect.effect);
    puts("signal diagnostic stage and command checks passed");
    return 0;
}
