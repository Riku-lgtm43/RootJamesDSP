#ifdef DEBUG
#define TAG "EffectDSPMain"
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG,__VA_ARGS__)
#endif

#ifdef AOSP_SOONG_BUILD
#include <hardware/audio_effect.h>
#else
#include "essential.h"
#endif

#include "jdsp_impl.h"
#include "legacy_orbit_c.h"

// Temporary v40 diagnostic. The worker emits aggregate levels once per second;
// the real-time process callback only updates lock-free counters.
#include <android/log.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <time.h>

#define DIAG_TAG "JamesDSP-Signal"
#define DIAG_MAX_FRAMES 8192
#define DIAG_NONZERO_THRESHOLD 0.0000001f

struct signal_measure {
    uint64_t nonzero;
    uint64_t nonfinite;
    uint32_t peak_micro;
};

struct effect_diagnostic {
    int32_t session_id;
    int32_t io_id;
    atomic_int enabled;
    atomic_int sample_rate;
    atomic_int format;
    atomic_uint_fast64_t callbacks;
    atomic_uint_fast64_t frames;
    atomic_uint_fast64_t input_nonzero;
    atomic_uint_fast64_t core_nonzero;
    atomic_uint_fast64_t final_nonzero;
    atomic_uint_fast64_t nonfinite;
    atomic_uint_fast64_t process_errors;
    atomic_uint_fast64_t unmetered;
    atomic_uint_fast32_t input_peak;
    atomic_uint_fast32_t core_peak;
    atomic_uint_fast32_t final_peak;
    atomic_bool stop;
    pthread_t worker;
    pthread_mutex_t wake_mutex;
    pthread_cond_t wake_cond;
    int mutex_initialized;
    int cond_initialized;
    int worker_started;
};

static struct signal_measure measure_float(const float *samples, size_t frames)
{
    struct signal_measure value = {0};
    if (!samples) return value;
    const size_t count = frames * 2;
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        const float sample = samples[i];
        if (!isfinite(sample)) {
            ++value.nonfinite;
            continue;
        }
        const float magnitude = fabsf(sample);
        if (magnitude > DIAG_NONZERO_THRESHOLD) ++value.nonzero;
        if (magnitude > peak) peak = magnitude;
    }
    const double scaled = (double)peak * 1000000.0;
    value.peak_micro = scaled >= UINT32_MAX ? UINT32_MAX : (uint32_t)scaled;
    return value;
}

static void update_peak(atomic_uint_fast32_t *peak, uint32_t candidate)
{
    uint_fast32_t prior = atomic_load_explicit(peak, memory_order_relaxed);
    while (prior < candidate &&
           !atomic_compare_exchange_weak_explicit(peak, &prior, candidate,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) { }
}

static void *diagnostic_worker(void *raw)
{
    struct effect_diagnostic *diag = raw;
    pthread_mutex_lock(&diag->wake_mutex);
    while (!atomic_load_explicit(&diag->stop, memory_order_acquire)) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        ++deadline.tv_sec;
        pthread_cond_timedwait(&diag->wake_cond, &diag->wake_mutex, &deadline);
        if (atomic_load_explicit(&diag->stop, memory_order_acquire)) break;
        const uint64_t callbacks = atomic_exchange_explicit(&diag->callbacks, 0, memory_order_relaxed);
        const uint64_t frames = atomic_exchange_explicit(&diag->frames, 0, memory_order_relaxed);
        const uint64_t in_nz = atomic_exchange_explicit(&diag->input_nonzero, 0, memory_order_relaxed);
        const uint64_t core_nz = atomic_exchange_explicit(&diag->core_nonzero, 0, memory_order_relaxed);
        const uint64_t final_nz = atomic_exchange_explicit(&diag->final_nonzero, 0, memory_order_relaxed);
        const uint64_t nonfinite = atomic_exchange_explicit(&diag->nonfinite, 0, memory_order_relaxed);
        const uint64_t errors = atomic_exchange_explicit(&diag->process_errors, 0, memory_order_relaxed);
        const uint64_t unmetered = atomic_exchange_explicit(&diag->unmetered, 0, memory_order_relaxed);
        const uint32_t in_peak = atomic_exchange_explicit(&diag->input_peak, 0, memory_order_relaxed);
        const uint32_t core_peak = atomic_exchange_explicit(&diag->core_peak, 0, memory_order_relaxed);
        const uint32_t final_peak = atomic_exchange_explicit(&diag->final_peak, 0, memory_order_relaxed);
        if (callbacks || unmetered) {
            __android_log_print(ANDROID_LOG_INFO, DIAG_TAG,
                "sid=%d io=%d enabled=%d rate=%d format=%d calls=%" PRIu64
                " frames=%" PRIu64 " nz=%" PRIu64 "/%" PRIu64 "/%" PRIu64
                " peak_u=%u/%u/%u nonfinite=%" PRIu64 " errors=%" PRIu64
                " unmetered=%" PRIu64,
                diag->session_id, diag->io_id,
                atomic_load_explicit(&diag->enabled, memory_order_relaxed),
                atomic_load_explicit(&diag->sample_rate, memory_order_relaxed),
                atomic_load_explicit(&diag->format, memory_order_relaxed),
                callbacks, frames, in_nz, core_nz, final_nz,
                in_peak, core_peak, final_peak, nonfinite, errors, unmetered);
        }
    }
    pthread_mutex_unlock(&diag->wake_mutex);
    return NULL;
}

static void diagnostic_init(struct effect_diagnostic *diag, int32_t session_id, int32_t io_id)
{
    diag->session_id = session_id;
    diag->io_id = io_id;
    atomic_init(&diag->enabled, 0);
    atomic_init(&diag->sample_rate, 0);
    atomic_init(&diag->format, 0);
    atomic_init(&diag->callbacks, 0);
    atomic_init(&diag->frames, 0);
    atomic_init(&diag->input_nonzero, 0);
    atomic_init(&diag->core_nonzero, 0);
    atomic_init(&diag->final_nonzero, 0);
    atomic_init(&diag->nonfinite, 0);
    atomic_init(&diag->process_errors, 0);
    atomic_init(&diag->unmetered, 0);
    atomic_init(&diag->input_peak, 0);
    atomic_init(&diag->core_peak, 0);
    atomic_init(&diag->final_peak, 0);
    atomic_init(&diag->stop, false);
    int error = pthread_mutex_init(&diag->wake_mutex, NULL);
    diag->mutex_initialized = error == 0;
    if (error != 0) goto failed;
    error = pthread_cond_init(&diag->wake_cond, NULL);
    diag->cond_initialized = error == 0;
    if (error != 0) goto failed;
    error = pthread_create(&diag->worker, NULL, diagnostic_worker, diag);
    diag->worker_started = error == 0;
    if (error == 0) return;
failed:
    __android_log_print(ANDROID_LOG_ERROR, DIAG_TAG,
                        "sid=%d monitor start failed: %d", session_id, error);
}

static void diagnostic_destroy(struct effect_diagnostic *diag)
{
    atomic_store_explicit(&diag->stop, true, memory_order_release);
    if (diag->worker_started) {
        pthread_mutex_lock(&diag->wake_mutex);
        pthread_cond_signal(&diag->wake_cond);
        pthread_mutex_unlock(&diag->wake_mutex);
        pthread_join(diag->worker, NULL);
    }
    if (diag->cond_initialized) pthread_cond_destroy(&diag->wake_cond);
    if (diag->mutex_initialized) pthread_mutex_destroy(&diag->wake_mutex);
}

// Effect section end
static effect_descriptor_t jamesdsp_descriptor =
{
	{ 0xf98765f4, 0xc321, 0x5de6, 0x9a45, { 0x12, 0x34, 0x59, 0x49, 0x5a, 0xb2 } },
	{ 0xf27317f4, 0xc984, 0x4de6, 0x9a90, { 0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2 } }, // own UUID
	EFFECT_CONTROL_API_VERSION,
	EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_FIRST,
	10,
	1,
	"JamesDSP v4.01",
	"James Fung"
};

// Library mandatory methods
struct effect_module_s
{
	const struct effect_interface_s *itfe;
	EffectDSPMain effect;
	effect_descriptor_t *descriptor;
    void *orbit_context;
    struct effect_diagnostic diagnostic;
};
static int32_t generic_process(effect_handle_t self, audio_buffer_t *in, audio_buffer_t *out)
{
	if (!self || !in || !out) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) self;
	struct effect_diagnostic *diag = &e->diagnostic;
	const int meter = e->effect.formatFloatModeInt32Mode == 1 &&
	                  in->f32 && out->f32 && in->frameCount == out->frameCount &&
	                  in->frameCount <= DIAG_MAX_FRAMES;
	struct signal_measure input = {0}, core = {0}, final = {0};
	if (meter) input = measure_float(in->f32, in->frameCount);
	int32_t result = EffectDSPMainProcess(&e->effect, in, out);
	if (meter) core = measure_float(out->f32, out->frameCount);
    if (e->orbit_context && in && out && out->f32 &&
        in->frameCount == out->frameCount &&
        e->effect.formatFloatModeInt32Mode == 1 && result == 0) {
        jdsp_legacy_orbit_process(e->orbit_context, out->f32, in->frameCount);
    }
	if (meter) {
		final = measure_float(out->f32, out->frameCount);
		atomic_fetch_add_explicit(&diag->input_nonzero, input.nonzero, memory_order_relaxed);
		atomic_fetch_add_explicit(&diag->core_nonzero, core.nonzero, memory_order_relaxed);
		atomic_fetch_add_explicit(&diag->final_nonzero, final.nonzero, memory_order_relaxed);
		atomic_fetch_add_explicit(&diag->nonfinite,
		    input.nonfinite + core.nonfinite + final.nonfinite, memory_order_relaxed);
		update_peak(&diag->input_peak, input.peak_micro);
		update_peak(&diag->core_peak, core.peak_micro);
		update_peak(&diag->final_peak, final.peak_micro);
	} else {
		atomic_fetch_add_explicit(&diag->unmetered, 1, memory_order_relaxed);
	}
	atomic_fetch_add_explicit(&diag->callbacks, 1, memory_order_relaxed);
	atomic_fetch_add_explicit(&diag->frames, in->frameCount, memory_order_relaxed);
	if (result != 0 && result != -ENODATA)
		atomic_fetch_add_explicit(&diag->process_errors, 1, memory_order_relaxed);
    return result;
}
static int32_t generic_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize, void *pCmdData, uint32_t *replySize, void *pReplyData)
{
	if (!self) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) self;
	if (e->orbit_context && cmdCode == EFFECT_CMD_SET_CONFIG && pCmdData && cmdSize >= sizeof(effect_config_t)) {
        int32_t result = EffectDSPMainCommand(&e->effect, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
        int32_t configStatus = -EINVAL;
        if (result == 0 && replySize && *replySize >= sizeof(configStatus) && pReplyData)
            memcpy(&configStatus, pReplyData, sizeof(configStatus));
        jdsp_legacy_orbit_configure(e->orbit_context, (const effect_config_t *)pCmdData,
                                    result == 0 && configStatus == 0);
		const effect_config_t *config = (const effect_config_t *)pCmdData;
		if (result == 0 && configStatus == 0) {
			atomic_store_explicit(&e->diagnostic.sample_rate,
			    (int)config->outputCfg.samplingRate, memory_order_relaxed);
			atomic_store_explicit(&e->diagnostic.format,
			    (int)config->outputCfg.format, memory_order_relaxed);
		}
		__android_log_print(ANDROID_LOG_INFO, DIAG_TAG,
		    "sid=%d SET_CONFIG result=%d status=%d rate=%u in_format=%u out_format=%u",
		    e->diagnostic.session_id, result, configStatus,
		    config->outputCfg.samplingRate, config->inputCfg.format, config->outputCfg.format);
        return result;
    }
    if (e->orbit_context && (cmdCode == EFFECT_CMD_SET_PARAM || cmdCode == EFFECT_CMD_GET_PARAM)) {
        int result = jdsp_legacy_orbit_command(e->orbit_context, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
        if (result != 1) return result;
    }
    const int32_t result = EffectDSPMainCommand(&e->effect, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
    if (cmdCode == EFFECT_CMD_ENABLE || cmdCode == EFFECT_CMD_DISABLE) {
        atomic_store_explicit(&e->diagnostic.enabled,
            e->effect.mEnable != 0, memory_order_relaxed);
        __android_log_print(ANDROID_LOG_INFO, DIAG_TAG,
            "sid=%d %s result=%d", e->diagnostic.session_id,
            cmdCode == EFFECT_CMD_ENABLE ? "ENABLE" : "DISABLE", result);
    }
    return result;
}
static int32_t generic_getDescriptor(effect_handle_t self, effect_descriptor_t *pDescriptor)
{
	if (!self || !pDescriptor) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) self;
	memcpy(pDescriptor, e->descriptor, sizeof(effect_descriptor_t));
	return 0;
}
static const struct effect_interface_s generic_interface =
{
	generic_process,
	generic_command,
	generic_getDescriptor,
	NULL
};
int32_t EffectCreate(const effect_uuid_t *uuid, int32_t sessionId, int32_t ioId, effect_handle_t *pEffect)
{
	if (!uuid || !pEffect) return -EINVAL;
	*pEffect = NULL;
	if (memcmp(uuid, &jamesdsp_descriptor.uuid, sizeof(*uuid)) != 0) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) calloc(1, sizeof(struct effect_module_s));
	if (!e) return -ENOMEM;
	e->itfe = &generic_interface;
#ifdef DEBUG
	LOGI("JamesDSP: Creating effect with %d sessionId", sessionId);
#endif
	EffectDSPMainConstructor(&e->effect);
    e->orbit_context = jdsp_legacy_orbit_create();
	diagnostic_init(&e->diagnostic, sessionId, ioId);
	e->descriptor = &jamesdsp_descriptor;
	*pEffect = (effect_handle_t)e;
	__android_log_print(ANDROID_LOG_INFO, DIAG_TAG,
	    "sid=%d io=%d CREATE", sessionId, ioId);
#ifdef DEBUG
	LOGI("JamesDSP: Effect created");
#endif
	return 0;
}
int32_t EffectRelease(effect_handle_t ei)
{
	if (!ei) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) ei;
	__android_log_print(ANDROID_LOG_INFO, DIAG_TAG,
	    "sid=%d RELEASE", e->diagnostic.session_id);
	diagnostic_destroy(&e->diagnostic);
	jdsp_legacy_orbit_destroy(e->orbit_context);
    EffectDSPMainDestructor(&e->effect);
	free(e);
	return 0;
}
int32_t EffectGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor)
{
#ifdef DEBUG
	LOGI("JamesDSP: Copying descriptor info");
#endif
	if (pDescriptor == NULL || uuid == NULL)
	{
#ifdef DEBUG
		LOGI("JamesDSP: EffectGetDescriptor() called with NULL pointer");
#endif
		return -EINVAL;
	}
	if (memcmp(uuid, &jamesdsp_descriptor.uuid, sizeof(*uuid)) != 0) return -EINVAL;
	*pDescriptor = jamesdsp_descriptor;
	return 0;
}
__attribute__((visibility("default"))) audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM =
{
	.tag = AUDIO_EFFECT_LIBRARY_TAG,
	.version = EFFECT_LIBRARY_API_VERSION,
#if __aarch64__ == 1
		.name = "James Audio DSP arm64",
#elif __ARM_ARCH_7A__ == 1
		.name = "James Audio DSP arm32",
#elif __i386__ == 1
		.name = "James Audio DSP x86",
#elif __x86_64__ == 1
		.name = "James Audio DSP x64",
#endif
		.implementor = "James Fung",
		.create_effect = EffectCreate,
		.release_effect = EffectRelease,
		.get_descriptor = EffectGetDescriptor,
};
