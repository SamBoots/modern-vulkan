#include "ShaderCompiler.h"

#include <Windows.h>
#include <combaseapi.h>
#include "dxc/dxcapi.h"

#include "Logger.h"
#include "BBMemory.h"

#include "MemoryArena.hpp"

// https://simoncoenen.com/blog/programming/graphics/DxcCompiling Guide used, I'll also use this as reference to remind myself.

#if defined(__GNUC__) || defined(__MINGW32__) || defined(__clang__) || defined(__clang_major__)
// ignore warning related to using IID_PPV_ARGS
BB_PRAGMA(clang diagnostic push)
BB_PRAGMA(clang diagnostic ignored "-Wlanguage-extension-token")
#endif 

using namespace BB;

struct ShaderCompiler_inst
{
	IDxcUtils* utils;
	IDxcCompiler3* compiler;
	IDxcIncludeHandler* include_header;
};

ShaderCompiler BB::CreateShaderCompiler(struct BB::MemoryArena& a_arena)
{
	ShaderCompiler_inst* inst = ArenaAllocType(a_arena, ShaderCompiler_inst);

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&inst->utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&inst->compiler));
	inst->utils->CreateDefaultIncludeHandler(&inst->include_header);

	return ShaderCompiler(reinterpret_cast<uintptr_t>(inst));
}
void BB::DestroyShaderCompiler(const ShaderCompiler a_shader_compiler)
{
	const ShaderCompiler_inst* inst = reinterpret_cast<ShaderCompiler_inst*>(a_shader_compiler.handle);
	inst->include_header->Release();
	inst->compiler->Release();
	inst->utils->Release();
}

const ShaderCode BB::CompileShader(const ShaderCompiler a_shader_compiler, const Buffer& a_buffer, const char* a_entry, const SHADER_STAGE a_shader_stage)
{
	const ShaderCompiler_inst* inst = reinterpret_cast<ShaderCompiler_inst*>(a_shader_compiler.handle);
	LPCWSTR shader_type;
	switch (a_shader_stage)
	{
	case SHADER_STAGE::VERTEX:
		shader_type = L"vs_6_4";
		break;
	case SHADER_STAGE::FRAGMENT_PIXEL:
		shader_type = L"ps_6_4";
		break;
	default:
		shader_type = L"ERROR_NO_STAGE";
		BB_ASSERT(false, "not yet supported shader stage");
		break;
	}
	constexpr size_t MAX_FILE_PATH = MAX_PATH;

	size_t entry_str_size = strlen(a_entry) + 1;
	wchar_t entry_w[MAX_FILE_PATH];

	size_t conv_chars = 0; 
	errno_t error = mbstowcs_s(&conv_chars, entry_w, entry_str_size, a_entry, MAX_FILE_PATH);
	BB_ASSERT(error == 0, "8 bit char to 16 bit wide char for a_entry failed");

	//Lots of arguments, since we will add some extra.
	LPCWSTR shader_compile_args[] =
	{
		L"-E", entry_w,		// Entry point
		L"-I", L"../../resources/shaders/HLSL",
		L"-T", shader_type,	// Shader Type
		L"-Zs",				// Enable debug
		L"-Qstrip_debug",	// Strip out the debug and reflect info to keep the actual shader object small.
		L"-HV", L"2021",

		//VULKAN SPECIFIC
		L"-spirv",
		L"-D",
		L"_VULKAN"
	};

	const uint32_t shader_compile_arg_count = _countof(shader_compile_args); //Current elements inside the standard shader compiler args


	DxcBuffer source;
	source.Ptr = a_buffer.data;
	source.Size = a_buffer.size;
	source.Encoding = DXC_CP_ACP;

	IDxcResult* result;
	HRESULT hresult;

	hresult = inst->compiler->Compile(
		&source,
		shader_compile_args,
		shader_compile_arg_count,
		inst->include_header,
		IID_PPV_ARGS(&result)
	);

	IDxcBlobUtf8* errors = nullptr;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

	if (errors != nullptr && errors->GetStringLength() != 0)
	{
		Logger::Log_Warning_High(
			__FILE__, 
			__LINE__, 
			"ss", "Shader Compilation failed with errors:\n%hs\n", 
			errors->GetStringPointer());
		errors->Release();
	}

	result->GetStatus(&hresult);
	if (FAILED(hresult))
	{
		BB_WARNING(false, "Failed to load shader", WarningType::HIGH);
		return ShaderCode(BB_INVALID_HANDLE_64);
	}

	IDxcBlob* shader_code;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_code), nullptr);
	if (shader_code->GetBufferPointer() == nullptr)
	{
		BB_WARNING(false, "Something went wrong with DXC shader compiling", WarningType::HIGH);
		return ShaderCode(BB_INVALID_HANDLE_64);
	}

	result->Release();

	return ShaderCode(reinterpret_cast<uintptr_t>(shader_code));
}

void BB::ReleaseShaderCode(const ShaderCode a_handle)
{
	reinterpret_cast<IDxcBlob*>(a_handle.handle)->Release();
}

Buffer BB::GetShaderCodeBuffer(const ShaderCode a_handle)
{
	return Buffer
	{
		reinterpret_cast<char*>(reinterpret_cast<IDxcBlob*>(a_handle.handle)->GetBufferPointer()),
		reinterpret_cast<IDxcBlob*>(a_handle.handle)->GetBufferSize()
	};
}

#if defined(__GNUC__) || defined(__MINGW32__) || defined(__clang__) || defined(__clang_major__)
BB_PRAGMA(clang diagnostic pop)
#endif 
