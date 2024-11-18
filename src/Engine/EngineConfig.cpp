#include "EngineConfig.hpp"
#include "Program.h"

using namespace BB;

ENGINE_CONFIG_LOAD_STATUS BB::GetEngineConfigData(MemoryArena& a_temp_arena, EngineConfig& a_out_config)
{
	a_out_config = DEFAULT_CONFIG_FILE;
	if (!OSFileExist(CONFIG_FILE_NAME))
	{
		return ENGINE_CONFIG_LOAD_STATUS::NO_FOUND;
	}

	const Buffer file_buffer = OSReadFile(a_temp_arena, CONFIG_FILE_NAME);
	if (file_buffer.data == nullptr || file_buffer.size > sizeof(EngineConfig))
	{
		return ENGINE_CONFIG_LOAD_STATUS::FILE_READ_FAILED;
	}

	if (reinterpret_cast<const uint32_t*>(file_buffer.data)[0] != CONFIG_IDENTITY)
	{
		return ENGINE_CONFIG_LOAD_STATUS::WRONG_FILE_IDENTIFIER;
	}

	if (reinterpret_cast<const uint32_t*>(file_buffer.data)[1] != CONFIG_NUMBER)
	{
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
