#include "replay-output-source.hpp"

#include "replay-layout.hpp"
#include "replay-media-provider.hpp"

#include <graphics/graphics.h>
#include <obs-module.h>

#include <cstdint>

namespace sp::replay {
namespace {

struct ReplayOutputContext {};

struct VideoSize {
	uint32_t width{1920};
	uint32_t height{1080};
};

VideoSize canvasSize()
{
	obs_video_info info = {};
	if (!obs_get_video_info(&info) || info.base_width == 0 || info.base_height == 0)
		return {};
	return {info.base_width, info.base_height};
}

const char *getName(void *)
{
	return obs_module_text("ReplayOutput");
}

void *create(obs_data_t *, obs_source_t *)
{
	return new ReplayOutputContext;
}

void destroy(void *data)
{
	delete static_cast<ReplayOutputContext *>(data);
}

uint32_t width(void *)
{
	return canvasSize().width;
}

uint32_t height(void *)
{
	return canvasSize().height;
}

void render(void *, gs_effect_t *)
{
	obs_source_t *media = acquireReplayMediaForRender();
	if (media) {
		const auto canvas = canvasSize();
		const auto mediaWidth = obs_source_get_width(media);
		const auto mediaHeight = obs_source_get_height(media);
		const auto draw = coverRect(mediaWidth, mediaHeight, canvas.width, canvas.height);
		const float scaleX = mediaWidth > 0 ? static_cast<float>(draw.width) / mediaWidth : 1.0f;
		const float scaleY = mediaHeight > 0 ? static_cast<float>(draw.height) / mediaHeight : 1.0f;
		gs_matrix_push();
		gs_matrix_translate3f(static_cast<float>(draw.x), static_cast<float>(draw.y), 0.0f);
		gs_matrix_scale3f(scaleX, scaleY, 1.0f);
		obs_source_video_render(media);
		gs_matrix_pop();
		obs_source_release(media);
	}
}

obs_source_info makeOutputInfo()
{
	obs_source_info info = {};
	info.id = "secretariatpro_replay_output";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = getName;
	info.create = create;
	info.destroy = destroy;
	info.get_width = width;
	info.get_height = height;
	info.video_render = render;
	return info;
}

obs_source_info outputInfo = makeOutputInfo();

} // namespace

void registerReplayOutputSource()
{
	obs_register_source(&outputInfo);
}

} // namespace sp::replay
