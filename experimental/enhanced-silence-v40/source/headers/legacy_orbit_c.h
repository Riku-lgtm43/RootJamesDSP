#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *jdsp_legacy_orbit_create(void);
void jdsp_legacy_orbit_destroy(void *ctx);
void jdsp_legacy_orbit_process(void *ctx, float *stereo, size_t frames);
void jdsp_legacy_orbit_configure(void *ctx, const void *effect_config, int accepted);
/* 1 = not an orbit command; 0 = handled; negative = rejected */
int jdsp_legacy_orbit_command(void *ctx, uint32_t cmd, uint32_t size, void *data, uint32_t *reply_size, void *reply);
#ifdef __cplusplus
}
#endif
