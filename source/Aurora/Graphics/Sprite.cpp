#include <Aurora/Graphics/Sprite.h>
#include <Aurora/Graphics/TextureManager.h>

namespace Aurora
{

	namespace Graphics
	{

		Sprite::Sprite()
		{
			imageNumber = 0;
		}

		TexturesPSPVertex getVertex(float u,float v,float x, float y,float z)
		{
			TexturesPSPVertex vert;
			vert.u = u;
			vert.v = v;
			vert.x = x;
			vert.y = y;
			vert.z = z;
			return vert;
		}

		Sprite::Sprite(const char* filename)
		{
			TextureManager::Instance()->LoadTexture(filename);

			imageNumber = TextureManager::Instance()->GetTextureNumber(filename);

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = img->Width;
			height = img->Height;

			float hPercent = (float)img->Height / (float)img->power2Height;
			float wPercent = (float)img->Width / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(0.0f,0.0f,-img->Width/2,-img->Height/2,0.0f);
				vertices[1] = getVertex(0.0f,hPercent,-img->Width/2, img->Height/2,0.0f);
				vertices[2] = getVertex(wPercent,0.0f,img->Width/2,-img->Height/2,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,img->Width/2, img->Height/2,0.0f);
			}

			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::Sprite(std::string filename, bool constOne)
		{
		    if(constOne)
            {
                TextureManager::Instance()->LoadConstTexture(filename);

                imageNumber = TextureManager::Instance()->GetConstTextureNumber(filename);

                //generate wertices
                vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

                Image *img = TextureManager::Instance()->ConstImages[imageNumber];

                width = img->Width;
                height = img->Height;

                float hPercent = (float)img->Height / (float)img->power2Height;
                float wPercent = (float)img->Width / (float)img->power2Width;

                if( vertices )
                {
                    vertices[0] = getVertex(0.0f,0.0f,-img->Width/2,-img->Height/2,0.0f);
                    vertices[1] = getVertex(0.0f,hPercent,-img->Width/2, img->Height/2,0.0f);
                    vertices[2] = getVertex(wPercent,0.0f,img->Width/2,-img->Height/2,0.0f);
                    vertices[3] = getVertex(wPercent,hPercent,img->Width/2, img->Height/2,0.0f);
                }

                sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
            }
		}

		Sprite::Sprite(int textureNumer)
		{
			imageNumber = textureNumer;

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = img->Width;
			height = img->Height;

			float hPercent = (float)img->Height / (float)img->power2Height;
			float wPercent = (float)img->Width / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(0.0f,0.0f,-img->Width/2,-img->Height/2,0.0f);
				vertices[1] = getVertex(0.0f,hPercent,-img->Width/2, img->Height/2,0.0f);
				vertices[2] = getVertex(wPercent,0.0f,img->Width/2,-img->Height/2,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,img->Width/2, img->Height/2,0.0f);
			}

			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::Sprite(int textureNumer, bool constOne)
		{
			imageNumber = textureNumer;

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex));
            Image *img;
            if(constOne)
            {
                img = TextureManager::Instance()->ConstImages[imageNumber];
            }
			else
            {
                img = TextureManager::Instance()->Images[imageNumber];
            }

			width = img->Width;
			height = img->Height;

			float hPercent = (float)img->Height / (float)img->power2Height;
			float wPercent = (float)img->Width / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(0.0f,0.0f,-img->Width/2,-img->Height/2,0.0f);
				vertices[1] = getVertex(0.0f,hPercent,-img->Width/2, img->Height/2,0.0f);
				vertices[2] = getVertex(wPercent,0.0f,img->Width/2,-img->Height/2,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,img->Width/2, img->Height/2,0.0f);
			}

			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::Sprite(const char* filename,int startW,int startH,int endW,int endH)
		{
			TextureManager::Instance()->LoadTexture(filename);

			imageNumber = TextureManager::Instance()->GetTextureNumber(filename);

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = endW;
			height = endH;

			float hstart = (float)startH / (float)img->power2Height;
			float wstart = (float)startW / (float)img->power2Width;
			float hPercent = (float)(startH + endH) / (float)img->power2Height;
			float wPercent = (float)(startW + endW) / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(wstart,hstart,-width/2,-height/2,0.0f);
				vertices[1] = getVertex(wstart,hPercent,-width/2, height/2,0.0f);
				vertices[2] = getVertex(wPercent,hstart,width/2,-height/2,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,width/2, height/2,0.0f);
			}

			//sceKernelDcacheWritebackInvalidateAll();
			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::Sprite(int textureNumer,int startW,int startH,int endW,int endH)
		{
			imageNumber = textureNumer;

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = endW;
			height = endH;

			float hstart = (float)startH / (float)img->power2Height;
			float wstart = (float)startW / (float)img->power2Width;
			float hPercent = (float)(startH + endH) / (float)img->power2Height;
			float wPercent = (float)(startW + endW) / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(wstart,hstart,-width/2,-height/2,0.0f);
				vertices[1] = getVertex(wstart,hPercent,-width/2, height/2,0.0f);
				vertices[2] = getVertex(wPercent,hstart,width/2,-height/2,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,width/2, height/2,0.0f);
			}

			//sceKernelDcacheWritebackInvalidateAll();
			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::Sprite(int textureNumer,int startW,int startH,int endW,int endH, bool obr)
		{
			imageNumber = textureNumer;

			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = endW;
			height = endH;

			float hstart = (float)startH / (float)img->power2Height;
			float wstart = (float)startW / (float)img->power2Width;
			float hPercent = (float)(startH + endH) / (float)img->power2Height;
			float wPercent = (float)(startW + endW) / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(wstart,hstart,0,0,0.0f);
				vertices[1] = getVertex(wstart,hPercent,0,height,0.0f);
				vertices[2] = getVertex(wPercent,hstart,width,0,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,width, height,0.0f);
			}

			//sceKernelDcacheWritebackInvalidateAll();
			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

        void Sprite::SetMapPos(int textureNumer,int startW,int startH,int endW,int endH)
		{
			imageNumber = textureNumer;
            free(vertices);
			//generate wertices
			vertices = (TexturesPSPVertex*)memalign(16, 4 * sizeof(TexturesPSPVertex) );

			Image *img = TextureManager::Instance()->Images[imageNumber];

			width = endW;
			height = endH;

			float hstart = (float)startH / (float)img->power2Height;
			float wstart = (float)startW / (float)img->power2Width;
			float hPercent = (float)(startH + endH) / (float)img->power2Height;
			float wPercent = (float)(startW + endW) / (float)img->power2Width;

			if( vertices )
			{
				vertices[0] = getVertex(wstart,hstart,0,0,0.0f);
				vertices[1] = getVertex(wstart,hPercent,0,height,0.0f);
				vertices[2] = getVertex(wPercent,hstart,width,0,0.0f);
				vertices[3] = getVertex(wPercent,hPercent,width, height,0.0f);
			}

			//sceKernelDcacheWritebackInvalidateAll();
			sceKernelDcacheWritebackInvalidateRange(vertices, 4 * sizeof(TexturesPSPVertex));
		}

		Sprite::~Sprite()
		{
			free(vertices);
		}

		void Sprite::SetPosition(float x,float y)
		{
			posX = x;
			posY = y;
		}

        float Sprite::GetPositionX()
		{
			return posX;
		}

        float Sprite::GetPositionY()
		{
			return posY;
		}

		void Sprite::Scale(float x,float y)
		{
			scaleX = x;
			scaleY = y;

			for(int i=0; i<4; i++)
			{
				vertices[i].x = vertices[i].x * scaleX;
				vertices[i].y = vertices[i].y * scaleY;
			}

		}

		void Sprite::NewScale(float x)
		{
			for(int i=2; i<4; i++)
			{
				vertices[i].x = x;
			}
		}


		void Sprite::Alpha(float _alpha)
		{
			alpha = _alpha;
			if(alpha < 0.0f)
				alpha = 0.0f;
			if(alpha > 1.0f)
				alpha = 1.0f;
		}

		// TextureManager::SetTexture leaves the texture function on GU_TFX_REPLACE, which replaces the
		// fragment with the texel and so ignores the vertex colour -- including any alpha in it. A
		// sprite that asked for an alpha therefore switches to MODULATE for its own draw and tints the
		// texel by the current colour instead. That only works while the texture is already bound:
		// SetTexture only touches the texture function when the texture number actually changes, so
		// the state has to be put back by hand afterwards, or the next sprite to draw the same texture
		// would inherit MODULATE with a stale colour.
		void Sprite::ApplyAlpha(bool tint)
		{
			if(tint == true)
			{
				sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
				sceGuColor(GU_COLOR(1.0f,1.0f,1.0f,alpha));
			}
			else
			{
				sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
				sceGuColor(0xFFFFFFFF);
			}
		}

		void Sprite::RGB(float _red, float _green, float _blue)
		{
		}

		void Sprite::Draw()
		{
			sceGumPushMatrix();

			ScePspFVector3 loc = {posX,posY,0.0f};
			sceGumTranslate(&loc);

			sceGuEnable(GU_TEXTURE_2D);
			TextureManager::Instance()->SetTexture(imageNumber,GU_NEAREST,GU_NEAREST);

			const bool tinted = (alpha < 1.0f);
			if(tinted == true)
				ApplyAlpha(true);

			sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, vertices);

			if(tinted == true)
				ApplyAlpha(false);

			sceGuDisable(GU_TEXTURE_2D);
			sceGumPopMatrix();
		}

        void Sprite::DrawLinear()
		{
			sceGumPushMatrix();

			ScePspFVector3 loc = {posX,posY,0.0f};
			sceGumTranslate(&loc);

			sceGuEnable(GU_TEXTURE_2D);
			TextureManager::Instance()->SetTexture(imageNumber,GU_LINEAR,GU_LINEAR);

			const bool tinted = (alpha < 1.0f);
			if(tinted == true)
				ApplyAlpha(true);

			sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, vertices);

			if(tinted == true)
				ApplyAlpha(false);

			sceGuDisable(GU_TEXTURE_2D);
			sceGumPopMatrix();
		}

        void Sprite::ConstDraw()
		{
			sceGumPushMatrix();

			ScePspFVector3 loc = {posX,posY,0.0f};
			sceGumTranslate(&loc);

			sceGuEnable(GU_TEXTURE_2D);
			TextureManager::Instance()->SetConstTexture(imageNumber,GU_NEAREST,GU_NEAREST);

			const bool tinted = (alpha < 1.0f);
			if(tinted == true)
				ApplyAlpha(true);

			sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, vertices);

			if(tinted == true)
				ApplyAlpha(false);

			sceGuDisable(GU_TEXTURE_2D);
			sceGumPopMatrix();
		}

		void Sprite::RemoveImage()
		{
			TextureManager::Instance()->RemoveTexture(imageNumber);
		}

	}

}

