#pragma once

#include <cstdint>

namespace sp::replay {

struct VideoRect {
	int x{0};
	int y{0};
	std::uint32_t width{0};
	std::uint32_t height{0};
};

[[nodiscard]] VideoRect coverRect(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t canvasWidth,
				  std::uint32_t canvasHeight) noexcept;

} // namespace sp::replay
