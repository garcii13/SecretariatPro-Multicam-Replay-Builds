#include "replay-layout.hpp"

#include <cassert>
#include <iostream>

using sp::replay::coverRect;

int main()
{
	const auto hd = coverRect(1280, 720, 1920, 1080);
	assert(hd.x == 0 && hd.y == 0);
	assert(hd.width == 1920 && hd.height == 1080);

	const auto fourByThree = coverRect(1024, 768, 1920, 1080);
	assert(fourByThree.x == 0 && fourByThree.y == -180);
	assert(fourByThree.width == 1920 && fourByThree.height == 1440);

	const auto portrait = coverRect(720, 1280, 1920, 1080);
	assert(portrait.x == 0 && portrait.y == -1167);
	assert(portrait.width == 1920 && portrait.height == 3414);

	const auto unavailable = coverRect(0, 0, 1920, 1080);
	assert(unavailable.x == 0 && unavailable.y == 0);
	assert(unavailable.width == 1920 && unavailable.height == 1080);

	std::cout << "layout-tests: OK\n";
	return 0;
}
