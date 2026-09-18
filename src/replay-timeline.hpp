#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sp::replay {

using CameraIndex = std::size_t;

struct Segment {
	CameraIndex cameraIndex{0};
	std::int64_t inMs{0};
	std::int64_t outMs{0};
	int speedPercent{100};

	[[nodiscard]] std::int64_t sourceDurationMs() const noexcept;
	[[nodiscard]] std::int64_t playbackDurationMs() const noexcept;
	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] std::string label() const;
};

class Timeline {
public:
	static constexpr std::size_t MaxSegments = 6;

	void reset(std::int64_t eventDurationMs, std::int64_t replayWindowMs);
	[[nodiscard]] bool add(CameraIndex cameraIndex, std::int64_t sourceDurationMs, int speedPercent);
	[[nodiscard]] bool removeLast();
	void clear();

	[[nodiscard]] const std::vector<Segment> &segments() const noexcept;
	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] std::int64_t eventDurationMs() const noexcept;
	[[nodiscard]] std::int64_t windowStartMs() const noexcept;
	[[nodiscard]] std::int64_t cursorMs() const noexcept;
	[[nodiscard]] std::int64_t playbackDurationMs() const noexcept;

private:
	std::vector<Segment> segments_;
	std::int64_t eventDurationMs_{0};
	std::int64_t windowStartMs_{0};
	std::int64_t cursorMs_{0};
};

} // namespace sp::replay
