#include <Aurora/Graphics/TextureManager.h>
#include <Aurora/Graphics/vram.h>
#include <Aurora/Utils/Logger.h>

// longjmp, for the libpng error handler below. png.h brings this in under PNG_SETJMP_SUPPORTED, but
// this file now depends on it directly rather than incidentally.
#include <setjmp.h>

/*
 * TEMPORARY: line-level tracing inside the PNG loader, and the permanent fix it was built to find.
 *
 * The Updater died during `new Sprite("...title/dirt.png", ...)` with Log.txt stopping between "screen:
 * loading the dirt art" and the loader's own next message. Because Logger flushes every line, that
 * placed the fault strictly inside LoadPng -- and, as the trace below went on to show, strictly inside
 * png_read_info, with the console reporting an invalid memory access. See PngErrorHandler for what was
 * actually wrong; the short version is that libpng's error path was jumping through an uninitialised
 * buffer because this file never satisfied the API's setjmp requirement.
 *
 * The step-by-step tracing is TEMPORARY and sits behind MC_UPDATER_TRACE, which only the Updater's
 * makefile defines -- the game compiles this same file and must not carry the firehose. The tracing
 * goes through Scripts/InitTrace.h, which writes trace.txt with sceIoOpen/sceIoWrite and never through
 * Logger, so the trace survives a fault in the logger and is an independent record of the same run.
 *
 * Remove the MC_TM/MC_TMI/MC_TMP calls once the crash is confirmed fixed. PngErrorHandler and the
 * setjmp in LoadPng are NOT temporary and must stay -- they are the fix.
 */
#ifdef MC_UPDATER_TRACE
extern "C" {
	void MC_Trace(const char* tag);
	void MC_TraceInt(const char* tag, int value);
	void MC_TraceUInt(const char* tag, unsigned int value);
	void MC_TracePtr(const char* tag, const void* pointer);
	void MC_TraceStr(const char* tag, const char* text);
}
#define MC_TM(tag)             MC_Trace("png: " tag)
#define MC_TMI(tag, value)     MC_TraceInt("png: " tag, (int)(value))
#define MC_TMP(tag, pointer)   MC_TracePtr("png: " tag, (const void*)(pointer))
#else
#define MC_TM(tag)             ((void)0)
#define MC_TMI(tag, value)     ((void)0)
#define MC_TMP(tag, pointer)   ((void)0)
#endif

/*
 * libpng's error and warning callbacks.
 *
 * REGISTERING THESE IS THE FIX for the Updater's crash while loading title/dirt.png, and the reason is
 * a rule of libpng's API that this file had never satisfied.
 *
 * libpng reports a failure by calling the error function, and the default error function ends in
 * `png_longjmp(png_ptr, 1)` -- a longjmp to the jump buffer libpng keeps inside the png_struct. That
 * buffer is only useful if the APPLICATION has primed it, which libpng's documentation requires:
 *
 *     if (setjmp(png_jmpbuf(png_ptr))) { ...handle the error... }
 *     png_read_info(png_ptr, info_ptr);
 *
 * LoadPng never called setjmp. libpng cannot do it itself -- pngread.c, where png_read_info lives,
 * references neither setjmp nor longjmp, which was checked against this project's own libpng.a -- so
 * its jump buffer held whatever the fresh allocation contained, and the first reportable problem
 * became a branch through uninitialised memory. That is exactly the reported fault: the console dies
 * inside png_read_info with an invalid memory access and the log simply stops. In the emulator nothing
 * went wrong to report, the jump was never taken, and the screen drew perfectly, which is why this only
 * ever appeared on hardware.
 *
 * With a handler registered, a failure is a return value and a line in the trace instead of a wild
 * jump, and the message libpng passes in names the actual cause -- so the run after this one says WHY
 * the load failed rather than only that it did.
 */
static void PngErrorHandler(png_structp png_ptr, png_const_charp message)
{
#ifdef MC_UPDATER_TRACE
	MC_TraceStr("png: LIBPNG ERROR", message ? message : "(no message)");
#else
	(void)message;
#endif
	// The jump target established by the setjmp in LoadPng. The handler is expected to leave, and every
	// libpng entry point is written to cope with its frame being abandoned.
	longjmp(png_jmpbuf(png_ptr), 1);
}

static void PngWarningHandler(png_structp png_ptr, png_const_charp message)
{
	(void)png_ptr;
#ifdef MC_UPDATER_TRACE
	MC_TraceStr("png: libpng warning", message ? message : "(no message)");
#else
	(void)message;
#endif
}

namespace Aurora
{
	namespace Graphics
	{
		TextureManager TextureManager::m_TextureManager;

		TextureManager* TextureManager::Instance()
		{
			return &m_TextureManager;
		}

		void TextureManager::Init()
		{
			currentTexture = -1;
			boundTexels = NULL;
			texelsDirty = false;
			//by default it will load default texture default.png
			//LoadTexture("default.png"); //<-- something like this
		}

		bool TextureManager::TextureExist(std::string name)
		{
			for (unsigned int i=0;i < Names.size();i++)
			{
				if (strcmp(name.c_str(),Names[i].c_str())==0)
				{
					return true;
				}
			}
			return false;
		}

		int TextureManager::GetTextureNumber(std::string name)
		{
			for (unsigned int i=0;i < Names.size();i++)
			{
				if (strcmp(name.c_str(),Names[i].c_str())==0)
				{
					return i;
				}
			}
			return -1;
		}

        int TextureManager::GetConstTextureNumber(std::string name)
		{
			for (unsigned int i=0;i < ConstNames.size();i++)
			{
				if (strcmp(name.c_str(),ConstNames[i].c_str())==0)
				{
					return i;
				}
			}
			return -1;
		}

		void TextureManager::RemoveTexture(int number)
		{
			//name
			if(number >= 0 && number < (int)Images.size())
            {
                Names.erase(Names.begin() + number);
                //image (Image has no destructor of its own; free the pixels here)
                if(Images[number]->ImageData != NULL)
                    free(Images[number]->ImageData);
                delete Images[number];
                Images.erase(Images.begin() + number);
                currentTexture = -1;
            }
		}

		void TextureManager::RemoveConstTextures()
		{
		    if(ConstImages.empty() == false)
            {
                for(int i = 0; i < ConstImages.size(); i++)
                {
                    if(ConstImages[i]->ImageData != NULL)
                    {
                        free(ConstImages[i]->ImageData);
                    }

                    delete ConstImages[i];
                }
                ConstNames.clear();
                ConstImages.clear();
            }
		}

		void TextureManager::ReleaseTextureData(int number)
		{
			if(number < 0 || number >= (int)Images.size())
				return;
			Image* image = Images[number];
			if(image == NULL || image->ImageData == NULL)
				return;

			free(image->ImageData);
			image->ImageData = NULL;

			// The bound-texture cache names a texture by NUMBER, and this one no longer points at
			// anything. Clearing it makes the next SetTexture* rebind whatever is actually drawn.
			currentTexture = -1;
		}

		bool TextureManager::TextureDataLoaded(int number)
		{
			if(number < 0 || number >= (int)Images.size())
				return false;
			return Images[number] != NULL && Images[number]->ImageData != NULL;
		}

		void TextureManager::ReloadTextureData(int number, int colorMode)
		{
			if(number < 0 || number >= (int)Images.size())
				return;
			if(Images[number] != NULL && Images[number]->ImageData != NULL)
				return;                        // already resident

			// The name is kept in the parallel vector even after the pixels are freed, which is what
			// makes a release recoverable: reload from the same path, straight back into the same
			// index. A missing or unreadable file leaves the texture released, which SafeImage turns
			// into "draw nothing" -- the same outcome as the asset never being on the card.
			Image* fresh = LoadPng(Names[number], colorMode);
			if(fresh == NULL)
			{
				// A reload that could not allocate leaves the texture released rather than half-built,
				// which is the safe outcome -- but it is not silent. LoadPng only returns NULL here
				// when a file is missing/corrupt or the heap had nowhere near enough room, and the
				// difference matters to whoever reads this log, so the name is printed with the tag
				// that already exists for texture accounting.
				Utils::Logger::Instance()->LogMessage("texture reload failed, left released: %s\n",
					Names[number].c_str());
				return;
			}

			Image* old = Images[number];
			if(old != NULL)
			{
				if(old->ImageData != NULL)
					free(old->ImageData);
				delete old;                    // Image has no destructor, so the pixels are freed above
			}
			Images[number] = fresh;            // same index, so every cached number stays correct
			currentTexture = -1;
		}

		void TextureManager::LogTextureUsage(const char* tag)
		{
			// A texel is two bytes in the 16-bit modes and four in everything else, which is the
			// same rule LoadPng sizes its buffers by (OutBytesPerPixel). Counted here rather than
			// remembered from load time so it describes what is actually resident now.
			size_t bytes = 0;
			int sixteen = 0;
			int thirtyTwo = 0;
			int released = 0;
			for(size_t i = 0; i < Images.size(); i++)
			{
				Image* image = Images[i];
				if(image == NULL)
					continue;
				// A texture released by ReleaseTextureData keeps its slot and its dimensions but its
				// pixels are gone. Counting it anyway would report memory that the heap does not
				// hold, and this number exists precisely to check a claim like "the menu art is not
				// resident while a world runs" -- so it has to be the resident set, or it is useless.
				if(image->ImageData == NULL)
				{
					released++;
					continue;
				}
				int bytesPerTexel = (image->ColorMode == GU_PSM_4444 || image->ColorMode == GU_PSM_5650 || image->ColorMode == GU_PSM_5551) ? 2 : 4;
				bytes += (size_t)image->power2Width * (size_t)image->power2Height * (size_t)bytesPerTexel;
				if(bytesPerTexel == 2)
					sixteen++;
				else
					thirtyTwo++;
			}
			Utils::Logger::Instance()->LogMessage("textures %s: %d image(s), %d at 2 bytes/texel, %d at 4, %d released, %d KB resident\n",
				tag, (int)Images.size(), sixteen, thirtyTwo, released, (int)(bytes / 1024));
		}

		void TextureManager::RemoveTextures()
		{
			for(int i = 0; i < Images.size(); i++)
            {
                if(Images[i]->ImageData != NULL)
                {
                    free(Images[i]->ImageData);
                }

                delete Images[i];
            }
            Names.clear();
            Images.clear();
		}

		// Images[number] with a bounds check. The only way a number can fall outside the list is a
		// caller in game code: TextureHelper::GetTexture() returns -1 when the file is not in one of
		// its FastLoadTexture lists. An unchecked Images[-1] reads the word *before* the vector's
		// buffer -- a stale allocator pointer -- and dereferences that as an Image*, which is how a
		// missing texture asset became "Invalid Memory Access ... SetTextureModeulate +0x40" the
		// first time a chicken was drawn (entity/chicken/chicken.png was absent from the lists).
		// Returning NULL here turns that into "texture unchanged" instead of a wild read.
		static Image* SafeImage(std::vector<Image*>& images, int number)
		{
			if(number < 0 || number >= (int)images.size())
				return NULL;
			Image* image = images[number];
			// A released texture keeps its slot (see ReleaseTextureData) so every cached number stays
			// valid, but its ImageData is NULL, and handing that to sceGuTexImage would point the GE
			// at address zero. Treat it as absent: the draw becomes a no-op instead of a wild read,
			// which is the same failure mode this function was added for.
			if(image != NULL && image->ImageData == NULL)
				return NULL;
			return image;
		}

		void TextureManager::SetTexture(int number,int minFilter,int maxFilter)
		{
			FlushDirtyTexels();
			if(currentTexture != number)
			{
				Image *image = SafeImage(Images, number);
				if(image == NULL)
					return;

				sceGuTexMode(image->ColorMode,0,0,1);
				sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
				sceGuTexFilter(minFilter,maxFilter);
				sceGuTexOffset( 0.0f, 0.0f );
				sceGuTexWrap(GU_REPEAT,GU_REPEAT);
				sceGuTexImage(0,image->power2Width,image->power2Height,image->power2Width,image->ImageData);
				//sceGuTexImage(0,image->Width,image->Height,image->Width,image->ImageData);

				currentTexture = number;
			}
		}

        void TextureManager::SetConstTexture(int number,int minFilter,int maxFilter)
		{
			//if(currentTexture != number)
			//{
				Image *image = SafeImage(ConstImages, number);
				if(image == NULL)
					return;

				sceGuTexMode(image->ColorMode,0,0,1);
				sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
				sceGuTexFilter(minFilter,maxFilter);
				sceGuTexOffset( 0.0f, 0.0f );
				sceGuTexWrap(GU_REPEAT,GU_REPEAT);
				sceGuTexImage(0,image->power2Width,image->power2Height,image->power2Width,image->ImageData);
				//sceGuTexImage(0,image->Width,image->Height,image->Width,image->ImageData);

				currentTexture = number;
			//}
		}

		void TextureManager::SetTextureModeulate(int number)
		{
			FlushDirtyTexels();
			Image *image = SafeImage(Images, number);
			if(image == NULL)
				return;

			// Replacing the pixels of a texture that is already bound has to upload again. The
			// number did not change, so the old test kept the previous sheet on the GE -- the
			// player hand and the third-person model stayed on Steve after a skin was equipped.
			if(currentTexture == number && boundTexels == image->ImageData)
				return;

			sceGuTexMode(image->ColorMode,0,0,image->Swizzle);
			sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
			sceGuTexFilter(GU_NEAREST,GU_NEAREST);
			sceGuTexOffset( 0.0f, 0.0f );
			sceGuTexWrap(GU_CLAMP,GU_CLAMP);
			sceGuTexImage(0,image->power2Width,image->power2Height,image->power2Width,image->ImageData);

			currentTexture = number;
			boundTexels = image->ImageData;
		}

		void TextureManager::SetMipMapsTextures(int texture,int mipmap1,int mipmap2,int mipmap3)
		{
			FlushDirtyTexels();
			if(currentTexture != texture)
			{
				Image *image = SafeImage(Images, texture);
				Image *mip1 = SafeImage(Images, mipmap1);
				Image *mip2 = SafeImage(Images, mipmap2);
				Image *mip3 = SafeImage(Images, mipmap3);
				if(image == NULL || mip1 == NULL || mip2 == NULL || mip3 == NULL)
					return;

                sceGuTexMode(image->ColorMode,0,0,image->Swizzle);
				sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
				sceGuTexFilter(GU_NEAREST,GU_NEAREST);
				sceGuTexOffset( 0.0f, 0.0f );
				sceGuTexWrap(GU_CLAMP,GU_CLAMP);

				sceGuTexImage(0,image->power2Width,image->power2Height,image->power2Width,image->ImageData);
				sceGuTexImage(1,mip1->power2Width,mip1->power2Height,mip1->power2Width,mip1->ImageData);
				sceGuTexImage(2,mip2->power2Width,mip2->power2Height,mip2->power2Width,mip2->ImageData);
				sceGuTexImage(3,mip3->power2Width,mip3->power2Height,mip3->power2Width,mip3->ImageData);

				currentTexture = texture;
			}
		}

        void TextureManager::SetMipMaps2Textures(int texture,int mipmap1)
		{
			SetMipMaps2Textures(texture, mipmap1, 0.0f);
		}

		// `bias` is the level-of-detail offset handed to the GE. 0 leaves the selection to the
		// hardware; positive shifts it toward the smaller mip (the half-size terrain_mips atlas),
		// which is the crossover the caller tunes. See CraftWorld options.mipMapBias.
		void TextureManager::SetMipMaps2Textures(int texture,int mipmap1,float bias)
		{
			FlushDirtyTexels();
			if(currentTexture != texture)
			{
				Image *image = SafeImage(Images, texture);
				Image *mip1 = SafeImage(Images, mipmap1);
				if(image == NULL || mip1 == NULL)
					return;

                sceGuTexMode(image->ColorMode,2,0,image->Swizzle);
				sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
				sceGuTexOffset( 0.0f, 0.0f );
				sceGuTexWrap(GU_CLAMP,GU_CLAMP);
				sceGuTexLevelMode(GU_TEXTURE_AUTO,bias);
				sceGuTexFilter(GU_NEAREST_MIPMAP_NEAREST,GU_NEAREST);

				sceGuTexImage(0,image->power2Width,image->power2Height,image->power2Width,image->ImageData);
				sceGuTexImage(1,image->power2Width,image->power2Height,image->power2Width,image->ImageData);
				sceGuTexImage(2,mip1->power2Width,mip1->power2Height,mip1->power2Width,mip1->ImageData);

				currentTexture = texture;
			}
		}

		bool TextureManager::FileExist(std::string fileName)
		{
			FILE *fp;

			if ((fp = fopen(fileName.c_str(), "r")) == NULL)
			{
				Utils::Logger::Instance()->LogMessage("Can't loaded texture: %s\n",fileName.c_str());
				return false;
			}

			fclose(fp);

			return true;
		}


		void TextureManager::LoadTexture(std::string fileName)
		{
			// The conservative default. Callers that can afford two bytes per texel ask for it
			// through the overload below.
			LoadTexture(fileName, GU_PSM_8888);
		}

		void TextureManager::LoadTexture(std::string fileName, int colorMode)
		{
			//check if texture is loaded
			if(TextureExist(fileName))
			{
				MC_TM("LoadTexture: already loaded, nothing to do");
				return;
			}

			MC_TM("LoadTexture: enter");
			MC_TMP("LoadTexture: file", fileName.c_str());

			//load texture
			if(fileName.substr(fileName.find_last_of(".") + 1) == "png")
			{
				//load png image
				Image *image = LoadPng(fileName, colorMode);
				MC_TMP("LoadTexture: LoadPng returned", image);
				if (image != NULL)
				{
					Names.push_back(fileName);
					Images.push_back(image);
					Utils::Logger::Instance()->LogMessage("Loaded texture: %s\n",fileName.c_str());
				}else
					Utils::Logger::Instance()->LogMessage("Can't open file %s\n",fileName.c_str());
			}
			else
			{
				MC_TM("LoadTexture: not a .png, skipped");
			}
		}

		void TextureManager::LoadConstTexture(std::string fileName)
		{

			//load texture
			if(fileName.substr(fileName.find_last_of(".") + 1) == "png")
			{
				//load png image
				Image *image = LoadPng(fileName);
				if (image != NULL)
				{
					ConstNames.push_back(fileName);
					ConstImages.push_back(image);
					Utils::Logger::Instance()->LogMessage("Loaded texture: %s \n",fileName.c_str());
				}else
					Utils::Logger::Instance()->LogMessage("Can't open file %s \n",fileName.c_str());
			}
		}

		void TextureManager::LoadTexureVFS(std::string fileName)
		{
			if(fileName.substr(fileName.find_last_of(".") + 1) == "png")
			{
				//load png image
				Image *image = LoadPngFromMemory(fileName);
				if (image != NULL)
				{
					Names.push_back(fileName);
					Images.push_back(image);
					Utils::Logger::Instance()->LogMessage("Loaded texture: %s \n",fileName.c_str());
				}else
					Utils::Logger::Instance()->LogMessage("Can't open file %s \n",fileName.c_str());
			}
		}
        //return testImage->ImageData[(1+(x * 4)) + (y * (testImage->Width*4))] >> 8;

		// Index (in 16-bit units) of a 32bpp texel inside a swizzled image: swizzle
		// blocks are 16 bytes (4 texels) wide by 8 rows, laid out row-major. This
		// replaces the old float-division version, which the water/lava/fire
		// animation called thousands of times per tick.
		static inline int SwizzledTexelIndex16(const Image* img, int x, int y)
		{
			int boxesPerRow = img->Width >> 2;
			int box = (x >> 2) + (y >> 3) * boxesPerRow;
			return box * 64 + ((x & 3) << 1) + ((y & 7) << 3);
		}

		int TextureManager::getRed(int x, int y, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;
		    return testImage->ImageData[SwizzledTexelIndex16(testImage, x, y)] & 0xFF;
		}

		int TextureManager::getGreen(int x, int y, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;
		    return testImage->ImageData[SwizzledTexelIndex16(testImage, x, y)] >> 8;
		}

		int TextureManager::getBlue(int x, int y, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;
		    return testImage->ImageData[SwizzledTexelIndex16(testImage, x, y) + 1] & 0xFF;
		}

		int TextureManager::getAlpha(int x, int y, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;
		    return testImage->ImageData[SwizzledTexelIndex16(testImage, x, y) + 1] >> 8;
		}

		void TextureManager::setColour(int x,int y, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return;
		    int idx = SwizzledTexelIndex16(testImage, x, y);
            testImage->ImageData[idx] = (red) | (green << 8);
            testImage->ImageData[idx+1] = (blue) | (alpha << 8);
		}

		unsigned int TextureManager::getPixelRaw(int x, int y, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;
		    int idx = SwizzledTexelIndex16(testImage, x, y);
		    return (unsigned int)testImage->ImageData[idx] | ((unsigned int)testImage->ImageData[idx+1] << 16);
		}

		void TextureManager::setPixelRaw(int x, int y, unsigned int raw, int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return;
		    int idx = SwizzledTexelIndex16(testImage, x, y);
		    testImage->ImageData[idx] = (unsigned short)(raw & 0xFFFF);
		    testImage->ImageData[idx+1] = (unsigned short)(raw >> 16);
		}

		void TextureManager::FlushTexelRegion(int x0, int y0, int x1, int y1, int number)
		{
		    // CPU writes sit in the data cache; the GE reads RAM directly. Write back the
		    // swizzle blocks covering the region and ask for a GE texture-cache flush on
		    // the next bind (must be emitted inside the display list).
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return;
		    int lo = SwizzledTexelIndex16(testImage, x0 & ~3, y0 & ~7);
		    int hi = SwizzledTexelIndex16(testImage, x1 & ~3, y1 & ~7) + 64;
		    if(hi > lo)
		        sceKernelDcacheWritebackRange(testImage->ImageData + lo, (hi - lo) * sizeof(unsigned short));
		    texelsDirty = true;
		}

		void TextureManager::FlushDirtyTexels()
		{
		    if(texelsDirty)
		    {
		        sceGuTexFlush();
		        texelsDirty = false;
		    }
		}

		float TextureManager::getWidth(int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0.0f;

            return testImage->Width;
		}

		int TextureManager::getHeight(int number)
		{
		    Image* testImage = SafeImage(Images, number);
		    if(testImage == NULL)
		        return 0;

            return testImage->Height;
		}

		Image* TextureManager::LoadPng(std::string fileName)
		{
			return LoadPng(fileName.c_str(),GU_PSM_8888,1,0);
		}

		Image* TextureManager::LoadPng(std::string fileName, int colorMode)
		{
			// The path that picks the stored format. GU_PSM_8888 keeps what this used to do for
			// every texture, so a caller that does not ask for anything specific is unaffected.
			return LoadPng(fileName.c_str(),colorMode,1,0);
		}

		Image* TextureManager::LoadPngFromMemory(std::string fileName)
		{
			return NULL;
		}

		Image* TextureManager::LoadPng(const char* filename,int ColorMode,int Swizzle,int Vram,int cropClassicSkin)
		{
			unsigned short *Buffer;
			//unsigned short *swizzled_pixels = NULL;

			int OutBytesPerPixel;
			int Power2Width = 0;
			int Power2Height = 0;

			MC_TM("LoadPng: enter");
			MC_TMI("LoadPng: ColorMode", ColorMode);
			MC_TMI("LoadPng: Vram", Vram);

			png_structp png_ptr;
			png_infop info_ptr;
			unsigned int sig_read = 0;
			png_uint_32 width, height,x, y;
			int bit_depth, color_type, interlace_type;
			unsigned int* line;
			FILE *fp;

			// NULL until they are allocated, so the error path below can free them unconditionally.
			line = NULL;
			Buffer = NULL;

			if(ColorMode == GU_PSM_4444 || ColorMode == GU_PSM_5650 || ColorMode == GU_PSM_5551)
				OutBytesPerPixel = 2;
			else
				OutBytesPerPixel = 4;

			if ((fp = fopen(filename, "rb")) == NULL)
			{
				printf("Can't open file %s\n",filename);
				MC_TM("LoadPng: fopen FAILED, returning NULL");
				return NULL;
			}

			MC_TM("LoadPng: fopen ok");

			png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			MC_TMP("LoadPng: png_create_read_struct", png_ptr);
			if (png_ptr == NULL)
			{
				fclose(fp);
				return NULL;
			}

			info_ptr = png_create_info_struct(png_ptr);
			MC_TMP("LoadPng: png_create_info_struct", info_ptr);
			if (info_ptr == NULL)
			{
				fclose(fp);
				png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
				return NULL;
			}

			/* THE JUMP TARGET, and the one line whose absence caused this crash.
			 *
			 * libpng reports a decode failure by calling the error function, whose default ends in a
			 * longjmp to this buffer. Without a setjmp here the buffer is uninitialised, so the first
			 * problem libpng found branched through garbage and took the console with it -- inside
			 * png_read_info, with nothing written to the log, on hardware only. See PngErrorHandler at
			 * the top of this file for how that was established.
			 *
			 * setjmp returns 0 when it is primed and 1 when the handler jumps back, so this reads as
			 * "if libpng has failed, unwind this frame". Everything allocated before the failure is
			 * released here: freeing NULL is harmless, which is why line and Buffer start NULL. */
			if (setjmp(png_jmpbuf(png_ptr)))
			{
				MC_TM("LoadPng: libpng reported a failure; abandoning this file");
				if (line != NULL)
					free(line);
				if (Buffer != NULL)
					free(Buffer);
				png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
				fclose(fp);
				return NULL;
			}

			// Registered after the jump target, before anything that can report a problem.
			png_set_error_fn(png_ptr, NULL, PngErrorHandler, PngWarningHandler);

			MC_TM("LoadPng: setup enter");
			png_init_io(png_ptr, fp);
			png_set_sig_bytes(png_ptr, sig_read);

			MC_TM("LoadPng: png_read_info enter");
			png_read_info(png_ptr, info_ptr);
			MC_TM("LoadPng: png_read_info done");

			png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, int_p_NULL, int_p_NULL);
			MC_TMI("LoadPng: width", width);
			MC_TMI("LoadPng: height", height);
			MC_TMI("LoadPng: bit_depth", bit_depth);
			MC_TMI("LoadPng: color_type", color_type);
			MC_TMI("LoadPng: OutBytesPerPixel", OutBytesPerPixel);

			png_set_strip_16(png_ptr);
			png_set_packing(png_ptr);

			if (color_type == PNG_COLOR_TYPE_PALETTE)
				png_set_palette_to_rgb(png_ptr);
			if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
				png_set_expand_gray_1_2_4_to_8(png_ptr);
			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
				png_set_tRNS_to_alpha(png_ptr);

			png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
			MC_TM("LoadPng: transforms set");

			line = (unsigned int*)malloc(width * 4);
			MC_TMP("LoadPng: row line buffer", line);
			if (!line)
			{
				fclose(fp);
				png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
				return NULL;
			}

			Power2Width = PowerOfTwo(width);
			Power2Height = PowerOfTwo(height);
			MC_TMI("LoadPng: Power2Width", Power2Width);
			MC_TMI("LoadPng: Power2Height", Power2Height);
			//Buffer = (unsigned short*)memalign(16,Power2Width*Power2Height*OutBytesPerPixel);
			//Buffer = (unsigned short*)malloc(sizeof(unsigned short) * (Power2Width*Power2Height*OutBytesPerPixel));
			Buffer = (unsigned short*)malloc(Power2Width*Power2Height*OutBytesPerPixel);
			MC_TMP("LoadPng: texel buffer", Buffer);
			MC_TMI("LoadPng: texel bytes", Power2Width*Power2Height*OutBytesPerPixel);
			// The pixel loop below writes into this through Buffer32/Buffer with no further guard, so a
			// failed allocation here faults the console at address 4 rather than reporting anything.
			// Peak resident cost is line + Buffer + the image data below, i.e. about twice the texture
			// -- which is why this is reachable in the nether and nowhere else.
			if (Buffer == NULL)
			{
				MC_TM("LoadPng: texel buffer allocation failed, abandoning this file");
				free(line);
				png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
				fclose(fp);
				return NULL;
			}

			MC_TM("LoadPng: png_read_row loop enter");
			for (y = 0; y < height; y++)
			{
				png_read_row(png_ptr, (unsigned char*) line, NULL);

				for (x = 0; x < width; x++)
				{
					unsigned int *Buffer32 = (unsigned int*)Buffer;
					unsigned int color32 = line[x];
					unsigned short color16;

					if(ColorMode == GU_PSM_5551)
					{
						color16 = Color8888To5551(color32);
						Buffer[y*Power2Width+x] = color16;
					}
					else if(ColorMode == GU_PSM_4444)
					{
						color16 = Color8888To4444(color32);
						Buffer[y*Power2Width+x] = color16;
					}
					else if(ColorMode == GU_PSM_5650)
					{
						color16 = Color8888To5650(color32);
						Buffer[y*Power2Width+x] = color16;
					}
					else
					{
						Buffer32[y*Power2Width+x] = color32;
					}
				}
			}

			MC_TM("LoadPng: png_read_row loop done");

			free(line);
			line = NULL;	// so a libpng failure after this point cannot free it twice
			MC_TM("LoadPng: row line buffer freed");

			// A 64x64 skin keeps the body on the top 32 rows and the jacket, hat and sleeves on the
			// bottom, plus a second copy of the head in the hat slot. This model samples only the
			// top half, so a skin whose base layer is empty (the visible pixels are all on the
			// overlay) would draw as nothing. Lay the overlay onto the base while the pixels are
			// still in rows, then keep that top half.
			if (cropClassicSkin && width == 64 && height == 64 && OutBytesPerPixel == 4)
			{
				unsigned char* skin = (unsigned char*)Buffer;
				int skinStride = Power2Width * 4;
				const int layer[4][6] = {
					{32, 0,  0,  0,  32, 16},
					{0,  32, 0,  16, 16, 16},
					{16, 32, 16, 16, 24, 16},
					{40, 32, 40, 16, 16, 16}
				};
				for (int layerIndex = 0; layerIndex < 4; layerIndex++)
				{
					int sx = layer[layerIndex][0];
					int sy = layer[layerIndex][1];
					int dx = layer[layerIndex][2];
					int dy = layer[layerIndex][3];
					int lw = layer[layerIndex][4];
					int lh = layer[layerIndex][5];
					for (int row = 0; row < lh; row++)
					{
						unsigned char* src = skin + (sy + row) * skinStride + sx * 4;
						unsigned char* dst = skin + (dy + row) * skinStride + dx * 4;
						for (int col = 0; col < lw; col++)
						{
							int sourceAlpha = src[3];
							if (sourceAlpha != 0)
							{
								if (sourceAlpha == 255 || dst[3] == 0)
								{
									dst[0] = src[0];
									dst[1] = src[1];
									dst[2] = src[2];
									dst[3] = src[3];
								}
								else
								{
									int destAlpha = dst[3];
									int inverse = 255 - sourceAlpha;
									int outAlpha = sourceAlpha + destAlpha * inverse / 255;
									if (outAlpha < 1)
										outAlpha = 1;
									dst[0] = (unsigned char)((src[0] * sourceAlpha + dst[0] * destAlpha * inverse / 255) / outAlpha);
									dst[1] = (unsigned char)((src[1] * sourceAlpha + dst[1] * destAlpha * inverse / 255) / outAlpha);
									dst[2] = (unsigned char)((src[2] * sourceAlpha + dst[2] * destAlpha * inverse / 255) / outAlpha);
									dst[3] = (unsigned char)outAlpha;
								}
							}
							src += 4;
							dst += 4;
						}
					}
				}
				height = 32;
				Power2Height = 32;
			}

			png_read_end(png_ptr, info_ptr);
			MC_TM("LoadPng: png_read_end done");

			png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
			MC_TM("LoadPng: png_destroy_read_struct done");

			fclose(fp);
			MC_TM("LoadPng: fclose done");

			Image *Image1 = new Image();
			MC_TMP("LoadPng: new Image", Image1);
			// Same reasoning as the texel buffer: the field writes immediately below would store into a
			// low address if this came back null.
			if (Image1 == NULL)
			{
				MC_TM("LoadPng: Image allocation failed, abandoning this file");
				free(Buffer);
				return NULL;
			}

			Image1->Width = width;
			Image1->Height = height;
			Image1->power2Width = Power2Width;
			Image1->power2Height = Power2Height;
			Image1->vRam = Vram;
			Image1->ColorMode = ColorMode;
			Image1->Swizzle = Swizzle;

			MC_TM("LoadPng: allocating the image data");
			if (Vram == 1)
			{
				Image1->ImageData = (unsigned short*)getStaticVramTexture(Power2Width,Power2Height,ColorMode);

			}else
			{
				Image1->ImageData = (unsigned short*)malloc(Image1->power2Height*Image1->power2Width*OutBytesPerPixel);
			}
			MC_TMP("LoadPng: image data", Image1->ImageData);

			// THE GUARD THAT WAS MISSING, and the fault it caused. swizzle_fast is the very next
			// statement and it writes 128-byte blocks straight through this pointer; with a null
			// destination the first store is 4(v1) and the console dies on an unhandled "Write Word
			// at 00000004". Measured under the nether probe: the 512x512 title background reloading
			// into a heap with about 2 MB free, where line + Buffer + this buffer peak at roughly
			// twice the texture size. Returning NULL leaves the caller's texture released, which
			// SafeImage already renders as nothing -- the same outcome as an asset that is not on
			// the card, and a great deal better than a power-off.
			if (Image1->ImageData == NULL)
			{
				MC_TM("LoadPng: image data allocation FAILED, releasing and returning NULL");
				free(Buffer);
				Buffer = NULL;
				delete Image1;
				return NULL;
			}

			MC_TM("LoadPng: swizzle enter");
			if (Swizzle)
				swizzle_fast((u8*)Image1->ImageData,(const u8*)Buffer,Image1->power2Width*OutBytesPerPixel,Image1->power2Height);
			else
				memcpy(Image1->ImageData, Buffer, Image1->power2Height * Image1->power2Width * OutBytesPerPixel);
			MC_TM("LoadPng: swizzle done");

			free(Buffer);
			Buffer = NULL;	// as above: the cache flush below is not the last libpng-adjacent step
			MC_TM("LoadPng: texel buffer freed");

			//clear the cache or there will be some errors
			sceKernelDcacheWritebackInvalidateAll();
			MC_TM("LoadPng: dcache invalidated, returning");

			return Image1;
		}

		void TextureManager::ReplaceImage(int number, Image* fresh)
		{
			if (number < 0 || number >= (int)Images.size() || fresh == NULL)
			{
				if (fresh != NULL)
				{
					if (fresh->ImageData != NULL && fresh->vRam == 0)
						free(fresh->ImageData);
					delete fresh;
				}
				return;
			}

			Image* old = Images[number];
			if (old != NULL)
			{
				if (old->ImageData != NULL && old->vRam == 0)
					free(old->ImageData);
				delete old;
			}
			Images[number] = fresh;
			currentTexture = -1;
			boundTexels = NULL;
		}
	}
}
