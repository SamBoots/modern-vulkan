#include "ShaderCompiler.h"

#include <Windows.h>
#include <combaseapi.h>
#include "DXC/dxcapi.h"

#include "Logger.h"
#include "BBMemory.h"

//https://simoncoenen.com/blog/programming/graphics/DxcCompiling Guide used, I'll also use this as reference to remind myself.

using namespace BB;
using namespace BB::Render;

struct ShaderCompiler
{
	IDxcUtils* utils;
	IDxcCompiler3* compiler;
	IDxcLibrary* library;
};

static ShaderCompiler s_shader_compiler;

void BB::Render::InitShaderCompiler()
{
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_shader_compiler.utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_shader_compiler.compiler));
	DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&s_shader_compiler.library));
}

const ShaderCode BB::Render::CompileShader(Allocator a_temp_allocator, const char* a_full_path, const char* a_entry, const SHADER_STAGE a_shader_stage)
{
	LPCWSTR shader_type;
	switch (a_shader_stage)
	{
	case SHADER_STAGE::VERTEX:
		shader_type = L"vs_6_4";
		break;
	case SHADER_STAGE::FRAGMENT_PIXEL:
		shader_type = L"ps_6_4";
		break;
	}

	size_t full_path_str_size = strlen(a_full_path);
	size_t entry_str_size = strlen(a_entry);
	wchar_t* full_path_w = BBnewArr(a_temp_allocator, full_path_str_size + 1, wchar_t);
	wchar_t* entry_w = BBnewArr(a_temp_allocator, entry_str_size + 1, wchar_t);

	BB_ASSERT(mbstowcs(full_path_w, a_full_path, full_path_str_size) == full_path_str_size, "8 bit char to 16 bit wide char for a_full_path failed");
	BB_ASSERT(mbstowcs(entry_w, a_entry, entry_str_size) == entry_str_size, "8 bit char to 16 bit wide char for a_entry failed");

	full_path_w[full_path_str_size] = L'\0';
	entry_w[entry_str_size] = L'\0';

	//Lots of arguments, since we will add some extra.
	LPCWSTR shader_compile_args[] =
	{
		full_path_w,
		L"-E", entry_w,		// Entry point
		L"-T", shader_type,	// Shader Type
		L"-Zs",				// Enable debug
		L"-Qstrip_debug",	// Strip out the debug and reflect info to keep the actual shader object small.

		//VULKAN SPECIFIC
		L"-spirv",
		L"-D",
		L"_VULKAN"
	};

	const uint32_t shader_compile_arg_count = _countof(shader_compile_args); //Current elements inside the standard shader compiler args


	IDxcBlobEncoding* source_blob;
	s_shader_compiler.utils->LoadFile(full_path_w, nullptr, &source_blob);
	DxcBuffer source;
	source.Ptr = source_blob->GetBufferPointer();
	source.Size = source_blob->GetBufferSize();
	source.Encoding = DXC_CP_ACP;

	IDxcResult* result;
	HRESULT hresult;

	hresult = s_shader_compiler.compiler->Compile(
		&source,
		shader_compile_args,
		shader_compile_arg_count,
		nullptr,
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
		BB_ASSERT(false, "Failed to load shader");
	}

	IDxcBlob* shader_code;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_code), nullptr);
	if (shader_code->GetBufferPointer() == nullptr)
	{
		BB_ASSERT(false, "Something went wrong with DXC shader compiling");
	}

	source_blob->Release();
	result->Release();

	return ShaderCode((uintptr_t)shader_code);
}

void Render::ReleaseShaderCode(const ShaderCode a_handle)
{
	reinterpret_cast<IDxcBlob*>(a_handle.handle)->Release();
}

Buffer Render::GetShaderCodeBuffer(const ShaderCode a_handle)
{
	return Buffer
	{
		reinterpret_cast<char*>(reinterpret_cast<IDxcBlob*>(a_handle.handle)->GetBufferPointer()),
		reinterpret_cast<IDxcBlob*>(a_handle.handle)->GetBufferSize()
	};
}