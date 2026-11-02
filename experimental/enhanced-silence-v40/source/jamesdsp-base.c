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
};
static int32_t generic_process(effect_handle_t self, audio_buffer_t *in, audio_buffer_t *out)
{
	if (!self || !in || !out) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) self;
	int32_t result = EffectDSPMainProcess(&e->effect, in, out);
    if (e->orbit_context && in && out && out->f32 &&
        in->frameCount == out->frameCount &&
        e->effect.formatFloatModeInt32Mode == 1 && result == 0)
        jdsp_legacy_orbit_process(e->orbit_context, out->f32, in->frameCount);
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
        return result;
    }
    if (e->orbit_context && (cmdCode == EFFECT_CMD_SET_PARAM || cmdCode == EFFECT_CMD_GET_PARAM)) {
        int result = jdsp_legacy_orbit_command(e->orbit_context, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
        if (result != 1) return result;
    }
    return EffectDSPMainCommand(&e->effect, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
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
	(void)ioId;
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
	e->descriptor = &jamesdsp_descriptor;
	*pEffect = (effect_handle_t)e;
#ifdef DEBUG
	LOGI("JamesDSP: Effect created");
#endif
	return 0;
}
int32_t EffectRelease(effect_handle_t ei)
{
	if (!ei) return -EINVAL;
	struct effect_module_s *e = (struct effect_module_s *) ei;
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
