#include "replay-timeline.hpp"

#include <cassert>
#include <iostream>

using sp::replay::Timeline;

int main()
{
	Timeline timeline;
	timeline.reset(30'000, 10'000);
	assert(timeline.windowStartMs() == 20'000);
	assert(timeline.cursorMs() == 20'000);

	assert(timeline.add(0, 4'000, 100));
	assert(timeline.add(2, 3'000, 50));
	assert(timeline.segments().size() == 2);
	assert(timeline.segments()[1].cameraIndex == 2);
	assert(timeline.segments()[0].inMs == 20'000);
	assert(timeline.segments()[1].inMs == 24'000);
	assert(timeline.playbackDurationMs() == 10'000);

	assert(timeline.removeLast());
	assert(timeline.cursorMs() == 24'000);
	assert(!timeline.add(1, 1'000, 25));

	timeline.clear();
	for (std::size_t i = 0; i < Timeline::MaxSegments; ++i)
		assert(timeline.add(i, 1'000, 100));
	assert(!timeline.add(8, 1'000, 100));

	Timeline clamped;
	clamped.reset(8'000, 20'000);
	assert(clamped.windowStartMs() == 0);
	assert(clamped.add(0, 20'000, 100));
	assert(clamped.segments().front().outMs == 8'000);
	assert(!clamped.add(1, 1'000, 100));

	std::cout << "timeline-tests: OK\n";
	return 0;
}
