#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"

namespace BB
{
	using ShaderCompiler = FrameworkHandle<struct ShaderCompilerTag>;

	ShaderCompiler CreateShaderCompiler(Allocator a_system_allocator);
	void DestroyShaderCompiler(const ShaderCompiler a_shader_compiler);

	const ShaderCode CompileShader(const ShaderCompiler a_shader_compiler, const char* a_full_path, const char* a_entry, const SHADER_STAGE a_shader_stage);
	void ReleaseShaderCode(const ShaderCode a_handle);

	Buffer GetShaderCodeBuffer(const ShaderCode a_handle);
}
