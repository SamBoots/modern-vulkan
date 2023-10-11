#include "BBMemory.h"
#include "RenderFrontendCommon.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char MODELS_DIRECTORY[] = "Resources/models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "resources/textures/";

	namespace Asset
	{
		char* FindOrCreateString(const char* a_string);
	};
}