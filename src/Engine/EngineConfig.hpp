#pragma once
#include "Enginefwd.hpp"
#include "Allocators/MemoryArena.hpp"

namespace BB
{
	// increment 
	constexpr uint32_t CONFIG_IDENTITY = 0xCEAD5005;
	constexpr uint32_t CONFIG_NUMBER = 1;
	constexpr uint2 CONFIG_DEFAULT_WINDOW_SIZE = uint2(1280, 720);
	constexpr uint2 CONFIG_DEFAULT_WINDOW_OFFSET = uint2(1280 / 4, 720 / 4);
	constexpr const char* CONFIG_FILE_NAME = "config.bbc";

	struct EngineConfig
	{
		uint32_t config_identity = CONFIG_IDENTITY;
		uint32_t config_number = 1;
		uint2 window_size;
		uint2 window_offset;
		bool window_full_screen;
	};

	constexpr EngineConfig DEFAULT_CONFIG_FILE
	{
		CONFIG_IDENTITY,
		CONFIG_NUMBER,
		CONFIG_DEFAULT_WINDOW_SIZE,
		CONFIG_DEFAULT_WINDOW_OFFSET,
		false
	};

	enum class ENGINE_CONFIG_LOAD_STATUS
	{
		SUCCESS = 0,
		NO_FOUND,
		FILE_READ_FAILED,
		WRONG_FILE_IDENTIFIER,
		OUT_OF_DATE
	};

	// on false a_out_config becomes DEFAULT_CONFIG_FILE
	ENGINE_CONFIG_LOAD_STATUS GetEngineConfigData(MemoryArena& a_temp_arena, EngineConfig& a_out_config);
	bool WriteEngineConfigData(const EngineConfig& a_data);
}
