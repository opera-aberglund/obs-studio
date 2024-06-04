#include <obs-module.h>
#include "sink-source.h"


bool load_image_from_memory(uint8_t *img_data, uint32_t data_size, uint8_t *dest);
int init_sink_thread(struct sink_source *context);
int join_sink_thread(struct sink_source *context);

static const char *sink_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "SinkInput";
}

static void *sink_source_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
	struct sink_source *context = (sink_source *)bzalloc(sizeof(struct sink_source));
    context->stop_signal = false;
	context->source = source;
    // TODO: How do we get the size of the image?
    context->width = 1280;
    context->height = 720;
    int img_size = context->width * context->height * 4;
    context->img_data = (uint8_t*)bzalloc(img_size);
    memset(context->img_data, 0, img_size);
            
    init_sink_thread(context);
    
    obs_enter_graphics();
    context->texture = gs_texture_create(context->width, context->height, GS_BGRA, 1, (const uint8_t **)&(context->img_data), GS_DYNAMIC);
    obs_leave_graphics();

	return context;
}

static void sink_source_destroy(void *data)
{
	struct sink_source *context = (sink_source *)data;
    join_sink_thread(context);
    bfree(context->img_data);
	bfree(context);
}

static uint32_t sink_source_getwidth(void *data)
{
	struct sink_source *context = (sink_source *)data;
    return context->width;
}

static uint32_t sink_source_getheight(void *data)
{
	struct sink_source *context = (sink_source *)data;
	return context->height;
}

static void sink_source_render(void *data, gs_effect_t *effect)
{
	struct sink_source *context = (sink_source *)data;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture_srgb(param, context->texture);
    
    gs_draw_sprite(context->texture, 0, context->width, context->height);

	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(previous);
}

static void sink_source_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(seconds);
	struct sink_source *context = (sink_source *)data;

    if (!atomic_load(&context->image_decoded)) {
        atomic_store(&context->decoding_image, true);
        load_image_from_memory(context->read_buffer, context->read_buffer_data_size, context->img_data);
        atomic_store(&context->decoding_image, false);
        
        atomic_store(&context->image_decoded, true);

        obs_enter_graphics();
        gs_texture_set_image(context->texture, context->img_data, context->width * 4, false);
        obs_leave_graphics();
    }
}

static enum gs_color_space
sink_source_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
    UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);
    return GS_CS_SRGB;
}

static struct obs_source_info sink_source_info = {
	.id = "sink_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = sink_source_get_name,
	.create = sink_source_create,
	.destroy = sink_source_destroy,
	.get_width = sink_source_getwidth,
	.get_height = sink_source_getheight,
	.video_render = sink_source_render,
	.video_tick = sink_source_tick,
	.icon_type = OBS_ICON_TYPE_CUSTOM,
	.video_get_color_space = sink_source_get_color_space,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("image-sink", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Sink source";
}

bool obs_module_load(void)
{
	obs_register_source(&sink_source_info);
	return true;
}
