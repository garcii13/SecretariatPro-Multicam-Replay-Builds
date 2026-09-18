#include "replay-engine.hpp"
#include "replay-media-provider.hpp"
#include "plugin-support.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs-encoder.h>
#include <util/platform.h>

#include <QMetaObject>
#include <QDir>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace sp::replay {
namespace {

constexpr const char *ReplayOutputName = "SP Replay Output";

struct ReplaySceneItems {
	obs_source_t *output{nullptr};
	obs_source_t *live{nullptr};
};

bool collectSource(void *data, obs_source_t *source)
{
	auto *items = static_cast<std::vector<std::pair<std::string, std::string>> *>(data);
	if (!source || std::strcmp(obs_source_get_id(source), "secretariatpro_replay_output") == 0)
		return true;
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) == 0)
		return true;
	const char *uuid = obs_source_get_uuid(source);
	const char *name = obs_source_get_name(source);
	if (uuid && *uuid && name && *name)
		items->emplace_back(uuid, name);
	return true;
}

bool configureReplayItem(obs_scene_t *, obs_sceneitem_t *item, void *data)
{
	auto *targets = static_cast<ReplaySceneItems *>(data);
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (source == targets->output) {
		obs_sceneitem_set_visible(item, true);
	} else {
		obs_sceneitem_set_visible(item, source == targets->live);
	}
	return true;
}

} // namespace

ReplayEngine *ReplayEngine::instance_ = nullptr;

obs_source_t *acquireReplayMediaForRender()
{
	auto *engine = ReplayEngine::instance();
	return engine ? engine->acquireMediaForRender() : nullptr;
}

ReplayEngine::ReplayEngine(QObject *parent) : QObject(parent), settings_(SettingsStore::load())
{
	instance_ = this;
	connect(this, &ReplayEngine::statusChanged, this, [this](const QString &message) {
		bridgeStatus_ = message.toStdString();
		bridgeError_.clear();
		writeBridgeState();
	});
	connect(this, &ReplayEngine::errorRaised, this, [this](const QString &message) {
		bridgeError_ = message.toStdString();
		writeBridgeState();
	});
	connect(this, &ReplayEngine::bufferStateChanged, this, [this](bool) { writeBridgeState(); });
	connect(this, &ReplayEngine::savingStateChanged, this, [this](bool) { writeBridgeState(); });
	connect(this, &ReplayEngine::eventStateChanged, this, [this](bool) { writeBridgeState(); });
	connect(this, &ReplayEngine::timelineChanged, this, [this]() { writeBridgeState(); });
	connect(this, &ReplayEngine::playbackStateChanged, this, [this](bool) { writeBridgeState(); });
	connect(this, &ReplayEngine::activeCameraChanged, this, [this](int) { writeBridgeState(); });
	if (settings_.sourceUuids.size() < 2)
		settings_.sourceUuids.resize(2);
	rebuildCaptures();

	playbackTimer_.setInterval(20);
	playbackTimer_.setTimerType(Qt::PreciseTimer);
	connect(&playbackTimer_, &QTimer::timeout, this, &ReplayEngine::pollPlayback);
	bridgeCommandTimer_.setInterval(150);
	connect(&bridgeCommandTimer_, &QTimer::timeout, this, &ReplayEngine::pollBridgeCommand);
	bridgeCommandTimer_.start();
	bridgeStatus_ = "Plugin listo";
	writeBridgeState();
}

ReplayEngine::~ReplayEngine()
{
	bridgeCommandTimer_.stop();
	cleanupTemporaryMedia();
	if (previousScene_) {
		obs_source_release(previousScene_);
		previousScene_ = nullptr;
	}
	bridgeLoaded_ = false;
	writeBridgeState();
	instance_ = nullptr;
}

ReplayEngine *ReplayEngine::instance() noexcept
{
	return instance_;
}

const Settings &ReplayEngine::settings() const noexcept
{
	return settings_;
}

void ReplayEngine::updateSettings(const Settings &settings)
{
	const auto previousCameraCount = settings_.sourceUuids.size();
	settings_ = settings;
	if (settings_.sourceUuids.size() < 2)
		settings_.sourceUuids.resize(2);
	settings_.bufferSeconds = std::clamp(settings_.bufferSeconds, 10, 60);
	settings_.replayWindowSeconds = std::clamp(settings_.replayWindowSeconds, 3, settings_.bufferSeconds);
	settings_.segmentSeconds = std::clamp(settings_.segmentSeconds, 1, 10);
	settings_.bitrateKbps = std::clamp(settings_.bitrateKbps, 4000, 50000);
	if (settings_.sourceUuids.size() != previousCameraCount) {
		stopBuffers();
		releaseMedia();
		eventReady_ = false;
		timeline_.clear();
		rebuildCaptures();
		emit eventStateChanged(false);
		emit timelineChanged();
	}
	SettingsStore::save(settings_);
	writeBridgeState();
}

void ReplayEngine::rebuildCaptures()
{
	captures_.clear();
	captures_.reserve(settings_.sourceUuids.size());
	for (std::size_t index = 0; index < settings_.sourceUuids.size(); ++index) {
		auto capture = std::make_unique<IsoCapture>(static_cast<int>(index));
		capture->setSavedCallback([this](int cameraIndex, const std::string &path) {
			QMetaObject::invokeMethod(
				this, [this, cameraIndex, path]() { handleCaptureSaved(cameraIndex, path); },
				Qt::QueuedConnection);
		});
		captures_.push_back(std::move(capture));
	}
	savedPaths_.assign(captures_.size(), {});
	{
		std::lock_guard<std::mutex> lock(mediaMutex_);
		mediaSources_.assign(captures_.size(), nullptr);
		activeCamera_ = 0;
	}
}

std::vector<EncoderOption> ReplayEngine::availableVideoEncoders() const
{
	std::vector<EncoderOption> result;
	const char *id = nullptr;
	for (std::size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		if (!id || obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;
		const char *codec = obs_get_encoder_codec(id);
		if (!codec || (std::strcmp(codec, "h264") != 0 && std::strcmp(codec, "avc") != 0))
			continue;
		const auto caps = obs_get_encoder_caps(id);
		if ((caps & OBS_ENCODER_CAP_DEPRECATED) != 0 || (caps & OBS_ENCODER_CAP_INTERNAL) != 0)
			continue;
		const char *display = obs_encoder_get_display_name(id);
		result.push_back({id, display && *display ? display : id});
	}
	std::sort(result.begin(), result.end(),
		  [](const EncoderOption &left, const EncoderOption &right) { return left.name < right.name; });
	return result;
}

std::vector<std::pair<std::string, std::string>> ReplayEngine::availableVideoSources() const
{
	std::vector<std::pair<std::string, std::string>> result;
	obs_enum_sources(collectSource, &result);
	std::sort(result.begin(), result.end(),
		  [](const auto &left, const auto &right) { return left.second < right.second; });
	return result;
}

bool ReplayEngine::startBuffers()
{
	bridgeError_.clear();
	stopBuffers();
	releaseMedia();
	eventReady_ = false;
	timeline_.clear();
	emit eventStateChanged(false);
	emit timelineChanged();

	if (settings_.sourceUuids.size() < 2) {
		emit errorRaised(QStringLiteral("Configura al menos dos cámaras antes de activar el búfer."));
		return false;
	}
	if (captures_.size() != settings_.sourceUuids.size())
		rebuildCaptures();

	std::vector<obs_source_t *> sources(settings_.sourceUuids.size(), nullptr);
	std::unordered_set<std::string> uniqueSources;
	for (std::size_t index = 0; index < settings_.sourceUuids.size(); ++index) {
		const auto &uuid = settings_.sourceUuids[index];
		if (uuid.empty()) {
			emit errorRaised(
				QString("Selecciona una fuente válida para CAM %1.").arg(static_cast<int>(index + 1)));
			return false;
		}
		if (!uniqueSources.insert(uuid).second) {
			emit errorRaised(
				QString("CAM %1 repite una fuente ya seleccionada.").arg(static_cast<int>(index + 1)));
			return false;
		}
	}
	for (std::size_t index = 0; index < settings_.sourceUuids.size(); ++index) {
		const auto &uuid = settings_.sourceUuids[index];
		sources[index] = obs_get_source_by_uuid(uuid.c_str());
		if (!sources[index]) {
			for (obs_source_t *source : sources)
				if (source)
					obs_source_release(source);
			emit errorRaised(QString("OBS no encontró la fuente configurada para CAM %1.")
						 .arg(static_cast<int>(index + 1)));
			return false;
		}
	}

	const auto directory = SettingsStore::replayDirectory();
	bool started = true;
	std::size_t failedIndex = 0;
	for (std::size_t index = 0; index < captures_.size(); ++index) {
		if (!captures_[index]->start(sources[index], settings_.encoderId, directory, settings_.bufferSeconds,
					     settings_.bitrateKbps)) {
			started = false;
			failedIndex = index;
			break;
		}
	}
	for (obs_source_t *source : sources)
		obs_source_release(source);

	if (!started) {
		const auto reason = captures_[failedIndex]->lastError();
		stopBuffers();
		emit errorRaised(QString("CAM %1: %2")
					 .arg(static_cast<int>(failedIndex + 1))
					 .arg(QString::fromStdString(reason)));
		return false;
	}

	buffersActive_ = true;
	SettingsStore::save(settings_);
	emit bufferStateChanged(true);
	emit statusChanged(QString("Búfer activo · %1 cámaras · %2 s")
				   .arg(static_cast<int>(settings_.sourceUuids.size()))
				   .arg(settings_.bufferSeconds));
	return true;
}

void ReplayEngine::stopBuffers()
{
	for (auto &capture : captures_)
		capture->stop();
	const bool changed = buffersActive_;
	buffersActive_ = false;
	saving_ = false;
	if (changed)
		emit bufferStateChanged(false);
	emit savingStateChanged(false);
}

bool ReplayEngine::buffersActive() const noexcept
{
	return buffersActive_;
}

bool ReplayEngine::markReplay()
{
	if (!buffersActive_) {
		emit errorRaised(QStringLiteral("Activa primero el búfer multicámara."));
		return false;
	}
	if (saving_) {
		emit errorRaised(QStringLiteral("Todavía se está preparando la repetición anterior."));
		return false;
	}

	bridgeError_.clear();
	saving_ = true;
	eventReady_ = false;
	++eventSerial_;
	savedPaths_.assign(captures_.size(), {});
	emit savingStateChanged(true);
	emit eventStateChanged(false);
	emit statusChanged(QString("Guardando simultáneamente %1 cámaras…").arg(static_cast<int>(captures_.size())));

	bool saved = true;
	for (const auto &capture : captures_)
		saved = capture->saveReplay() && saved;
	if (!saved) {
		saving_ = false;
		emit savingStateChanged(false);
		emit errorRaised(QStringLiteral("No se pudieron guardar todos los búferes de forma sincronizada."));
		return false;
	}
	return true;
}

bool ReplayEngine::eventReady() const noexcept
{
	return eventReady_;
}

void ReplayEngine::writeBridgeState() const
{
	char *directory = obs_module_config_path("");
	if (!directory)
		return;
	const bool directoryReady = os_mkdirs(directory) != MKDIR_ERROR;
	bfree(directory);
	if (!directoryReady)
		return;

	char *path = obs_module_config_path("secretariatpro-bridge.json");
	if (!path)
		return;
	obs_data_t *data = obs_data_create();
	obs_data_set_int(data, "schema", 1);
	obs_data_set_string(data, "plugin_version", PLUGIN_VERSION);
	obs_data_set_bool(data, "loaded", bridgeLoaded_);
	obs_data_set_bool(data, "buffers_active", buffersActive_);
	obs_data_set_bool(data, "saving", saving_);
	obs_data_set_bool(data, "event_ready", eventReady_);
	obs_data_set_bool(data, "playing", playing_);
	obs_data_set_int(data, "event_serial", static_cast<long long>(eventSerial_));
	obs_data_set_int(data, "active_camera", static_cast<long long>(activeCamera_));
	obs_data_set_int(data, "camera_count", static_cast<long long>(settings_.sourceUuids.size()));
	obs_data_set_int(data, "timeline_segments", static_cast<long long>(timeline_.segments().size()));
	obs_data_set_string(data, "last_command_id", lastCommandId_.c_str());
	obs_data_set_int(data, "window_start_ms", timeline_.windowStartMs());
	obs_data_set_string(data, "status", bridgeStatus_.c_str());
	obs_data_set_string(data, "error", bridgeError_.c_str());
	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
				  std::chrono::system_clock::now().time_since_epoch())
				  .count();
	obs_data_set_int(data, "updated_at_unix_ms", static_cast<long long>(now));

	obs_data_array_t *paths = obs_data_array_create();
	for (std::size_t index = 0; index < savedPaths_.size(); ++index) {
		if (savedPaths_[index].empty())
			continue;
		obs_data_t *item = obs_data_create();
		obs_data_set_int(item, "camera", static_cast<long long>(index + 1));
		obs_data_set_string(item, "path", savedPaths_[index].c_str());
		obs_data_array_push_back(paths, item);
		obs_data_release(item);
	}
	obs_data_set_array(data, "saved_paths", paths);
	obs_data_array_release(paths);
	obs_data_array_t *segments = obs_data_array_create();
	for (const auto &segment : timeline_.segments()) {
		obs_data_t *item = obs_data_create();
		obs_data_set_int(item, "camera", static_cast<long long>(segment.cameraIndex + 1));
		obs_data_set_int(item, "duration_seconds", static_cast<long long>(segment.sourceDurationMs() / 1000));
		obs_data_set_int(item, "speed_percent", static_cast<long long>(segment.speedPercent));
		obs_data_set_int(item, "in_ms", segment.inMs);
		obs_data_set_int(item, "out_ms", segment.outMs);
		obs_data_array_push_back(segments, item);
		obs_data_release(item);
	}
	obs_data_set_array(data, "timeline", segments);
	obs_data_array_release(segments);
	(void)obs_data_save_json_safe(data, path, "tmp", "bak");
	obs_data_release(data);
	bfree(path);
}

void ReplayEngine::cleanupTemporaryMedia()
{
	out();
	stopBuffers();
	releaseMedia();
	timeline_.clear();
	savedPaths_.assign(captures_.size(), {});
	eventReady_ = false;
	const auto path = SettingsStore::replayDirectory();
	if (!path.empty()) {
		QDir directory(QString::fromStdString(path));
		for (const auto &name : directory.entryList({"SP-CAM*.mkv", "SP-CAM*.mp4"}, QDir::Files | QDir::NoSymLinks))
			if (!directory.remove(name))
				obs_log(LOG_WARNING, "Could not remove replay buffer file: %s", name.toUtf8().constData());
	}
	emit eventStateChanged(false);
	emit timelineChanged();
}

void ReplayEngine::pollBridgeCommand()
{
	char *path = obs_module_config_path("secretariatpro-command.json");
	if (!path)
		return;
	obs_data_t *command = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!command)
		return;
	const std::string commandId = obs_data_get_string(command, "command_id");
	if (commandId.empty() || commandId == lastCommandId_) {
		obs_data_release(command);
		return;
	}
	const std::string action = obs_data_get_string(command, "action");
	bool ok = false;
	if (action == "compose") {
		clearTimeline();
		obs_data_array_t *segments = obs_data_get_array(command, "segments");
		const auto count = segments ? obs_data_array_count(segments) : 0;
		ok = count > 0;
		for (std::size_t index = 0; ok && index < count; ++index) {
			obs_data_t *item = obs_data_array_item(segments, index);
			const auto camera = static_cast<std::size_t>(std::max<long long>(1, obs_data_get_int(item, "camera")) - 1);
			const int duration = static_cast<int>(obs_data_get_int(item, "duration_seconds"));
			const int speed = static_cast<int>(obs_data_get_int(item, "speed_percent"));
			ok = addSegment(camera, duration, speed);
			obs_data_release(item);
		}
		if (segments)
			obs_data_array_release(segments);
		if (ok && obs_data_get_bool(command, "play_now"))
			ok = take();
	} else if (action == "cleanup") {
		cleanupTemporaryMedia();
		ok = true;
	} else if (action == "clear_timeline") {
		clearTimeline();
		ok = true;
	}
	lastCommandId_ = commandId;
	bridgeError_ = ok ? "" : "No se pudo aplicar la composición solicitada por Secretariat Pro Live";
	writeBridgeState();
	obs_data_release(command);
}

bool ReplayEngine::addSegment(std::size_t cameraIndex, int durationSeconds, int speedPercent)
{
	if (!eventReady_ || cameraIndex >= mediaSources_.size())
		return false;
	const bool added = timeline_.add(cameraIndex, static_cast<std::int64_t>(durationSeconds) * 1000, speedPercent);
	if (added)
		emit timelineChanged();
	return added;
}

bool ReplayEngine::removeLastSegment()
{
	const bool removed = timeline_.removeLast();
	if (removed)
		emit timelineChanged();
	return removed;
}

void ReplayEngine::clearTimeline()
{
	timeline_.clear();
	emit timelineChanged();
}

const Timeline &ReplayEngine::timeline() const noexcept
{
	return timeline_;
}

bool ReplayEngine::take()
{
	bridgeError_.clear();
	if (!eventReady_) {
		emit errorRaised(QStringLiteral("Marca una jugada y espera a que se preparen todas las cámaras."));
		return false;
	}
	if (timeline_.empty()) {
		const auto remaining = timeline_.eventDurationMs() - timeline_.windowStartMs();
		if (!timeline_.add(0, remaining, 100)) {
			emit errorRaised(QStringLiteral("No se pudo crear el plano de repetición predeterminado."));
			return false;
		}
		emit timelineChanged();
	}
	obs_source_t *liveScene = obs_frontend_get_current_scene();
	if (!liveScene) {
		emit errorRaised(QStringLiteral("OBS no devolvió una escena de directo válida."));
		return false;
	}
	if (std::strcmp(obs_source_get_name(liveScene), settings_.replaySceneName.c_str()) == 0) {
		obs_source_release(liveScene);
		emit errorRaised(QStringLiteral("Selecciona una escena de directo antes de lanzar la repetición."));
		return false;
	}
	if (!ensureReplayScene(liveScene)) {
		obs_source_release(liveScene);
		return false;
	}

	obs_source_t *replayScene = obs_get_source_by_name(settings_.replaySceneName.c_str());
	if (!replayScene) {
		emit errorRaised(QStringLiteral("No se encontró la escena de repetición."));
		return false;
	}

	if (previousScene_)
		obs_source_release(previousScene_);
	previousScene_ = liveScene;

	obs_frontend_set_current_scene(replayScene);
	obs_source_release(replayScene);
	playing_ = true;
	segmentIndex_ = 0;
	emit playbackStateChanged(true);
	applySegment(0);
	emit statusChanged(QStringLiteral("REPLAY EN PROGRAMA"));
	return true;
}

void ReplayEngine::out()
{
	playbackTimer_.stop();
	for (obs_source_t *source : mediaSources_) {
		if (source)
			obs_source_media_play_pause(source, true);
	}

	if (previousScene_) {
		obs_frontend_set_current_scene(previousScene_);
		obs_source_release(previousScene_);
		previousScene_ = nullptr;
	}
	if (playing_) {
		playing_ = false;
		emit playbackStateChanged(false);
		emit statusChanged(QStringLiteral("Directo restaurado"));
	}
}

void ReplayEngine::switchCamera(std::size_t cameraIndex)
{
	if (!eventReady_ || cameraIndex >= mediaSources_.size())
		return;
	if (playing_) {
		const auto currentIndex = activeCamera_;
		const auto nextIndex = cameraIndex;
		if (mediaSources_[currentIndex] && mediaSources_[nextIndex]) {
			const auto currentTime = obs_source_media_get_time(mediaSources_[currentIndex]);
			if (currentTime >= 0)
				obs_source_media_set_time(mediaSources_[nextIndex], currentTime);
			obs_source_media_play_pause(mediaSources_[nextIndex], false);
		}
	}
	setActiveCamera(cameraIndex);
}

std::size_t ReplayEngine::activeCamera() const noexcept
{
	return activeCamera_;
}

std::size_t ReplayEngine::cameraCount() const noexcept
{
	return settings_.sourceUuids.size();
}

bool ReplayEngine::playing() const noexcept
{
	return playing_;
}

obs_source_t *ReplayEngine::acquireMediaForRender() const
{
	std::lock_guard<std::mutex> lock(mediaMutex_);
	return activeCamera_ < mediaSources_.size() && mediaSources_[activeCamera_]
		       ? obs_source_get_ref(mediaSources_[activeCamera_])
		       : nullptr;
}

void ReplayEngine::handleCaptureSaved(int cameraIndex, const std::string &path)
{
	if (!saving_ || cameraIndex < 0 || static_cast<std::size_t>(cameraIndex) >= savedPaths_.size())
		return;
	savedPaths_[static_cast<std::size_t>(cameraIndex)] = path;
	if (std::any_of(savedPaths_.begin(), savedPaths_.end(), [](const std::string &saved) { return saved.empty(); }))
		return;

	saving_ = false;
	emit savingStateChanged(false);
	loadEventMedia();
}

void ReplayEngine::loadEventMedia()
{
	releaseMedia();
	std::vector<obs_source_t *> created(savedPaths_.size(), nullptr);
	for (std::size_t index = 0; index < savedPaths_.size(); ++index)
		created[index] = createMediaSource(static_cast<int>(index), savedPaths_[index]);
	if (std::any_of(created.begin(), created.end(), [](obs_source_t *source) { return source == nullptr; })) {
		for (obs_source_t *source : created) {
			if (source) {
				obs_source_dec_active(source);
				obs_source_dec_showing(source);
				obs_source_release(source);
			}
		}
		emit errorRaised(QStringLiteral(
			"OBS guardó los clips, pero no pudo abrir todas las cámaras como fuentes multimedia."));
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mediaMutex_);
		mediaSources_ = std::move(created);
	}
	QTimer::singleShot(100, this, [this]() {
		for (obs_source_t *source : mediaSources_)
			if (source)
				obs_source_media_play_pause(source, true);
		pollMediaDuration(30);
	});
}

void ReplayEngine::pollMediaDuration(int attemptsRemaining)
{
	if (mediaSources_.empty() ||
	    std::any_of(mediaSources_.begin(), mediaSources_.end(), [](obs_source_t *source) { return !source; }))
		return;
	std::int64_t duration = std::numeric_limits<std::int64_t>::max();
	for (obs_source_t *source : mediaSources_) {
		const auto cameraDuration = obs_source_media_get_duration(source);
		if (cameraDuration <= 0) {
			duration = 0;
			break;
		}
		duration = std::min(duration, cameraDuration);
	}
	if (duration > 0) {
		timeline_.reset(duration, static_cast<std::int64_t>(settings_.replayWindowSeconds) * 1000);
		eventReady_ = true;
		setActiveCamera(0);
		emit eventStateChanged(true);
		emit timelineChanged();
		emit statusChanged(
			QString("Repetición preparada · %1 s disponibles").arg(duration / 1000.0, 0, 'f', 1));
		return;
	}
	if (attemptsRemaining <= 0) {
		emit errorRaised(QStringLiteral("Los clips se guardaron, pero OBS no pudo determinar su duración."));
		return;
	}
	QTimer::singleShot(100, this, [this, attemptsRemaining]() { pollMediaDuration(attemptsRemaining - 1); });
}

void ReplayEngine::releaseMedia()
{
	std::vector<obs_source_t *> old;
	{
		std::lock_guard<std::mutex> lock(mediaMutex_);
		old.swap(mediaSources_);
	}
	for (obs_source_t *source : old) {
		if (!source)
			continue;
		obs_source_media_stop(source);
		obs_source_dec_active(source);
		obs_source_dec_showing(source);
		obs_source_release(source);
	}
}

obs_source_t *ReplayEngine::createMediaSource(int cameraIndex, const std::string &path)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", path.c_str());
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "clear_on_media_end", true);
	obs_data_set_bool(settings, "hw_decode", true);
	obs_data_set_int(settings, "speed_percent", 100);
	const std::string name = "SP Replay Private CAM " + std::to_string(cameraIndex + 1);
	obs_source_t *source = obs_source_create_private("ffmpeg_source", name.c_str(), settings);
	obs_data_release(settings);
	if (!source)
		return nullptr;

	obs_source_set_volume(source, 0.0f);
	obs_source_inc_showing(source);
	obs_source_inc_active(source);
	obs_source_media_restart(source);
	return source;
}

bool ReplayEngine::ensureReplayScene(obs_source_t *liveScene)
{
	obs_source_t *output = obs_get_source_by_name(ReplayOutputName);
	if (output && std::strcmp(obs_source_get_id(output), "secretariatpro_replay_output") != 0) {
		obs_source_release(output);
		emit errorRaised(QStringLiteral("Ya existe una fuente llamada 'SP Replay Output' de otro tipo."));
		return false;
	}
	if (!output)
		output = obs_source_create("secretariatpro_replay_output", ReplayOutputName, nullptr, nullptr);
	if (!output) {
		emit errorRaised(QStringLiteral("No se pudo crear la fuente de salida de replay."));
		return false;
	}

	obs_source_t *sceneSource = obs_get_source_by_name(settings_.replaySceneName.c_str());
	obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;
	bool releaseScene = false;
	if (!sceneSource) {
		scene = obs_scene_create(settings_.replaySceneName.c_str());
		releaseScene = true;
	}
	if (!scene) {
		if (sceneSource)
			obs_source_release(sceneSource);
		obs_source_release(output);
		emit errorRaised(QStringLiteral(
			"El nombre configurado para la escena ya pertenece a una fuente que no es una escena."));
		return false;
	}

	obs_sceneitem_t *item = obs_scene_find_source(scene, ReplayOutputName);
	if (!item)
		item = obs_scene_add(scene, output);
	obs_sceneitem_t *liveItem = liveScene ? obs_scene_find_source(scene, obs_source_get_name(liveScene)) : nullptr;
	if (!liveItem && liveScene)
		liveItem = obs_scene_add(scene, liveScene);
	ReplaySceneItems targets{output, liveScene};
	obs_scene_enum_items(scene, configureReplayItem, &targets);
	if (item) {
		obs_video_info info = {};
		if (obs_get_video_info(&info)) {
			vec2 position;
			vec2 bounds;
			vec2_set(&position, 0.0f, 0.0f);
			vec2_set(&bounds, static_cast<float>(info.base_width), static_cast<float>(info.base_height));
			obs_sceneitem_set_pos(item, &position);
			obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
			obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_STRETCH);
			obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
			obs_sceneitem_set_bounds(item, &bounds);
		}
	}
	if (liveItem)
		obs_sceneitem_set_order(liveItem, OBS_ORDER_MOVE_BOTTOM);
	if (item)
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);

	if (releaseScene)
		obs_scene_release(scene);
	if (sceneSource)
		obs_source_release(sceneSource);
	obs_source_release(output);
	return item != nullptr;
}

void ReplayEngine::applySegment(std::size_t index)
{
	if (index >= timeline_.segments().size()) {
		finishPlayback();
		return;
	}
	segmentIndex_ = index;
	const auto &segment = timeline_.segments()[segmentIndex_];
	setActiveCamera(segment.cameraIndex);
	setMediaSpeedAndTime(segment.speedPercent, segment.inMs);
	playbackTimer_.start();
}

void ReplayEngine::pollPlayback()
{
	if (!playing_ || segmentIndex_ >= timeline_.segments().size())
		return;
	const auto &segment = timeline_.segments()[segmentIndex_];
	obs_source_t *source = activeCamera_ < mediaSources_.size() ? mediaSources_[activeCamera_] : nullptr;
	if (!source)
		return;
	const auto time = obs_source_media_get_time(source);
	if (time >= segment.outMs - 15)
		applySegment(segmentIndex_ + 1);
}

void ReplayEngine::setMediaSpeedAndTime(int speedPercent, std::int64_t timeMs)
{
	for (obs_source_t *source : mediaSources_) {
		if (!source)
			continue;
		obs_source_media_play_pause(source, true);
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_set_int(settings, "speed_percent", speedPercent);
		obs_source_update(source, settings);
		obs_data_release(settings);
		obs_source_media_set_time(source, timeMs);
		obs_source_media_play_pause(source, false);
	}
}

void ReplayEngine::finishPlayback()
{
	out();
}

void ReplayEngine::setActiveCamera(std::size_t cameraIndex)
{
	if (cameraIndex >= mediaSources_.size())
		return;
	{
		std::lock_guard<std::mutex> lock(mediaMutex_);
		activeCamera_ = cameraIndex;
	}
	emit activeCameraChanged(static_cast<int>(cameraIndex));
}

} // namespace sp::replay
