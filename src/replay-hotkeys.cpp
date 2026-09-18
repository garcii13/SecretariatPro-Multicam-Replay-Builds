#include "replay-hotkeys.hpp"

#include "replay-engine.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QMetaObject>

#include <array>
#include <string>

namespace sp::replay {
namespace {

enum class HotkeyAction {
	ToggleBuffer,
	Mark,
	Take,
	Out,
	NextCamera,
	PreviousCamera,
};

std::string hotkeySettingsPath()
{
	char *path = obs_module_config_path("hotkeys.json");
	if (!path)
		return {};
	std::string result(path);
	bfree(path);
	return result;
}

} // namespace

struct ReplayHotkeys::Impl {
	struct Binding {
		const char *name{nullptr};
		const char *descriptionKey{nullptr};
		HotkeyAction action{HotkeyAction::ToggleBuffer};
		obs_hotkey_id id{OBS_INVALID_HOTKEY_ID};
		Impl *owner{nullptr};
	};

	explicit Impl(ReplayEngine *replayEngine)
		: engine(replayEngine),
		  bindings{{{"secretariatpro_replay_toggle_buffer", "HotkeyToggleBuffer", HotkeyAction::ToggleBuffer},
			    {"secretariatpro_replay_mark", "HotkeyMark", HotkeyAction::Mark},
			    {"secretariatpro_replay_take", "HotkeyTake", HotkeyAction::Take},
			    {"secretariatpro_replay_out", "HotkeyOut", HotkeyAction::Out},
			    {"secretariatpro_replay_next_camera", "HotkeyNextCamera", HotkeyAction::NextCamera},
			    {"secretariatpro_replay_previous_camera", "HotkeyPreviousCamera",
			     HotkeyAction::PreviousCamera}}}
	{
		for (auto &binding : bindings) {
			binding.owner = this;
			binding.id = obs_hotkey_register_frontend(binding.name, obs_module_text(binding.descriptionKey),
								  hotkeyCallback, &binding);
		}
		load();
	}

	~Impl()
	{
		save();
		for (const auto &binding : bindings) {
			if (binding.id != OBS_INVALID_HOTKEY_ID)
				obs_hotkey_unregister(binding.id);
		}
	}

	static void hotkeyCallback(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
	{
		if (!pressed || !data)
			return;
		auto *binding = static_cast<Binding *>(data);
		if (binding->owner)
			binding->owner->trigger(binding->action);
	}

	void trigger(HotkeyAction action)
	{
		QMetaObject::invokeMethod(
			engine,
			[replayEngine = engine, action]() {
				switch (action) {
				case HotkeyAction::ToggleBuffer:
					if (replayEngine->buffersActive())
						replayEngine->stopBuffers();
					else
						(void)replayEngine->startBuffers();
					break;
				case HotkeyAction::Mark:
					(void)replayEngine->markReplay();
					break;
				case HotkeyAction::Take:
					(void)replayEngine->take();
					break;
				case HotkeyAction::Out:
					replayEngine->out();
					break;
				case HotkeyAction::NextCamera: {
					const auto count = replayEngine->cameraCount();
					if (count > 0)
						replayEngine->switchCamera((replayEngine->activeCamera() + 1) % count);
					break;
				}
				case HotkeyAction::PreviousCamera: {
					const auto count = replayEngine->cameraCount();
					if (count > 0)
						replayEngine->switchCamera((replayEngine->activeCamera() + count - 1) %
									   count);
					break;
				}
				}
			},
			Qt::QueuedConnection);
	}

	void load()
	{
		const auto path = hotkeySettingsPath();
		if (path.empty())
			return;
		obs_data_t *data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
		if (!data)
			return;
		for (const auto &binding : bindings) {
			if (binding.id == OBS_INVALID_HOTKEY_ID)
				continue;
			obs_data_array_t *saved = obs_data_get_array(data, binding.name);
			if (saved) {
				obs_hotkey_load(binding.id, saved);
				obs_data_array_release(saved);
			}
		}
		obs_data_release(data);
	}

	void save() const
	{
		char *directory = obs_module_config_path("");
		if (!directory)
			return;
		const bool directoryReady = os_mkdirs(directory) != MKDIR_ERROR;
		bfree(directory);
		const auto path = hotkeySettingsPath();
		if (!directoryReady || path.empty())
			return;

		obs_data_t *data = obs_data_create();
		for (const auto &binding : bindings) {
			if (binding.id == OBS_INVALID_HOTKEY_ID)
				continue;
			obs_data_array_t *saved = obs_hotkey_save(binding.id);
			if (saved) {
				obs_data_set_array(data, binding.name, saved);
				obs_data_array_release(saved);
			}
		}
		(void)obs_data_save_json_safe(data, path.c_str(), "tmp", "bak");
		obs_data_release(data);
	}

	ReplayEngine *engine{nullptr};
	std::array<Binding, 6> bindings;
};

ReplayHotkeys::ReplayHotkeys(ReplayEngine *engine) : impl_(std::make_unique<Impl>(engine)) {}

ReplayHotkeys::~ReplayHotkeys() = default;

} // namespace sp::replay
