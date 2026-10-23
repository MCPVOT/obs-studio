/* Test-only oracle: the original libobs/obs-service-getters.c.h, unmodified,
 * with its exports renamed to oracle_* so it can link next to the Rust
 * shims (#1899, #1913). find_service() and the context accessors are
 * test doubles that both sides call. obs_service_get_info_table() and
 * obs_service_get_plugin_data() are copied from obs-service.c,
 * and oracle_service_new() builds the struct obs_service both sides read,
 * so the parity tests drive the C and the Rust with the same service. */
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#define obs_service_get_supported_resolutions oracle_obs_service_get_supported_resolutions
#define obs_service_get_max_fps oracle_obs_service_get_max_fps
#define obs_service_get_max_bitrate oracle_obs_service_get_max_bitrate
#define obs_service_get_supported_video_codecs oracle_obs_service_get_supported_video_codecs
#define obs_service_get_supported_audio_codecs oracle_obs_service_get_supported_audio_codecs
#define obs_service_get_protocol oracle_obs_service_get_protocol
#define obs_service_get_preferred_output_type oracle_obs_service_get_preferred_output_type
#define obs_service_get_connect_info oracle_obs_service_get_connect_info
#define obs_service_can_try_to_connect oracle_obs_service_can_try_to_connect
#define obs_service_get_display_name oracle_obs_service_get_display_name
#define obs_service_get_name oracle_obs_service_get_name
#define obs_service_defaults oracle_obs_service_defaults
#define obs_get_service_properties oracle_obs_get_service_properties
#define obs_service_properties oracle_obs_service_properties
#define obs_service_get_type oracle_obs_service_get_type
#define obs_service_update oracle_obs_service_update
#define obs_service_get_settings oracle_obs_service_get_settings
#define obs_service_apply_encoder_settings oracle_obs_service_apply_encoder_settings
#define obs_service_get_type_data oracle_obs_service_get_type_data
#define obs_service_get_id oracle_obs_service_get_id

#include "obs-service-getters.c.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* obs-service.c */
const struct obs_service_info *obs_service_get_info_table(const obs_service_t *service)
{
	return &service->info;
}

void *obs_service_get_plugin_data(const obs_service_t *service)
{
	return service->context.data;
}

const char *obs_service_get_context_name(const obs_service_t *service)
{
	return service->context.name;
}

obs_data_t *obs_service_get_context_settings(const obs_service_t *service)
{
	return service->context.settings;
}

/* obs-service.c's find_service() over obs->service_types, here over one
 * table the test sets (NULL: no types). */
static const struct obs_service_info *oracle_types;
static size_t oracle_types_num;

void oracle_service_set_types(const struct obs_service_info *types, size_t num);
void oracle_service_set_types(const struct obs_service_info *types, size_t num)
{
	oracle_types = types;
	oracle_types_num = num;
}

const struct obs_service_info *find_service(const char *id)
{
	for (size_t i = 0; i < oracle_types_num; i++)
		if (strcmp(oracle_types[i].id, id) == 0)
			return oracle_types + i;
	return NULL;
}

/* The context fields #1913 reads; the plugin data and table are set by
 * oracle_service_new(). */
void oracle_service_set_context(struct obs_service *service, char *name, obs_data_t *settings);
void oracle_service_set_context(struct obs_service *service, char *name, obs_data_t *settings)
{
	service->context.name = name;
	service->context.settings = settings;
}

struct obs_service *oracle_service_new(const struct obs_service_info *info, void *data);
struct obs_service *oracle_service_new(const struct obs_service_info *info, void *data)
{
	struct obs_service *service = calloc(1, sizeof(*service));
	service->info = *info;
	service->context.data = data;
	return service;
}

void oracle_service_free(struct obs_service *service);
void oracle_service_free(struct obs_service *service)
{
	free(service);
}

#define SIZE_ALIGN(ty, stem)                        \
	size_t oracle_##stem##_size(void);          \
	size_t oracle_##stem##_size(void)           \
	{                                           \
		return sizeof(ty);                  \
	}                                           \
	size_t oracle_##stem##_align(void);         \
	size_t oracle_##stem##_align(void)          \
	{                                           \
		return _Alignof(ty);                \
	}

#define FIELD(ty, stem, field)                        \
	size_t oracle_##stem##_offset_##field(void);  \
	size_t oracle_##stem##_offset_##field(void)   \
	{                                             \
		return offsetof(ty, field);           \
	}

SIZE_ALIGN(struct obs_service_resolution, service_resolution)
FIELD(struct obs_service_resolution, service_resolution, cy)

SIZE_ALIGN(struct obs_service_info, service_info)
FIELD(struct obs_service_info, service_info, id)
FIELD(struct obs_service_info, service_info, get_name)
FIELD(struct obs_service_info, service_info, create)
FIELD(struct obs_service_info, service_info, destroy)
FIELD(struct obs_service_info, service_info, activate)
FIELD(struct obs_service_info, service_info, deactivate)
FIELD(struct obs_service_info, service_info, update)
FIELD(struct obs_service_info, service_info, get_defaults)
FIELD(struct obs_service_info, service_info, get_properties)
FIELD(struct obs_service_info, service_info, initialize)
FIELD(struct obs_service_info, service_info, get_url)
FIELD(struct obs_service_info, service_info, get_key)
FIELD(struct obs_service_info, service_info, get_username)
FIELD(struct obs_service_info, service_info, get_password)
FIELD(struct obs_service_info, service_info, deprecated_1)
FIELD(struct obs_service_info, service_info, apply_encoder_settings)
FIELD(struct obs_service_info, service_info, type_data)
FIELD(struct obs_service_info, service_info, free_type_data)
FIELD(struct obs_service_info, service_info, get_output_type)
FIELD(struct obs_service_info, service_info, get_supported_resolutions)
FIELD(struct obs_service_info, service_info, get_max_fps)
FIELD(struct obs_service_info, service_info, get_max_bitrate)
FIELD(struct obs_service_info, service_info, get_supported_video_codecs)
FIELD(struct obs_service_info, service_info, get_protocol)
FIELD(struct obs_service_info, service_info, get_supported_audio_codecs)
FIELD(struct obs_service_info, service_info, get_connect_info)
FIELD(struct obs_service_info, service_info, can_try_to_connect)
