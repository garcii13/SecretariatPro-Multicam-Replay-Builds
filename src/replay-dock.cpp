#include "replay-dock.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace sp::replay {
namespace {

QPushButton *makeAction(const QString &text, const char *role = nullptr)
{
	auto *button = new QPushButton(text);
	button->setMinimumHeight(38);
	if (role)
		button->setProperty("role", role);
	return button;
}

} // namespace

ReplayDock::ReplayDock(ReplayEngine *engine, QWidget *parent) : QWidget(parent), engine_(engine)
{
	setObjectName("SecretariatProReplayDock");
	setWindowTitle("SecretariatPro Replay");
	buildUi();
	populateEncoders();
	loadSettings();
	populateSources();
	refreshTimeline();
	refreshEnabledState();

	connect(engine_, &ReplayEngine::statusChanged, status_, &QLabel::setText);
	connect(engine_, &ReplayEngine::errorRaised, this, [this](const QString &message) {
		status_->setText(message);
		QMessageBox::warning(this, "SecretariatPro Replay", message);
	});
	connect(engine_, &ReplayEngine::bufferStateChanged, this, [this](bool) { refreshEnabledState(); });
	connect(engine_, &ReplayEngine::savingStateChanged, this, [this](bool saving) {
		saving_ = saving;
		refreshEnabledState();
	});
	connect(engine_, &ReplayEngine::eventStateChanged, this, [this](bool) { refreshEnabledState(); });
	connect(engine_, &ReplayEngine::timelineChanged, this, &ReplayDock::refreshTimeline);
	connect(engine_, &ReplayEngine::playbackStateChanged, this, [this](bool) { refreshEnabledState(); });
	connect(engine_, &ReplayEngine::activeCameraChanged, this, &ReplayDock::setCameraHighlight);
}

void ReplayDock::buildUi()
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	auto *content = new QWidget(scroll);
	auto *root = new QVBoxLayout(content);
	root->setContentsMargins(10, 10, 10, 10);
	root->setSpacing(9);

	auto *title = new QLabel("SECRETARIATPRO  ·  MULTICAM REPLAY");
	title->setObjectName("ReplayTitle");
	root->addWidget(title);

	auto *bufferGroup = new QGroupBox("Captura y programa");
	auto *bufferLayout = new QGridLayout(bufferGroup);
	bufferSeconds_ = new QSpinBox;
	bufferSeconds_->setRange(10, 60);
	bufferSeconds_->setSuffix(" s");
	windowSeconds_ = new QSpinBox;
	windowSeconds_->setRange(3, 30);
	windowSeconds_->setSuffix(" s");
	bufferLayout->addWidget(new QLabel("Búfer"), 0, 0);
	bufferLayout->addWidget(bufferSeconds_, 0, 1);
	bufferLayout->addWidget(new QLabel("Ventana de replay"), 0, 2);
	bufferLayout->addWidget(windowSeconds_, 0, 3);
	armButton_ = makeAction("ACTIVAR BÚFER", "arm");
	markButton_ = makeAction("MARCAR", "mark");
	takeButton_ = makeAction("LANZAR REPLAY", "take");
	outButton_ = makeAction("DIRECTO", "out");
	bufferLayout->addWidget(armButton_, 1, 0, 1, 2);
	bufferLayout->addWidget(takeButton_, 1, 2, 1, 2);
	bufferLayout->addWidget(markButton_, 2, 0, 1, 2);
	bufferLayout->addWidget(outButton_, 2, 2, 1, 2);
	root->addWidget(bufferGroup);

	status_ = new QLabel("Configura al menos dos cámaras y activa el búfer");
	status_->setObjectName("ReplayStatus");
	status_->setWordWrap(true);
	root->addWidget(status_);
	auto *hotkeyHint = new QLabel("Atajos configurables en Ajustes de OBS → Atajos → SecretariatPro Replay");
	hotkeyHint->setObjectName("ReplayHint");
	hotkeyHint->setWordWrap(true);
	root->addWidget(hotkeyHint);

	auto *sources = new QGroupBox("Fuentes ISO");
	auto *sourceLayout = new QVBoxLayout(sources);
	cameraRowsContainer_ = new QWidget(sources);
	cameraRowsLayout_ = new QVBoxLayout(cameraRowsContainer_);
	cameraRowsLayout_->setContentsMargins(0, 0, 0, 0);
	cameraRowsLayout_->setSpacing(5);
	sourceLayout->addWidget(cameraRowsContainer_);

	auto *cameraEditLayout = new QHBoxLayout;
	addCameraButton_ = new QPushButton("+ AÑADIR CÁMARA");
	removeCameraButton_ = new QPushButton("QUITAR ÚLTIMA");
	cameraEditLayout->addWidget(addCameraButton_);
	cameraEditLayout->addWidget(removeCameraButton_);
	sourceLayout->addLayout(cameraEditLayout);

	auto *sourceForm = new QFormLayout;
	encoder_ = new QComboBox;
	bitrate_ = new QSpinBox;
	bitrate_->setRange(4000, 50000);
	bitrate_->setSingleStep(1000);
	bitrate_->setSuffix(" kbps");
	sourceForm->addRow("Codificador", encoder_);
	sourceForm->addRow("Bitrate por cámara", bitrate_);
	auto *refreshSources = new QPushButton("Actualizar lista de fuentes");
	sourceForm->addRow(QString(), refreshSources);
	sourceLayout->addLayout(sourceForm);
	root->addWidget(sources);

	auto *sequence = new QGroupBox("Secuencia");
	auto *sequenceLayout = new QVBoxLayout(sequence);
	auto *segmentControls = new QGridLayout;
	segmentCamera_ = new QComboBox;
	segmentSeconds_ = new QSpinBox;
	segmentSeconds_->setRange(1, 60);
	segmentSeconds_->setSuffix(" s");
	speed_ = new QComboBox;
	speed_->addItem("100 %", 100);
	speed_->addItem("75 %", 75);
	speed_->addItem("50 % · cámara lenta", 50);
	speed_->addItem("25 %", 25);
	addSegmentButton_ = makeAction("+ AÑADIR PLANO");
	segmentControls->addWidget(new QLabel("Cámara"), 0, 0);
	segmentControls->addWidget(segmentCamera_, 0, 1);
	segmentControls->addWidget(new QLabel("Duración"), 0, 2);
	segmentControls->addWidget(segmentSeconds_, 0, 3);
	segmentControls->addWidget(speed_, 1, 0, 1, 2);
	segmentControls->addWidget(addSegmentButton_, 1, 2, 1, 2);
	sequenceLayout->addLayout(segmentControls);

	timeline_ = new QListWidget;
	timeline_->setMinimumHeight(105);
	sequenceLayout->addWidget(timeline_);
	auto *editLayout = new QHBoxLayout;
	removeSegment_ = new QPushButton("Quitar último");
	clearTimeline_ = new QPushButton("Vaciar");
	editLayout->addWidget(removeSegment_);
	editLayout->addWidget(clearTimeline_);
	sequenceLayout->addLayout(editLayout);
	root->addWidget(sequence);

	auto *liveGroup = new QGroupBox("Cambio de ángulo en programa");
	liveCameraLayout_ = new QGridLayout(liveGroup);
	root->addWidget(liveGroup);

	root->addStretch(1);
	scroll->setWidget(content);
	outer->addWidget(scroll);
	setMinimumWidth(340);

	setStyleSheet(R"(
		#ReplayTitle { color: #61C7C9; font-weight: 700; font-size: 13px; padding: 4px 2px; }
		#ReplayStatus { border-left: 3px solid #61C7C9; padding: 7px; }
		#ReplayHint { color: palette(mid); font-size: 11px; padding: 0 2px 3px 2px; }
		QPushButton[role="arm"] { font-weight: 600; }
		QPushButton[role="mark"] { background: #8d5b12; color: white; font-weight: 700; }
		QPushButton[role="take"] { background: #087d80; color: white; font-weight: 700; }
		QPushButton[role="out"] { background: #8f2830; color: white; font-weight: 700; }
		QPushButton[role="camera"][active="true"] { border: 2px solid #61C7C9; font-weight: 700; }
	)");

	connect(bufferSeconds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
		windowSeconds_->setMaximum(value);
		persistUiSettings();
	});
	connect(windowSeconds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { persistUiSettings(); });
	connect(segmentSeconds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { persistUiSettings(); });
	connect(bitrate_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { persistUiSettings(); });
	connect(encoder_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { persistUiSettings(); });
	connect(refreshSources, &QPushButton::clicked, this, [this]() {
		persistUiSettings();
		populateSources();
	});
	connect(addCameraButton_, &QPushButton::clicked, this, &ReplayDock::addCamera);
	connect(removeCameraButton_, &QPushButton::clicked, this, &ReplayDock::removeCamera);

	connect(armButton_, &QPushButton::clicked, this, [this]() {
		persistUiSettings();
		if (engine_->buffersActive())
			engine_->stopBuffers();
		else
			(void)engine_->startBuffers();
	});
	connect(markButton_, &QPushButton::clicked, engine_, &ReplayEngine::markReplay);
	connect(addSegmentButton_, &QPushButton::clicked, this, [this]() {
		const auto cameraIndex = static_cast<std::size_t>(segmentCamera_->currentData().toInt());
		(void)engine_->addSegment(cameraIndex, segmentSeconds_->value(), speed_->currentData().toInt());
	});
	connect(removeSegment_, &QPushButton::clicked, engine_, &ReplayEngine::removeLastSegment);
	connect(clearTimeline_, &QPushButton::clicked, engine_, &ReplayEngine::clearTimeline);
	connect(takeButton_, &QPushButton::clicked, engine_, &ReplayEngine::take);
	connect(outButton_, &QPushButton::clicked, engine_, &ReplayEngine::out);
}

void ReplayDock::syncCameraRows(std::size_t count)
{
	count = std::max<std::size_t>(2, count);
	while (cameraRows_.size() < count) {
		const auto cameraIndex = cameraRows_.size();
		CameraRow row;
		row.container = new QWidget(cameraRowsContainer_);
		row.label = new QLabel(QString("CAM %1").arg(static_cast<int>(cameraIndex + 1)), row.container);
		row.source = new QComboBox(row.container);
		row.source->addItem("— Seleccionar —", QString());
		auto *layout = new QHBoxLayout(row.container);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(row.label);
		layout->addWidget(row.source, 1);
		cameraRowsLayout_->addWidget(row.container);
		connect(row.source, qOverload<int>(&QComboBox::currentIndexChanged), this,
			[this](int) { persistUiSettings(); });
		cameraRows_.push_back(row);
	}
	while (cameraRows_.size() > count) {
		auto row = cameraRows_.back();
		cameraRows_.pop_back();
		delete row.container;
	}
	for (std::size_t index = 0; index < cameraRows_.size(); ++index)
		cameraRows_[index].label->setText(QString("CAM %1").arg(static_cast<int>(index + 1)));
	rebuildCameraControls();
}

void ReplayDock::rebuildCameraControls()
{
	const int previousSelection = segmentCamera_->currentData().toInt();
	{
		const QSignalBlocker blocker(segmentCamera_);
		segmentCamera_->clear();
		for (std::size_t index = 0; index < cameraRows_.size(); ++index)
			segmentCamera_->addItem(QString("CAM %1").arg(static_cast<int>(index + 1)),
						static_cast<int>(index));
		const int selected = segmentCamera_->findData(previousSelection);
		segmentCamera_->setCurrentIndex(selected >= 0 ? selected : 0);
	}

	for (auto *button : liveCameraButtons_) {
		liveCameraLayout_->removeWidget(button);
		delete button;
	}
	liveCameraButtons_.clear();
	for (std::size_t index = 0; index < cameraRows_.size(); ++index) {
		auto *button = makeAction(QString("CAM %1").arg(static_cast<int>(index + 1)), "camera");
		liveCameraLayout_->addWidget(button, static_cast<int>(index / 3), static_cast<int>(index % 3));
		connect(button, &QPushButton::clicked, this, [this, index]() { engine_->switchCamera(index); });
		liveCameraButtons_.push_back(button);
	}
	setCameraHighlight(static_cast<int>(engine_->activeCamera()));
}

void ReplayDock::populateSources()
{
	const auto sources = engine_->availableVideoSources();
	const auto &settings = engine_->settings();
	for (std::size_t cameraIndex = 0; cameraIndex < cameraRows_.size(); ++cameraIndex) {
		auto *combo = cameraRows_[cameraIndex].source;
		const QSignalBlocker blocker(combo);
		combo->clear();
		combo->addItem("— Seleccionar —", QString());
		for (const auto &[uuid, name] : sources)
			combo->addItem(QString::fromStdString(name), QString::fromStdString(uuid));
		const std::string uuid = cameraIndex < settings.sourceUuids.size() ? settings.sourceUuids[cameraIndex]
										   : std::string{};
		const int selected = combo->findData(QString::fromStdString(uuid));
		combo->setCurrentIndex(selected >= 0 ? selected : 0);
	}
}

void ReplayDock::populateEncoders()
{
	const QSignalBlocker blocker(encoder_);
	encoder_->clear();
	encoder_->addItem(QStringLiteral("Automático · H.264 por hardware"), QStringLiteral("auto_hardware"));
	for (const auto &option : engine_->availableVideoEncoders())
		encoder_->addItem(QString::fromStdString(option.name), QString::fromStdString(option.id));
}

void ReplayDock::loadSettings()
{
	const auto &settings = engine_->settings();
	syncCameraRows(settings.sourceUuids.size());
	const QSignalBlocker b1(encoder_);
	const QSignalBlocker b2(bufferSeconds_);
	const QSignalBlocker b3(windowSeconds_);
	const QSignalBlocker b4(segmentSeconds_);
	const QSignalBlocker b5(bitrate_);

	const int encoderIndex = encoder_->findData(QString::fromStdString(settings.encoderId));
	encoder_->setCurrentIndex(encoderIndex >= 0 ? encoderIndex : (encoder_->count() > 0 ? 0 : -1));
	bufferSeconds_->setValue(settings.bufferSeconds);
	windowSeconds_->setMaximum(settings.bufferSeconds);
	windowSeconds_->setValue(settings.replayWindowSeconds);
	segmentSeconds_->setValue(settings.segmentSeconds);
	bitrate_->setValue(settings.bitrateKbps);
}

void ReplayDock::persistUiSettings()
{
	Settings settings = engine_->settings();
	settings.sourceUuids.clear();
	settings.sourceUuids.reserve(cameraRows_.size());
	for (const auto &row : cameraRows_)
		settings.sourceUuids.push_back(row.source->currentData().toString().toStdString());
	settings.encoderId = encoder_->currentData().toString().toStdString();
	settings.bufferSeconds = bufferSeconds_->value();
	settings.replayWindowSeconds = windowSeconds_->value();
	settings.segmentSeconds = segmentSeconds_->value();
	settings.bitrateKbps = bitrate_->value();
	engine_->updateSettings(settings);
}

void ReplayDock::addCamera()
{
	persistUiSettings();
	Settings settings = engine_->settings();
	settings.sourceUuids.emplace_back();
	engine_->updateSettings(settings);
	syncCameraRows(settings.sourceUuids.size());
	populateSources();
	refreshEnabledState();
}

void ReplayDock::removeCamera()
{
	if (cameraRows_.size() <= 2)
		return;
	persistUiSettings();
	Settings settings = engine_->settings();
	settings.sourceUuids.pop_back();
	engine_->updateSettings(settings);
	syncCameraRows(settings.sourceUuids.size());
	populateSources();
	refreshEnabledState();
}

void ReplayDock::refreshTimeline()
{
	timeline_->clear();
	int number = 1;
	for (const auto &segment : engine_->timeline().segments()) {
		const QString text = QString("%1.  %2   [%3 → %4]")
					     .arg(number++)
					     .arg(QString::fromStdString(segment.label()))
					     .arg(segment.inMs / 1000.0, 0, 'f', 1)
					     .arg(segment.outMs / 1000.0, 0, 'f', 1);
		timeline_->addItem(text);
	}
	refreshEnabledState();
}

void ReplayDock::refreshEnabledState()
{
	const bool armed = engine_->buffersActive();
	const bool ready = engine_->eventReady();
	const bool playing = engine_->playing();
	const bool canEdit = ready && !playing;

	armButton_->setEnabled(!playing);
	armButton_->setText(armed ? "DESACTIVAR BÚFER" : "ACTIVAR BÚFER");
	markButton_->setEnabled(armed && !saving_ && !playing);
	markButton_->setText(saving_ ? "PREPARANDO…" : "MARCAR");
	takeButton_->setEnabled(ready && !playing);
	outButton_->setEnabled(playing);
	for (const auto &row : cameraRows_)
		row.source->setEnabled(!armed && !playing);
	addCameraButton_->setEnabled(!armed && !playing);
	removeCameraButton_->setEnabled(!armed && !playing && cameraRows_.size() > 2);
	encoder_->setEnabled(!armed && !playing);
	bufferSeconds_->setEnabled(!armed && !playing);
	bitrate_->setEnabled(!armed && !playing);
	windowSeconds_->setEnabled(!armed && !playing);
	segmentCamera_->setEnabled(canEdit);
	addSegmentButton_->setEnabled(canEdit && engine_->timeline().segments().size() < Timeline::MaxSegments);
	segmentSeconds_->setEnabled(canEdit);
	speed_->setEnabled(canEdit);
	removeSegment_->setEnabled(canEdit && !engine_->timeline().empty());
	clearTimeline_->setEnabled(canEdit && !engine_->timeline().empty());
	for (auto *button : liveCameraButtons_)
		button->setEnabled(ready);
}

void ReplayDock::setCameraHighlight(int cameraIndex)
{
	for (std::size_t index = 0; index < liveCameraButtons_.size(); ++index) {
		auto *button = liveCameraButtons_[index];
		button->setProperty("active", cameraIndex >= 0 && static_cast<std::size_t>(cameraIndex) == index);
		button->style()->unpolish(button);
		button->style()->polish(button);
	}
}

} // namespace sp::replay
