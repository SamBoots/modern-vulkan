#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"

namespace BB
{
	using ShaderCompiler = FrameworkHandle<struct ShaderCompilerTag>;

	struct MemoryArena;

	ShaderCompiler CreateShaderCompiler(MemoryArena& a_arena);
	void DestroyShaderCompiler(const ShaderCompiler a_shader_compiler);

	bool CompileShader(const ShaderCompiler a_shader_compiler, const Buffer& a_buffer, const char* a_entry, const SHADER_STAGE a_shader_stage, ShaderCode& a_out_shader_code);
	void ReleaseShaderCode(const ShaderCode a_handle);

	Buffer GetShaderCodeBuffer(const ShaderCode a_handle);
}
