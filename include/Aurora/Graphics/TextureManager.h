#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>

#define png_infopp_NULL (png_infopp)NULL
#define int_p_NULL (int*)NULL


#include <stdlib.h>
#include <malloc.h>
#include <png.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psputils.h>
#include <pspdisplay.h>

#include <math.h>
#include <psptypes.h>

namespace Aurora
{
	namespace Graphics
	{
		class Image
		{
		public:

			int Width;
			int Height;
			int power2Width;
			int power2Height;
			int vRam;
			int ColorMode;
			int Swizzle;

			unsigned int ID;

			std::string FileName;

			//more like for psp
			unsigned short *ImageData;

            virtual ~Image()
			{

			}
		};

		class TextureManager
		{
		private:

			static TextureManager m_TextureManager;
			const void* boundTexels;


			int PowerOfTwo(int value)
			{
				int poweroftwo = 1;
				while (poweroftwo < value ){
					poweroftwo <<= 1;
				}
				return poweroftwo;
			}

			unsigned short Color8888To5551(unsigned int Color32)
			{
				unsigned char Red = (Color32);
				unsigned short Red16 = Red>>3;
				unsigned char Green = (Color32>>8);
				unsigned short Green16 = (Green>>3)<<5;
				unsigned char Blue = (Color32>>16);
				unsigned short Blue16 = (Blue>>3)<<10;
				unsigned char Alpha = (Color32>>24);
				// 5551 has ONE alpha bit, so this is a threshold, not a scale. It was written as
				// (Alpha>>3)<<15, which in 16-bit arithmetic keeps only bit 0 of (Alpha>>3) -- i.e.
				// bit 3 of the alpha byte -- so a soft pixel came out opaque or invisible according
				// to one interior bit of its alpha rather than according to how transparent it was.
				// alpha=105 (the changelog panel cell of title/menu_elements.png) read as opaque and
				// alpha=240 (the splash's edge) read as invisible, while alpha=128 read as invisible
				// and alpha=8 as opaque. Measured on title/logo.png: of its 701 antialiased pixels,
				// 380 became opaque and 321 vanished -- a patchwork edge. (Alpha>>7)<<15 is the
				// threshold it was always meant to be.
				unsigned short Alpha16 = (Alpha>>7)<<15;
				unsigned short Color16 = Red16 | Green16 | Blue16 | Alpha16;
				return Color16;
			}

			unsigned short Color8888To4444(unsigned int Color32)
			{
				unsigned char Red = (Color32);
				unsigned short Red16 = Red>>4;
				unsigned char Green = (Color32>>8);
				unsigned short Green16 = (Green>>4)<<4;
				unsigned char Blue = (Color32>>16);
				unsigned short Blue16 = (Blue>>4)<<8;
				unsigned char Alpha = (Color32>>24);
				unsigned short Alpha16 = (Alpha>>4)<<12;
				unsigned short Color16 = Red16 | Green16 | Blue16 | Alpha16;
				return Color16;
			}

			unsigned short Color8888To5650(unsigned int Color32)
			{
				unsigned char Red = (Color32);
				unsigned short Red16 = Red>>3;
				unsigned char Green = (Color32>>8);
				unsigned short Green16 = (Green>>2)<<5;
				unsigned char Blue = (Color32>>16);
				unsigned short Blue16 = (Blue>>3)<<11;
				//unsigned char Alpha = (Color32>>24);
				unsigned short Color16 = Red16 | Green16 | Blue16;
				return Color16;
			}


			void swizzle_fast(u8* out, const u8* in, unsigned int width, unsigned int height)
			{
				unsigned int blockx, blocky;
				unsigned int j;

				unsigned int width_blocks = (width / 16);
				unsigned int height_blocks = (height / 8);

				unsigned int src_pitch = (width-16)/4;
				unsigned int src_row = width * 8;

				const u8* ysrc = in;
				u32* dst = (u32*)out;

				for (blocky = 0; blocky < height_blocks; ++blocky)
				{
					const u8* xsrc = ysrc;
					for (blockx = 0; blockx < width_blocks; ++blockx)
					{
						const u32* src = (u32*)xsrc;
						for (j = 0; j < 8; ++j)
						{
							*(dst++) = *(src++);
							*(dst++) = *(src++);
							*(dst++) = *(src++);
							*(dst++) = *(src++);
							src += src_pitch;
						}
						xsrc += 16;
					}
					ysrc += src_row;
				}
			}
		public:

			// cropClassicSkin: a 64x64 file is folded to 64x32, its overlay layers composited onto the
			// classic top half that the player model samples. Any other size is returned unchanged.
			Image* LoadPng(const char* filename,int ColorMode,int Swizzle,int Vram,int cropClassicSkin = 0);

			// Swap the pixels of a texture that is already in the list. The slot number stays, so
			// every draw that already asked for it (hand, inventory, third person) picks up the new
			// pixels. The previous pixels are freed.
			void ReplaceImage(int number, Image* fresh);

			std::vector<std::string> Names;
			std::vector<Image*> Images;

            std::vector<std::string> ConstNames;
			std::vector<Image*> ConstImages;

			int currentTexture;

			static TextureManager* Instance();

			void Init();

			bool FileExist(std::string fileName);

			bool TextureExist(std::string name);
			int GetTextureNumber(std::string name);

			void SetTexture(int number,int minFilter = GU_LINEAR,int maxFilter = GU_LINEAR);
			void SetTextureModeulate(int number);

			// Forget which texture is bound, so the next SetTexture re-issues the whole texture setup
			// (mode, function, filter, wrap, image) rather than skipping it because the number it is
			// asked for has not changed. Needed after anything binds a texture behind this class's
			// back -- intraFont does exactly that for its glyph atlas. See RenderManager::DebugPrint.
			void InvalidateCurrentTexture() { currentTexture = -1; }

			void SetMipMapsTextures(int texture,int mipmap1,int mipmap2,int mipmap3);
			void SetMipMaps2Textures(int texture,int mipmap1);
			// Optional LOD bias: 0 leaves mip selection to the GE. See the definition.
			void SetMipMaps2Textures(int texture,int mipmap1,float bias);

            void LoadConstTexture(std::string fileName);
            int GetConstTextureNumber(std::string name);
            void SetConstTexture(int number,int minFilter = GU_LINEAR,int maxFilter = GU_LINEAR);
            void RemoveConstTextures();

			void LoadTexture(std::string fileName);
			// As above, but picks the format the pixels are STORED in. GU_PSM_8888 is four bytes
			// per texel, GU_PSM_5551 (and 4444/5650) is two. The GE is told whatever the image
			// records (SetTexture passes image->ColorMode to sceGuTexMode), so this loader and the
			// pixel accessors below are the only things that have to agree. See
			// TextureHelper::FastLoadTexture for which textures ask for which, and why.
			void LoadTexture(std::string fileName, int colorMode);
			void LoadTexureVFS(std::string fileName);

			// Free a texture's pixels but KEEP its slot, so its number stays valid and every Sprite
			// that already cached that number keeps working -- it just draws nothing until
			// ReloadTextureData runs. RemoveTexture() cannot do this job: it erases from the parallel
			// Names/Images vectors, which renumbers every texture after it and invalidates every
			// number held anywhere else. Used by TextureHelper::ReleaseMenuTextures to give the
			// title-screen art back while a world is running.
			void ReleaseTextureData(int number);
			bool TextureDataLoaded(int number);
			void ReloadTextureData(int number, int colorMode);

			void RemoveTexture(int number);
			void RemoveTextures();

			// Total bytes the loaded, non-constant textures occupy in the heap, and how many of
			// them are stored at two bytes per texel rather than four. This is here because that
			// total is about 40% of the partition the game runs in and there was no way to read it
			// off a console: without it, "textures got smaller" was an argument rather than a
			// number. Logged at each point the set of loaded textures settles.
			void LogTextureUsage(const char* tag);

			Image* LoadPng(std::string fileName);
			Image* LoadPng(std::string fileName, int colorMode);
			Image* LoadPngFromMemory(std::string fileName);

			Image* CreateImage(int width,int height,int ColorMode,int Vram);

			int getAlpha(int x,int y,int number);
			int getGreen(int x,int y,int number);
			int getRed(int x,int y,int number);
			int getBlue(int x,int y,int number);

			void setColour(int x,int y, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha, int number);

			// Whole 32-bit texel (lo16 = r | g<<8, hi16 = b | a<<8) for fast tile copies.
			unsigned int getPixelRaw(int x,int y,int number);
			void setPixelRaw(int x,int y,unsigned int raw,int number);
			// Call after CPU texel writes: data-cache writeback of the touched swizzle
			// blocks plus a deferred sceGuTexFlush on the next bind.
			void FlushTexelRegion(int x0,int y0,int x1,int y1,int number);
			void FlushDirtyTexels();
			bool texelsDirty;

			float getWidth(int number);
			int getHeight(int number);
		};

	}
}

#endif
