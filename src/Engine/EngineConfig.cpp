#include "EngineConfig.hpp"
#include "Program.h"

using namespace BB;

// increment 
constexpr uint32_t CONFIG_IDENTITY = 0xBBCBBCBB;
constexpr uint32_t CONFIG_NUMBER = 3;
constexpr uint2 CONFIG_DEFAULT_WINDOW_SIZE = uint2(1280, 720);
constexpr uint2 CONFIG_DEFAULT_WINDOW_OFFSET = uint2(1280 / 4, 720 / 4);
constexpr const char* CONFIG_FILE_NAME = "config.bbc";

constexpr EngineConfig DEFAULT_CONFIG_FILE
{
	CONFIG_IDENTITY,
	CONFIG_NUMBER,
	CONFIG_DEFAULT_WINDOW_SIZE.x,
	CONFIG_DEFAULT_WINDOW_SIZE.y,
	CONFIG_DEFAULT_WINDOW_OFFSET.x,
	CONFIG_DEFAULT_WINDOW_OFFSET.y,
	false
};

ENGINE_CONFIG_LOAD_STATUS BB::GetEngineConfigData(MemoryArena& a_temp_arena, EngineConfig& a_out_config)
{
	if (!OSFileExist(CONFIG_FILE_NAME))
	{
		a_out_config = DEFAULT_CONFIG_FILE;
		return ENGINE_CONFIG_LOAD_STATUS::NO_FOUND;
	}

	const Buffer file_buffer = OSReadFile(a_temp_arena, CONFIG_FILE_NAME);
	if (file_buffer.data == nullptr || file_buffer.size > sizeof(EngineConfig))
	{
		a_out_config = DEFAULT_CONFIG_FILE;
		return ENGINE_CONFIG_LOAD_STATUS::FILE_READ_FAILED;
	}

	if (reinterpret_cast<const uint32_t*>(file_buffer.data)[0] != CONFIG_IDENTITY)
	{
		a_out_config = DEFAULT_CONFIG_FILE;
		return ENGINE_CONFIG_LOAD_STATUS::WRONG_FILE_IDENTIFIER;
	}

	if (reinterpret_cast<const uint32_t*>(file_buffer.data)[1] != CONFIG_NUMBER)
	{
		a_out_config = DEFAULT_CONFIG_FILE;
		return ENGINE_CONFIG_LOAD_STATUS::OUT_OF_DATE;
	}

	memcpy(&a_out_config, file_buffer.data, file_buffer.size);
	return ENGINE_CONFIG_LOAD_STATUS::SUCCESS;
}

bool BB::WriteEngineConfigData(const EngineConfig& a_data)
{
	const OSFileHandle config_file = OSCreateFile(CONFIG_FILE_NAME);
	return OSWriteFile(config_file, &a_data, sizeof(EngineConfig));
}
