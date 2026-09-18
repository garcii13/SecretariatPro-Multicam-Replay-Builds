#include "replay-timeline.hpp"

#include <algorithm>
#include <sstream>

namespace sp::replay {

std::int64_t Segment::sourceDurationMs() const noexcept
{
	return std::max<std::int64_t>(0, outMs - inMs);
}

std::int64_t Segment::playbackDurationMs() const noexcept
{
	if (!valid())
		return 0;
	return sourceDurationMs() * 100 / speedPercent;
}

bool Segment::valid() const noexcept
{
	return inMs >= 0 && outMs > inMs && (speedPercent == 50 || speedPercent == 100);
}

std::string Segment::label() const
{
	std::ostringstream stream;
	stream << "CAM " << (cameraIndex + 1) << "  " << sourceDurationMs() / 1000.0 << " s";
	if (speedPercent != 100)
		stream << "  " << speedPercent << '%';
	return stream.str();
}

void Timeline::reset(std::int64_t eventDurationMs, std::int64_t replayWindowMs)
{
	eventDurationMs_ = std::max<std::int64_t>(0, eventDurationMs);
	const auto window = std::clamp<std::int64_t>(replayWindowMs, 0, eventDurationMs_);
	windowStartMs_ = eventDurationMs_ - window;
	cursorMs_ = windowStartMs_;
	segments_.clear();
}

bool Timeline::add(CameraIndex cameraIndex, std::int64_t sourceDurationMs, int speedPercent)
{
	if (segments_.size() >= MaxSegments || sourceDurationMs <= 0 || cursorMs_ >= eventDurationMs_)
		return false;
	if (speedPercent != 50 && speedPercent != 100)
		return false;

	const auto end = std::min(eventDurationMs_, cursorMs_ + sourceDurationMs);
	Segment segment{cameraIndex, cursorMs_, end, speedPercent};
	if (!segment.valid())
		return false;

	segments_.push_back(segment);
	cursorMs_ = end;
	return true;
}

bool Timeline::removeLast()
{
	if (segments_.empty())
		return false;
	segments_.pop_back();
	cursorMs_ = segments_.empty() ? windowStartMs_ : segments_.back().outMs;
	return true;
}

void Timeline::clear()
{
	segments_.clear();
	cursorMs_ = windowStartMs_;
}

const std::vector<Segment> &Timeline::segments() const noexcept
{
	return segments_;
}

bool Timeline::empty() const noexcept
{
	return segments_.empty();
}

std::int64_t Timeline::eventDurationMs() const noexcept
{
	return eventDurationMs_;
}

std::int64_t Timeline::windowStartMs() const noexcept
{
	return windowStartMs_;
}

std::int64_t Timeline::cursorMs() const noexcept
{
	return cursorMs_;
}

std::int64_t Timeline::playbackDurationMs() const noexcept
{
	std::int64_t duration = 0;
	for (const auto &segment : segments_)
		duration += segment.playbackDurationMs();
	return duration;
}

} // namespace sp::replay
