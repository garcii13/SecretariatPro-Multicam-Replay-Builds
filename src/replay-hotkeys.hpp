#pragma once

#include <memory>

namespace sp::replay {

class ReplayEngine;

class ReplayHotkeys final {
public:
	explicit ReplayHotkeys(ReplayEngine *engine);
	~ReplayHotkeys();

	ReplayHotkeys(const ReplayHotkeys &) = delete;
	ReplayHotkeys &operator=(const ReplayHotkeys &) = delete;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace sp::replay
