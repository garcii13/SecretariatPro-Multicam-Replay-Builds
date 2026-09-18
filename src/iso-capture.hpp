#pragma once

#include <functional>
#include <string>

struct obs_encoder;
struct obs_output;
struct obs_source;
struct obs_view;
struct calldata;
typedef struct obs_encoder obs_encoder_t;
typedef struct obs_output obs_output_t;
typedef struct obs_source obs_source_t;
typedef struct obs_view obs_view_t;
typedef struct calldata calldata_t;
typedef struct video_output video_t;

namespace sp::replay {

class IsoCapture {
public:
	using SavedCallback = std::function<void(int cameraIndex, const std::string &path)>;

	explicit IsoCapture(int cameraIndex);
	~IsoCapture();

	IsoCapture(const IsoCapture &) = delete;
	IsoCapture &operator=(const IsoCapture &) = delete;

	[[nodiscard]] bool start(obs_source_t *source, const std::string &encoderId, const std::string &directory,
				 int bufferSeconds, int bitrateKbps);
	void stop();
	[[nodiscard]] bool saveReplay();
	[[nodiscard]] bool active() const noexcept;
	[[nodiscard]] const std::string &lastError() const noexcept;
	void setSavedCallback(SavedCallback callback);

private:
	static void handleSaved(void *data, calldata_t *params);
	void onSaved();
	[[nodiscard]] static std::string chooseAudioEncoder();

	int cameraIndex_{0};
	obs_view_t *view_{nullptr};
	video_t *video_{nullptr};
	obs_encoder_t *videoEncoder_{nullptr};
	obs_encoder_t *audioEncoder_{nullptr};
	obs_output_t *output_{nullptr};
	std::string lastError_;
	SavedCallback savedCallback_;
};

} // namespace sp::replay
