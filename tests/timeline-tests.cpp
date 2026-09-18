#include "replay-timeline.hpp"

#include <iostream>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "check failed: " #condition << "\n"; return EXIT_FAILURE; } } while (false)

using sp::replay::Timeline;

int main()
{
	Timeline timeline;
	timeline.reset(30'000, 10'000);
	CHECK(timeline.windowStartMs() == 20'000);
	CHECK(timeline.cursorMs() == 20'000);

	CHECK(timeline.add(0, 4'000, 100));
	CHECK(timeline.add(2, 3'000, 50));
	CHECK(timeline.segments().size() == 2);
	CHECK(timeline.segments()[1].cameraIndex == 2);
	CHECK(timeline.segments()[0].inMs == 20'000);
	CHECK(timeline.segments()[1].inMs == 24'000);
	CHECK(timeline.playbackDurationMs() == 10'000);

	CHECK(timeline.removeLast());
	CHECK(timeline.cursorMs() == 24'000);
	CHECK(!timeline.add(1, 1'000, 25));

	timeline.clear();
	for (std::size_t i = 0; i < Timeline::MaxSegments; ++i)
		CHECK(timeline.add(i, 1'000, 100));
	CHECK(!timeline.add(8, 1'000, 100));

	Timeline clamped;
	clamped.reset(8'000, 20'000);
	CHECK(clamped.windowStartMs() == 0);
	CHECK(clamped.add(0, 20'000, 100));
	CHECK(clamped.segments().front().outMs == 8'000);
	CHECK(!clamped.add(1, 1'000, 100));

	std::cout << "timeline-tests: OK\n";
	return 0;
}
