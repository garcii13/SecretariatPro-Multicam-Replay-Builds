#pragma once

#include "iso-capture.hpp"
#include "replay-settings.hpp"
#include "replay-timeline.hpp"

#include <QObject>
#include <QTimer>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct obs_source;
typedef struct obs_source obs_source_t;

namespace sp::replay {

struct EncoderOption {
	std::string id;
	std::string name;
};

class ReplayEngine final : public QObject {
	Q_OBJECT

public:
	explicit ReplayEngine(QObject *parent = nullptr);
	~ReplayEngine() override;

	ReplayEngine(const ReplayEngine &) = delete;
	ReplayEngine &operator=(const ReplayEngine &) = delete;

	static ReplayEngine *instance() noexcept;

	[[nodiscard]] const Settings &settings() const noexcept;
	void updateSettings(const Settings &settings);
	[[nodiscard]] std::vector<EncoderOption> availableVideoEncoders() const;
	[[nodiscard]] std::vector<std::pair<std::string, std::string>> availableVideoSources() const;

	[[nodiscard]] bool startBuffers();
	void stopBuffers();
	[[nodiscard]] bool buffersActive() const noexcept;
	[[nodiscard]] bool markReplay();
	[[nodiscard]] bool eventReady() const noexcept;

	[[nodiscard]] bool addSegment(std::size_t cameraIndex, int durationSeconds, int speedPercent);
	[[nodiscard]] bool removeLastSegment();
	void clearTimeline();
	[[nodiscard]] const Timeline &timeline() const noexcept;

	[[nodiscard]] bool take();
	void out();
	void switchCamera(std::size_t cameraIndex);
	[[nodiscard]] std::size_t activeCamera() const noexcept;
	[[nodiscard]] std::size_t cameraCount() const noexcept;
	[[nodiscard]] bool playing() const noexcept;

	[[nodiscard]] obs_source_t *acquireMediaForRender() const;

signals:
	void statusChanged(const QString &message);
	void errorRaised(const QString &message);
	void bufferStateChanged(bool active);
	void savingStateChanged(bool saving);
	void eventStateChanged(bool ready);
	void timelineChanged();
	void playbackStateChanged(bool playing);
	void activeCameraChanged(int cameraIndex);

private:
	void handleCaptureSaved(int cameraIndex, const std::string &path);
	void loadEventMedia();
	void pollMediaDuration(int attemptsRemaining);
	void releaseMedia();
	[[nodiscard]] obs_source_t *createMediaSource(int cameraIndex, const std::string &path);
	[[nodiscard]] bool ensureReplayScene(obs_source_t *liveScene);
	void applySegment(std::size_t index);
	void pollPlayback();
	void setMediaSpeedAndTime(int speedPercent, std::int64_t timeMs);
	void finishPlayback();
	void setActiveCamera(std::size_t cameraIndex);
	void rebuildCaptures();
	void writeBridgeState() const;

	static ReplayEngine *instance_;

	Settings settings_;
	std::vector<std::unique_ptr<IsoCapture>> captures_;
	std::vector<std::string> savedPaths_;
	std::vector<obs_source_t *> mediaSources_;
	obs_source_t *previousScene_{nullptr};
	Timeline timeline_;
	QTimer playbackTimer_;
	std::size_t segmentIndex_{0};
	std::size_t activeCamera_{0};
	mutable std::mutex mediaMutex_;
	bool buffersActive_{false};
	bool saving_{false};
	bool eventReady_{false};
	bool playing_{false};
	bool bridgeLoaded_{true};
	std::uint64_t eventSerial_{0};
	std::string bridgeStatus_;
	std::string bridgeError_;
};

} // namespace sp::replay
