#pragma once

#include <string>
#include <vector>

namespace sp::replay {

struct Settings {
	std::vector<std::string> sourceUuids{2};
	std::string encoderId{"obs_x264"};
	std::string replaySceneName{"SP Replay"};
	int bufferSeconds{30};
	int replayWindowSeconds{10};
	int segmentSeconds{3};
	int bitrateKbps{12000};
};

class SettingsStore {
public:
	[[nodiscard]] static Settings load();
	static bool save(const Settings &settings);
	[[nodiscard]] static std::string replayDirectory();

private:
	[[nodiscard]] static std::string settingsPath();
};

} // namespace sp::replay
