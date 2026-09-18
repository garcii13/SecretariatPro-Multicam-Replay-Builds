#include "replay-layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sp::replay {

VideoRect coverRect(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t canvasWidth,
		    std::uint32_t canvasHeight) noexcept
{
	if (sourceWidth == 0 || sourceHeight == 0 || canvasWidth == 0 || canvasHeight == 0)
		return {0, 0, canvasWidth, canvasHeight};

	const double scale = std::max(static_cast<double>(canvasWidth) / sourceWidth,
				      static_cast<double>(canvasHeight) / sourceHeight);
	const auto drawWidth = static_cast<std::uint32_t>(std::ceil(sourceWidth * scale));
	const auto drawHeight = static_cast<std::uint32_t>(std::ceil(sourceHeight * scale));
	const auto x = static_cast<int>((static_cast<std::int64_t>(canvasWidth) - drawWidth) / 2);
	const auto y = static_cast<int>((static_cast<std::int64_t>(canvasHeight) - drawHeight) / 2);
	return {x, y, drawWidth, drawHeight};
}

} // namespace sp::replay
