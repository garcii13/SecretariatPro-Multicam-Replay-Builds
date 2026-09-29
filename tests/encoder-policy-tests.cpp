#include "encoder-policy.hpp"
#include <cassert>

int main()
{
	using sp::replay::automaticHardwareEncoder;
	const std::string apple = "com.apple.videotoolbox.videoencoder.ave.avc";
	assert(automaticHardwareEncoder({"obs_x264", apple}) == apple);
	assert(automaticHardwareEncoder({"obs_x264", "obs_nvenc_h264_tex"}) == "obs_nvenc_h264_tex");
	assert(automaticHardwareEncoder({"obs_x264"}).empty());
	assert(automaticHardwareEncoder({}).empty());
}
