#pragma once
#include "Enginefwd.hpp"
#include "Allocators/MemoryArena.hpp"

namespace BB
{
	struct EngineConfig
	{
		uint32_t config_identity;
		uint32_t config_number;
		uint2 window_size;
		uint2 window_offset;
		bool window_full_screen;
	};

	enum class ENGINE_CONFIG_LOAD_STATUS
	{
		SUCCESS = 0,
		NO_FOUND,
		FILE_READ_FAILED,
		WRONG_FILE_IDENTIFIER,
		OUT_OF_DATE
	};

	// on ENGINE_CONFIG_LOAD_STATUS::SUCCESS a_out_config becomes the loaded config file, otherwise it's DEFAULT_CONFIG_FILE.
	ENGINE_CONFIG_LOAD_STATUS GetEngineConfigData(MemoryArena& a_temp_arena, EngineConfig& a_out_config);
	bool WriteEngineConfigData(const EngineConfig& a_data);
}
