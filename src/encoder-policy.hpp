#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace sp::replay {

// Only registered hardware encoders are considered. No silent CPU fallback:
// the operator can explicitly select x264 if this machine needs it.
inline std::string automaticHardwareEncoder(const std::vector<std::string> &available)
{
	const char *preferred[] = {
		"com.apple.videotoolbox.videoencoder.ave.avc", "obs_nvenc_h264_tex",
		"jim_nvenc", "obs_qsv11_v2", "h264_texture_amf", "h264_amf",
	};
	for (const char *id : preferred) {
		if (std::find(available.begin(), available.end(), id) != available.end())
			return id;
	}
	return {};
}

} // namespace sp::replay
