#include "replay-settings.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <algorithm>

namespace sp::replay {
namespace {

std::string modulePath(const char *relative)
{
	char *path = obs_module_config_path(relative);
	if (!path)
		return {};
	std::string result(path);
	bfree(path);
	return result;
}

} // namespace

std::string SettingsStore::settingsPath()
{
	return modulePath("settings.json");
}

std::string SettingsStore::replayDirectory()
{
	const auto directory = modulePath("replays");
	if (!directory.empty())
		os_mkdirs(directory.c_str());
	return directory;
}

Settings SettingsStore::load()
{
	Settings settings;
	const auto path = settingsPath();
	if (path.empty())
		return settings;

	obs_data_t *data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!data)
		return settings;

	obs_data_array_t *cameras = obs_data_get_array(data, "camera_uuids");
	const auto cameraCount = cameras ? obs_data_array_count(cameras) : 0;
	if (cameraCount >= 2) {
		settings.sourceUuids.clear();
		settings.sourceUuids.reserve(cameraCount);
		for (std::size_t index = 0; index < cameraCount; ++index) {
			obs_data_t *camera = obs_data_array_item(cameras, index);
			settings.sourceUuids.emplace_back(camera ? obs_data_get_string(camera, "uuid") : "");
			if (camera)
				obs_data_release(camera);
		}
	} else {
		settings.sourceUuids[0] = obs_data_get_string(data, "camera_1_uuid");
		settings.sourceUuids[1] = obs_data_get_string(data, "camera_2_uuid");
	}
	if (cameras)
		obs_data_array_release(cameras);
	const char *encoder = obs_data_get_string(data, "encoder_id");
	if (encoder && *encoder)
		settings.encoderId = encoder;
	const char *scene = obs_data_get_string(data, "replay_scene");
	if (scene && *scene)
		settings.replaySceneName = scene;

	if (obs_data_has_user_value(data, "buffer_seconds"))
		settings.bufferSeconds = std::clamp(static_cast<int>(obs_data_get_int(data, "buffer_seconds")), 10, 60);
	if (obs_data_has_user_value(data, "replay_window_seconds"))
		settings.replayWindowSeconds = std::clamp(
			static_cast<int>(obs_data_get_int(data, "replay_window_seconds")), 3, settings.bufferSeconds);
	if (obs_data_has_user_value(data, "segment_seconds"))
		settings.segmentSeconds =
			std::clamp(static_cast<int>(obs_data_get_int(data, "segment_seconds")), 1, 10);
	if (obs_data_has_user_value(data, "bitrate_kbps"))
		settings.bitrateKbps =
			std::clamp(static_cast<int>(obs_data_get_int(data, "bitrate_kbps")), 4000, 50000);

	obs_data_release(data);
	return settings;
}

bool SettingsStore::save(const Settings &settings)
{
	const auto directory = modulePath(nullptr);
	if (directory.empty() || os_mkdirs(directory.c_str()) == MKDIR_ERROR)
		return false;

	obs_data_t *data = obs_data_create();
	obs_data_array_t *cameras = obs_data_array_create();
	for (const auto &uuid : settings.sourceUuids) {
		obs_data_t *camera = obs_data_create();
		obs_data_set_string(camera, "uuid", uuid.c_str());
		obs_data_array_push_back(cameras, camera);
		obs_data_release(camera);
	}
	obs_data_set_array(data, "camera_uuids", cameras);
	obs_data_array_release(cameras);
	obs_data_set_string(data, "encoder_id", settings.encoderId.c_str());
	obs_data_set_string(data, "replay_scene", settings.replaySceneName.c_str());
	obs_data_set_int(data, "buffer_seconds", settings.bufferSeconds);
	obs_data_set_int(data, "replay_window_seconds", settings.replayWindowSeconds);
	obs_data_set_int(data, "segment_seconds", settings.segmentSeconds);
	obs_data_set_int(data, "bitrate_kbps", settings.bitrateKbps);

	const auto path = settingsPath();
	const bool saved = !path.empty() && obs_data_save_json_safe(data, path.c_str(), "tmp", "bak");
	obs_data_release(data);
	return saved;
}

} // namespace sp::replay
