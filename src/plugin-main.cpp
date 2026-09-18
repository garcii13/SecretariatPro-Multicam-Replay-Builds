#include "plugin-support.h"
#include "replay-dock.hpp"
#include "replay-engine.hpp"
#include "replay-hotkeys.hpp"
#include "replay-output-source.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

sp::replay::ReplayEngine *engine = nullptr;
sp::replay::ReplayHotkeys *hotkeys = nullptr;
bool uiCreated = false;

void createUi()
{
	if (uiCreated)
		return;
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	engine = new sp::replay::ReplayEngine;
	auto *dock = new sp::replay::ReplayDock(engine, mainWindow);
	if (!obs_frontend_add_dock_by_id("secretariatpro_multicam_replay", "SecretariatPro Replay", dock)) {
		delete dock;
		delete engine;
		engine = nullptr;
		obs_log(LOG_ERROR, "could not register the replay dock");
		return;
	}
	hotkeys = new sp::replay::ReplayHotkeys(engine);
	uiCreated = true;
}

void destroyUi()
{
	if (!uiCreated)
		return;
	uiCreated = false;
	delete hotkeys;
	hotkeys = nullptr;
	// Registration transfers widget ownership to OBS/Qt. Removing the dock
	// destroys its widget too; deleting it again causes a crash on exit.
	obs_frontend_remove_dock("secretariatpro_multicam_replay");
	delete engine;
	engine = nullptr;
}

void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		createUi();
	else if (event == OBS_FRONTEND_EVENT_EXIT)
		destroyUi();
}

} // namespace

bool obs_module_load(void)
{
	sp::replay::registerReplayOutputSource();
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	destroyUi();
	obs_log(LOG_INFO, "plugin unloaded");
}

const char *obs_module_description(void)
{
	return "Dynamic multicamera ISO replay, slow motion and multi-shot replay sequences for OBS Studio.";
}
