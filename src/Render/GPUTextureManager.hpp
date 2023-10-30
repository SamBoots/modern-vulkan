#pragma once
#include "Rendererfwd.hpp"

namespace BB
{
	using RTexture = FrameworkHandle32Bit<struct RTextureTag>;


	class GPUTextureManager
	{
	public:
		GPUTextureManager()
		{
			//texture 0 is always the debug texture.
			textures[0].image = debug_texture;
			textures[0].next_free = UINT32_MAX;

			next_free = 1;

			for (uint32_t i = 1; i < MAX_TEXTURES - 1; i++)
			{
				textures[i].image = debug_texture;
				textures[i].next_free = i + 1;
			}

			textures[MAX_TEXTURES - 1].image = debug_texture;
			textures[MAX_TEXTURES - 1].next_free = UINT32_MAX;
		}

		const RTexture UploadTexture()
		{
			const RTexture texture_slot = next_free;
			TextureSlot& slot = textures[texture_slot.handle];

			//do texture stuff

			next_free = slot.next_free;
			return texture_slot;
		}

		void FreeTexture(const RTexture a_texture)
		{
			TextureSlot& slot = textures[a_texture.handle];
			slot.next_free = next_free;
			next_free = a_texture.handle;
		}

	private:
		struct TextureSlot
		{
			RImage image;
			uint32_t next_free;
		};

		uint32_t next_free;
		TextureSlot textures[MAX_TEXTURES];

		//purple color
		RImage debug_texture;
	};
}