#include "iso-capture.hpp"
#include "plugin-support.h"

#include <obs-module.h>
#include <util/platform.h>

#include <cstring>
#include <utility>

namespace sp::replay {

IsoCapture::IsoCapture(int cameraIndex) : cameraIndex_(cameraIndex) {}

IsoCapture::~IsoCapture()
{
	stop();
}

bool IsoCapture::start(obs_source_t *source, const std::string &encoderId, const std::string &directory,
		       int bufferSeconds, int bitrateKbps)
{
	stop();
	lastError_.clear();

	if (!source) {
		lastError_ = "No se ha seleccionado una fuente de vídeo";
		return false;
	}
	if (directory.empty() || os_mkdirs(directory.c_str()) == MKDIR_ERROR) {
		lastError_ = "No se pudo crear el directorio temporal de repeticiones";
		return false;
	}

	view_ = obs_view_create();
	if (!view_) {
		lastError_ = "OBS no pudo crear la vista ISO";
		return false;
	}
	obs_view_set_source(view_, 0, source);
	video_ = obs_view_add(view_);
	if (!video_) {
		lastError_ = "OBS no pudo añadir la vista ISO al motor de vídeo";
		stop();
		return false;
	}

	obs_data_t *videoSettings = obs_data_create();
	obs_data_set_string(videoSettings, "rate_control", "CBR");
	obs_data_set_int(videoSettings, "bitrate", bitrateKbps);
	obs_data_set_int(videoSettings, "keyint_sec", 1);
	obs_data_set_int(videoSettings, "bf", 0);
	obs_data_set_string(videoSettings, "preset", "veryfast");
	obs_data_set_string(videoSettings, "profile", "high");
	obs_data_set_string(videoSettings, "tune", "zerolatency");
	obs_data_set_int(videoSettings, "threads", 2);

	const std::string videoName = "SP Replay CAM " + std::to_string(cameraIndex_ + 1) + " Video";
	videoEncoder_ = obs_video_encoder_create(encoderId.c_str(), videoName.c_str(), videoSettings, nullptr);
	obs_data_release(videoSettings);
	if (!videoEncoder_) {
		lastError_ = "No se pudo iniciar el codificador H.264 seleccionado: " + encoderId;
		stop();
		return false;
	}
	obs_encoder_set_video(videoEncoder_, video_);

	const auto audioId = chooseAudioEncoder();
	obs_data_t *audioSettings = obs_data_create();
	obs_data_set_int(audioSettings, "bitrate", 128);
	const std::string audioName = "SP Replay CAM " + std::to_string(cameraIndex_ + 1) + " Audio";
	audioEncoder_ = obs_audio_encoder_create(audioId.c_str(), audioName.c_str(), audioSettings, 0, nullptr);
	obs_data_release(audioSettings);
	if (!audioEncoder_) {
		lastError_ = "No se pudo iniciar un codificador AAC para el búfer";
		stop();
		return false;
	}
	obs_encoder_set_audio(audioEncoder_, obs_get_audio());

	obs_data_t *outputSettings = obs_data_create();
	obs_data_set_string(outputSettings, "directory", directory.c_str());
	const std::string filename = "SP-CAM" + std::to_string(cameraIndex_ + 1) + "-%CCYY-%MM-%DD-%hh-%mm-%ss";
	obs_data_set_string(outputSettings, "format", filename.c_str());
	obs_data_set_string(outputSettings, "extension", "mkv");
	obs_data_set_bool(outputSettings, "allow_spaces", false);
	obs_data_set_int(outputSettings, "max_time_sec", bufferSeconds);
	obs_data_set_int(outputSettings, "max_size_mb", 0);

	const std::string outputName = "SP Replay CAM " + std::to_string(cameraIndex_ + 1) + " Buffer";
	output_ = obs_output_create("replay_buffer", outputName.c_str(), outputSettings, nullptr);
	obs_data_release(outputSettings);
	if (!output_) {
		lastError_ = "El módulo obs-ffmpeg no ofrece la salida replay_buffer";
		stop();
		return false;
	}

	obs_output_set_video_encoder(output_, videoEncoder_);
	obs_output_set_audio_encoder(output_, audioEncoder_, 0);
	signal_handler_connect(obs_output_get_signal_handler(output_), "saved", &IsoCapture::handleSaved, this);

	if (!obs_output_start(output_)) {
		const char *reason = obs_output_get_last_error(output_);
		lastError_ = reason && *reason ? reason : "OBS rechazó el inicio del búfer ISO";
		stop();
		return false;
	}

	obs_log(LOG_INFO, "CAM %d ISO buffer started with encoder '%s'", cameraIndex_ + 1, encoderId.c_str());
	return true;
}

void IsoCapture::stop()
{
	if (output_) {
		signal_handler_disconnect(obs_output_get_signal_handler(output_), "saved", &IsoCapture::handleSaved,
					  this);
		if (obs_output_active(output_))
			obs_output_force_stop(output_);
		obs_output_release(output_);
		output_ = nullptr;
	}

	if (videoEncoder_) {
		obs_encoder_set_video(videoEncoder_, nullptr);
		obs_encoder_release(videoEncoder_);
		videoEncoder_ = nullptr;
	}
	if (audioEncoder_) {
		obs_encoder_set_audio(audioEncoder_, nullptr);
		obs_encoder_release(audioEncoder_);
		audioEncoder_ = nullptr;
	}
	if (view_) {
		if (video_)
			obs_view_remove(view_);
		obs_view_set_source(view_, 0, nullptr);
		obs_view_destroy(view_);
		view_ = nullptr;
		video_ = nullptr;
	}
}

bool IsoCapture::saveReplay()
{
	if (!active()) {
		lastError_ = "El búfer de esta cámara no está activo";
		return false;
	}

	calldata_t params = {};
	const bool called = proc_handler_call(obs_output_get_proc_handler(output_), "save", &params);
	calldata_free(&params);
	if (!called)
		lastError_ = "La salida replay_buffer no aceptó la orden de guardado";
	return called;
}

bool IsoCapture::active() const noexcept
{
	return output_ && obs_output_active(output_);
}

const std::string &IsoCapture::lastError() const noexcept
{
	return lastError_;
}

void IsoCapture::setSavedCallback(SavedCallback callback)
{
	savedCallback_ = std::move(callback);
}

void IsoCapture::handleSaved(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	static_cast<IsoCapture *>(data)->onSaved();
}

void IsoCapture::onSaved()
{
	if (!output_)
		return;

	calldata_t params = {};
	proc_handler_call(obs_output_get_proc_handler(output_), "get_last_replay", &params);
	const char *path = calldata_string(&params, "path");
	const std::string savedPath = path ? path : "";
	calldata_free(&params);

	if (!savedPath.empty() && savedCallback_)
		savedCallback_(cameraIndex_, savedPath);
}

std::string IsoCapture::chooseAudioEncoder()
{
	const char *preferred[] = {"ffmpeg_aac", "CoreAudio_AAC"};
	for (const char *id : preferred) {
		const char *codec = obs_get_encoder_codec(id);
		if (codec && std::strcmp(codec, "aac") == 0)
			return id;
	}

	const char *id = nullptr;
	for (std::size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		if (!id || obs_get_encoder_type(id) != OBS_ENCODER_AUDIO)
			continue;
		const char *codec = obs_get_encoder_codec(id);
		if (codec && std::strcmp(codec, "aac") == 0)
			return id;
	}
	return "ffmpeg_aac";
}

} // namespace sp::replay
