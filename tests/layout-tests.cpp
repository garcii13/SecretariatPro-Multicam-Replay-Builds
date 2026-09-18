#include "replay-layout.hpp"

#include <iostream>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "check failed: " #condition << "\n"; return EXIT_FAILURE; } } while (false)

using sp::replay::coverRect;

int main()
{
	const auto hd = coverRect(1280, 720, 1920, 1080);
	CHECK(hd.x == 0 && hd.y == 0);
	CHECK(hd.width == 1920 && hd.height == 1080);

	const auto fourByThree = coverRect(1024, 768, 1920, 1080);
	CHECK(fourByThree.x == 0 && fourByThree.y == -180);
	CHECK(fourByThree.width == 1920 && fourByThree.height == 1440);

	const auto portrait = coverRect(720, 1280, 1920, 1080);
	CHECK(portrait.x == 0 && portrait.y == -1167);
	CHECK(portrait.width == 1920 && portrait.height == 3414);

	const auto unavailable = coverRect(0, 0, 1920, 1080);
	CHECK(unavailable.x == 0 && unavailable.y == 0);
	CHECK(unavailable.width == 1920 && unavailable.height == 1080);

	std::cout << "layout-tests: OK\n";
	return 0;
}
