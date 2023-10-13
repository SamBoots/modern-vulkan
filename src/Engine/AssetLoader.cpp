#include "AssetLoader.hpp"
#include "Storage/BBString.h"

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning (pop)

#include "Storage/Hashmap.h"

using namespace BB;

//crappy hash, don't care for now.
const uint64_t StringHash(const char* a_string)
{
	uint64_t hash = 5381;
	int c;

	while (c = *a_string++)
		hash = ((hash << 5) + hash) + c;

	return hash;
}

struct AssetManager
{
	FreelistAllocator_t allocator{ mbSize * 64, "asset manager allocator" };

	OL_HashMap<uint64_t, char*> stringMap{ allocator, 128 };
};
static AssetManager s_AssetManager{};

using namespace BB;

char* Asset::FindOrCreateString(const char* a_string)
{
	const uint64_t t_StringHash = StringHash(a_string);
	char** t_StringPtr = s_AssetManager.stringMap.find(t_StringHash);
	if (t_StringPtr != nullptr)
		return *t_StringPtr;

	const uint32_t t_StringSize = static_cast<uint32_t>(strlen(a_string) + 1);
	char* t_String = BBnewArr(s_AssetManager.allocator, t_StringSize, char);
	memcpy(t_String, a_string, t_StringSize);
	t_String[t_StringSize - 1] = '\0';
	s_AssetManager.stringMap.emplace(t_StringHash, t_String);
	return t_String;
}