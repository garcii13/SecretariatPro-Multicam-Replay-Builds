#pragma once

#include "replay-engine.hpp"

#include <QWidget>

#include <cstddef>
#include <vector>

class QComboBox;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

namespace sp::replay {

class ReplayDock final : public QWidget {
	Q_OBJECT

public:
	explicit ReplayDock(ReplayEngine *engine, QWidget *parent = nullptr);

private:
	struct CameraRow {
		QWidget *container{nullptr};
		QLabel *label{nullptr};
		QComboBox *source{nullptr};
	};

	void buildUi();
	void syncCameraRows(std::size_t count);
	void rebuildCameraControls();
	void populateSources();
	void populateEncoders();
	void loadSettings();
	void persistUiSettings();
	void addCamera();
	void removeCamera();
	void refreshTimeline();
	void refreshEnabledState();
	void setCameraHighlight(int cameraIndex);

	ReplayEngine *engine_{nullptr};
	std::vector<CameraRow> cameraRows_;
	std::vector<QPushButton *> liveCameraButtons_;
	QWidget *cameraRowsContainer_{nullptr};
	QVBoxLayout *cameraRowsLayout_{nullptr};
	QGridLayout *liveCameraLayout_{nullptr};
	QComboBox *encoder_{nullptr};
	QComboBox *speed_{nullptr};
	QComboBox *segmentCamera_{nullptr};
	QSpinBox *bufferSeconds_{nullptr};
	QSpinBox *windowSeconds_{nullptr};
	QSpinBox *segmentSeconds_{nullptr};
	QSpinBox *bitrate_{nullptr};
	QPushButton *addCameraButton_{nullptr};
	QPushButton *removeCameraButton_{nullptr};
	QPushButton *armButton_{nullptr};
	QPushButton *markButton_{nullptr};
	QPushButton *addSegmentButton_{nullptr};
	QPushButton *removeSegment_{nullptr};
	QPushButton *clearTimeline_{nullptr};
	QPushButton *takeButton_{nullptr};
	QPushButton *outButton_{nullptr};
	QListWidget *timeline_{nullptr};
	QLabel *status_{nullptr};
	bool saving_{false};
};

} // namespace sp::replay
