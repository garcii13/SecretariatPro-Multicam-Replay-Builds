#pragma once

struct obs_source;
typedef struct obs_source obs_source_t;

namespace sp::replay {

[[nodiscard]] obs_source_t *acquireReplayMediaForRender();

} // namespace sp::replay
