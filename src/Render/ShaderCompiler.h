#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"

namespace BB
{
	void InitShaderCompiler();

	const ShaderCode CompileShader(Allocator a_temp_allocator, const char* a_full_path, const char* a_entry, const SHADER_STAGE a_shader_stage);
	void ReleaseShaderCode(const ShaderCode a_handle);

	Buffer GetShaderCodeBuffer(const ShaderCode a_handle);
}