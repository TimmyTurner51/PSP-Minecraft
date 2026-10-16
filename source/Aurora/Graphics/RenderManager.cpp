#include <Aurora/Graphics/RenderManager.h>
#include <math.h>

// Fine-grained boot tracing; see Scripts/InitTrace.h for why the init path is instrumented this way.
#include "InitTrace.h"

namespace Aurora
{
	namespace Graphics
	{
        float cubeSize = 0.5075f;
        SimplePSPVertex2 __attribute__((aligned(16))) top[4] =
		{
			{-cubeSize,-cubeSize,cubeSize},	// Top
			{cubeSize,-cubeSize,cubeSize},
			{ cubeSize,-cubeSize,-cubeSize},
			{ -cubeSize,-cubeSize, -cubeSize}
		};
		SimplePSPVertex2 __attribute__((aligned(16))) bottom[4] =
		{
			{-cubeSize, cubeSize, -cubeSize},	// Bottom
			{ cubeSize, cubeSize, -cubeSize},
			{ cubeSize, cubeSize, cubeSize},
			{-cubeSize, cubeSize, cubeSize}
		};
		SimplePSPVertex2 __attribute__((aligned(16))) front[4] =
		{
			{-cubeSize,-cubeSize, cubeSize},	// Front
			{ cubeSize,-cubeSize, cubeSize},
			{ cubeSize, cubeSize, cubeSize},
			{-cubeSize, cubeSize, cubeSize}
		};
		SimplePSPVertex2 __attribute__((aligned(16))) back[4] =
		{
			{- cubeSize,cubeSize, -cubeSize},	// Back
			{ cubeSize,cubeSize,-cubeSize},
			{ cubeSize, -cubeSize,-cubeSize},
			{ -cubeSize, -cubeSize, -cubeSize}
		};
		SimplePSPVertex2 __attribute__((aligned(16))) left[4] =
		{
			{ -cubeSize,-cubeSize,cubeSize},	// Left
			{-cubeSize,cubeSize,cubeSize},
			{-cubeSize, cubeSize,-cubeSize},
			{ -cubeSize, -cubeSize,-cubeSize}
		};
		SimplePSPVertex2 __attribute__((aligned(16))) right[4] =
		{
			{cubeSize,-cubeSize,-cubeSize},	// Right
			{cubeSize,cubeSize, -cubeSize},
			{cubeSize, cubeSize, cubeSize},
			{cubeSize, -cubeSize,cubeSize}
		};

        void RenderManager::DrawCube(float x, float y, float z)
        {
           /* sceGumPushMatrix();
			ScePspFVector3 move = {x,y,z};
			sceGumTranslate(&move);
			sceGuColor(GU_COLOR(0,0,0,1));
			sceGuDisable(GU_TEXTURE_2D);
			sceGuShadeModel(GU_FLAT);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, top);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, bottom);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, front);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, back);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, left);
			sceGumDrawArray( GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, 4, 0, right);
			sceGuShadeModel(GU_SMOOTH);
			sceGumPopMatrix();*/
        }

		void RenderManager::Init()
		{
		    fontVerticalShift = 0;
		    fontType = 0;
		    defaultFontType = 1;

		    SetFont(1);
			mVerticalSync = false;
			worldOrigin.x = 0.0f;
			worldOrigin.y = 0.0f;
			worldOrigin.z = 0.0f;
			listNum = 0;
			cleanColor = 0xFF00FF00; //0xFFF5B783
			fov = 65.0f;//54
			fovv = 65.0f;
			aspect =  480.0f / 272.0f;
			znear = 0.1f;
			zfar = 200.0f;
			screenNumber = 0;
			MC_Trace("rm: fields initialised, sceGuInit next");

			//initialize GU engine
			// The GU calls below are the first code that touches the graphics hardware directly, so each
			// one is named: in the emulator they are bookkeeping, on a console they are real registers.
			sceGuInit();
			MC_Trace("rm: sceGuInit ok");

			sceGuStart(GU_DIRECT,list);
			MC_Trace("rm: sceGuStart ok");

            sceGuDrawBuffer( GU_PSM_8888, SCEGU_VRAM_BP32_0, BUF_WIDTH );
			sceGuDispBuffer( SCR_WIDTH, SCR_HEIGHT, SCEGU_VRAM_BP32_1, BUF_WIDTH);
			sceGuDepthBuffer( SCEGU_VRAM_BP32_2, BUF_WIDTH);
			MC_Trace("rm: draw/disp/depth buffers set");

			sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
			sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
			MC_Trace("rm: offset+viewport set");

            sceGuDepthRange(50000, 10000);
			MC_Trace("rm: depth range set");

			sceGuEnable(GU_SCISSOR_TEST);
			sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
			sceGuEnable(GU_SCISSOR_TEST);
			sceGuDepthFunc(GU_GEQUAL);
			sceGuEnable(GU_DEPTH_TEST);
			MC_Trace("rm: scissor+depth test set");

			sceGuDisable(GU_TEXTURE_2D);
			sceGuEnable(GU_CLIP_PLANES);
			MC_Trace("rm: texture2d off, clip planes on");

			sceGuEnable(GU_CULL_FACE);
            sceGuFrontFace(GU_CCW);
			MC_Trace("rm: cull face set");

			sceGuEnable(GU_BLEND);
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
			sceGuAlphaFunc(GU_GREATER, 0.0f, 0xff );
			MC_Trace("rm: blend+alpha set");

			sceGuStencilFunc(GU_ALWAYS, 1, 1); // always set 1 bit in 1 bit mask
			sceGuStencilOp(GU_KEEP, GU_KEEP, GU_REPLACE); // keep value on failed test (fail and zfail) and replace on pass
			MC_Trace("rm: stencil set");

			sceGuTexFilter(GU_LINEAR,GU_LINEAR);
			MC_Trace("rm: tex filter set, sceGuFinish next");
			sceGuFinish();
			MC_Trace("rm: sceGuFinish ok, sceGuSync next");
			sceGuSync(0,0);
			MC_Trace("rm: sceGuSync ok");

			sceDisplayWaitVblankStart();
			MC_Trace("rm: vblank waited");
			sceGuDisplay(GU_TRUE);
			MC_Trace("rm: display on");

			sceCtrlSetSamplingCycle(0);
			sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
			MC_Trace("rm: controller sampling set");

			//initialize variables
			mCam = 0;

			g_vbl_count = -1;
			g_vbl_step = 1;
			g_vbl_time = 0;
			g_cpu_load = 0;
			g_gpu_load = 0;
			g_frame_count = 0;
			performanceCounter = false;
			lastGeWaitUs = 0;
			lastVblankUs = 0;
			framePending = false;
			frameSubmitTime = 0;
			frameRetiredHook = 0;
			sceGuShadeModel(GU_SMOOTH);
			MC_Trace("rm: Init complete");
		}

		void RenderManager::InitDebugFont()
		{
			MC_Trace("rm: intraFontInit enter");
			intraFontInit();
			MC_Trace("rm: intraFontInit done");

			// The returned handles are recorded, not just the fact that the call returned: a null handle
			// here reaches intraFontSetStyle and the draw path, where it faults somewhere less obvious
			// than the load that failed.
			MC_Trace("rm: intraFontLoad numerals");
			numFont = intraFontLoad("Assets/Fonts/numerals.pgf",INTRAFONT_CACHE_ASCII);
			MC_TracePtr("rm: numerals handle", numFont);
			MC_Trace("rm: intraFontLoad eng");
			engFont = intraFontLoad("Assets/Fonts/eng_letters.pgf",INTRAFONT_CACHE_ASCII);
			MC_TracePtr("rm: eng handle", engFont);
			MC_Trace("rm: intraFontLoad rus");
			rusFont = intraFontLoad("Assets/Fonts/rus_letters.pgf",INTRAFONT_CACHE_ASCII);
			MC_TracePtr("rm: rus handle", rusFont);

			intraFontSetStyle(numFont,0.5f,WHITE,BLACK,INTRAFONT_ALIGN_CENTER);
			intraFontSetStyle(engFont,0.5f,WHITE,BLACK,INTRAFONT_ALIGN_CENTER);
			intraFontSetStyle(rusFont,0.5f,WHITE,BLACK,INTRAFONT_ALIGN_CENTER);
			MC_Trace("rm: font styles set");

			debugFont = engFont;
			MC_Trace("rm: InitDebugFont complete");
		}

		void RenderManager::SetFontStyle(float size, unsigned int color, unsigned int shadowColor, unsigned int options)
		{
		    if(fontType != 0)
		    {
		        if(shadowColor == 0 || shadowColor == 999)
                {
                    intraFontSetStyle(debugFont, size, color, 0, options);
                }
                else
                {
                    intraFontSetStyle(debugFont, size, color, shadowColor, options);
                }
		    }
		    else
            {
                intraFontSetStyle(debugFont, size, color, shadowColor, options);
            }
		}

        void RenderManager::SetFont(int type)
		{
		    fontType = type;
			switch(type)
			{
			    case 0: debugFont = numFont; fontVerticalShift = 0; break;
			    case 1: debugFont = engFont; fontVerticalShift = -4; break;
			    case 2: debugFont = rusFont; fontVerticalShift = -4; break;
			}
		}

        void RenderManager::SetDefaultFont()
		{
		    SetFont(defaultFontType);
		}


        int RenderManager::GetFontLanguage()
		{
		    return fontType;
		}

		void RenderManager::DebugPrint(int x,int y, const char *message, ...)
		{
			va_list argList;
			char cbuffer[512];
			va_start(argList, message);
			vsnprintf(cbuffer, 512, message, argList);
			va_end(argList);

			sceGuEnable(GU_BLEND);
			sceGuEnable(GU_TEXTURE_2D);

			intraFontPrint(debugFont, x, y+fontVerticalShift, cbuffer);

			// intraFont binds its own texture: sceGuTexImage, as a 4-bit paletted image with a 2-entry
			// CLUT, and it switches the texture function to GU_TFX_MODULATE. All of that is done behind
			// TextureManager's back, so the class still believes whatever it last bound is current --
			// and SetTexture is a no-op while the number it is asked for has not changed. Anything drawn
			// next with that same texture would therefore sample the font atlas with the wrong palette
			// and the wrong texture function: a button that comes out as a plain white box because a
			// label was drawn over the button above it, for instance. Telling the manager its binding is
			// stale costs one texture setup on the next bind and makes text safe to interleave with
			// sprites and models, which is what every menu and the HUD do.
			TextureManager::Instance()->InvalidateCurrentTexture();

			sceGuDisable(GU_BLEND);
			sceGuDisable(GU_TEXTURE_2D);
		}

		int RenderManager::GetTextWidth(const char *message)
		{
			if(debugFont == 0)
				return 0;
			return (int)intraFontMeasureText(debugFont, message);
		}

		void RenderManager::Start()
		{
			FinishPendingFrame();
			sceGuStart(GU_DIRECT,list);
		}

		void RenderManager::SetFrameRetiredHook(void (*hook)())
		{
			frameRetiredHook = hook;
		}

		void RenderManager::WaitForGe()
		{
			if(!framePending)
				return;
			sceGuSync(0,0);
		}

		void RenderManager::FinishPendingFrame()
		{
			if(!framePending)
				return;
			framePending = false;

			// The GE has been drawing the list since EndFrame's sceGuFinish, and the game has been
			// running its update in the meantime. This is where the two meet again.
			int gpu_time;
			if(!performanceCounter)
			{
				sceGuSync(0,0);
				gpu_time = sceKernelGetSystemTimeLow();
				if(frameRetiredHook != 0)
					frameRetiredHook();
				sceDisplayWaitVblankStart();
				sceGuSwapBuffers();
				lastGeWaitUs = (unsigned int)(gpu_time - (int)frameSubmitTime);
				return;
			}

			int sync_start = sceKernelGetSystemTimeLow();
			sceGuSync(0,0);
			gpu_time = sceKernelGetSystemTimeLow();
			// Nothing the GE could still be reading is released before this point.
			if(frameRetiredHook != 0)
				frameRetiredHook();
			sceDisplayWaitVblankStart();
			sceGuSwapBuffers();

			g_frame_count ++ ;

			//performance
			int vbl_time = sceKernelGetSystemTimeLow();
			int vbl_count = sceDisplayGetVcount();
			// geWait is the part of the GE's time the CPU actually stood still for: whatever was left
			// of the list after the update that ran alongside it.
			lastGeWaitUs = (unsigned int)(gpu_time - sync_start);
			lastVblankUs = (unsigned int)(vbl_time - gpu_time);

			if ( g_vbl_count >= 0 )
			{
				g_vbl_step = vbl_count - g_vbl_count ;
				if ( g_frame_count % 6 == 0 )
				{
					g_cpu_load = ( (int)frameSubmitTime - g_vbl_time ) * 100 / 16667 ;
					g_gpu_load = ( gpu_time - g_vbl_time ) * 100 / 16667 ;
				}
			}
			g_vbl_time = vbl_time ;
			g_vbl_count = vbl_count ;
		}

		void RenderManager::CleanBuffers()
		{
			sceGuClearColor(cleanColor);
			sceGuClearStencil(0);
			sceGuClearDepth(0);
			sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
			//reset textures
			TextureManager::Instance()->currentTexture = -1;
		}

		void RenderManager::StartFrame(float a, float b, float c)
		{
			FinishPendingFrame();
			sceGuStart(GU_DIRECT,list);

            if(a > 1.0f)
            {
                a = 1.0f;
            }
            if(b > 1.0f)
            {
                b = 1.0f;
            }
            if(c > 1.0f)
            {
                c = 1.0f;
            }

            sceGuClearColor(GU_COLOR(a,b,c,1.0));
			sceGuClearStencil(0);
			sceGuClearDepth(0);
			sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

			//set camera look at -- later maybe i will move this code somewhere else
			if(mCam != NULL)
			{
				SetupCameraMatrices(false);
			}

			TextureManager::Instance()->currentTexture = -1;
			sceGuShadeModel(GU_SMOOTH);
		}

		void RenderManager::LookAt()
		{
			if(mCam != NULL)
			{
				SetupCameraMatrices(true);
			}
		}

		void RenderManager::SetupCameraMatrices(bool flushMatrix)
		{
			sceGumMatrixMode(GU_PROJECTION);
			sceGumLoadIdentity();
			sceGumLoadMatrix(&proj);

			// Origin near the camera, snapped to 64 blocks so it changes rarely.
			// Everything below is expressed relative to it: eye, view, and the base
			// model matrix. Keeps camera-space math in small floats.
			worldOrigin.x = floorf(mCam->m_vPosition.x / 64.0f) * 64.0f;
			worldOrigin.y = 0.0f;
			worldOrigin.z = floorf(mCam->m_vPosition.z / 64.0f) * 64.0f;

			sceGumMatrixMode(GU_VIEW);
			sceGumLoadIdentity();

			ScePspFVector3 pos = {mCam->m_vPosition.x + mCam->m_vOffset.x + mCam->m_vBoom.x - worldOrigin.x,
			                      mCam->m_vPosition.y + mCam->m_vOffset.y + mCam->m_vBoom.y - worldOrigin.y,
			                      mCam->m_vPosition.z + mCam->m_vOffset.z + mCam->m_vBoom.z - worldOrigin.z};
			// Look direction rebuilt from yaw/pitch (not from m_vView) so it does not
			// inherit rounding from the large absolute position.
			float yaw = mCam->horAngle * 3.14159265f / 180.0f;
			float sp = sinf(mCam->upDownAngle);
			float cp = cosf(mCam->upDownAngle);
			// Front view: the same ray through the eye, seen from the other side, so 180 of yaw and
			// the pitch negated (cos is even, so cp is unchanged). This is the whole of the mirror --
			// the camera's position already carries the matching boom.
			if(mCam->m_vMirror)
			{
				yaw += 3.14159265f;
				sp = -sp;
			}
			float cy = cosf(yaw);
			float sy = sinf(yaw);
			ScePspFVector3 eye = {pos.x + cp * cy, pos.y + sp, pos.z + cp * sy};
			// up is continuous for every pitch: no roll snap near vertical
			ScePspFVector3 up = {-sp * cy, cp, -sp * sy};

			// Portal roll, applied HERE and not in Camera::RebuildView. That was the original mistake:
			// RebuildView does maintain m_vUpVector, but this function ignores it and rebuilds up from
			// yaw and pitch alone, so a roll set on the camera never reached the view matrix at all.
			//
			// The rotation is inexpensive because up is already perpendicular to the look direction:
			// cross(look, up) reduces to the horizontal right vector (-sin yaw, 0, cos yaw), so rolling
			// by t is just up*cos(t) + right*sin(t). No general rotation is needed.
			if(mCam->rollAngle != 0.0f)
			{
				const float rollC = cosf(mCam->rollAngle);
				const float rollS = sinf(mCam->rollAngle);
				up.x = up.x * rollC - sy * rollS;
				up.y = up.y * rollC;
				up.z = up.z * rollC + cy * rollS;
			}

			sceGumLookAt(&pos, &eye, &up);
			if(flushMatrix)
			{
				sceGumUpdateMatrix();
			}

			sceGumStoreMatrix(&view);

			sceGumMatrixMode(GU_MODEL);
			LoadWorldModelMatrix();

			if(mCam->needUpdate)
			{
				UpdateFrustumMatrix();
				mCam->mFrustum.ExtractPlanes(projection_view_matrix);
				mCam->needUpdate = false;
			}
		}

		void RenderManager::LoadWorldModelMatrix()
		{
			sceGumLoadIdentity();
			ScePspFVector3 shift = {-worldOrigin.x, -worldOrigin.y, -worldOrigin.z};
			sceGumTranslate(&shift);
		}

		void RenderManager::EndFrame()
		{
			// Submit only. The GE starts on the list here and keeps drawing while the game runs the
			// next update; the wait for it, the vblank and the buffer swap happen in
			// FinishPendingFrame, which every entry point that touches the GU again calls first.
			// Until then nothing the list refers to may be freed or overwritten: mesh buffers go
			// through the frame-retired hook, and texture animation runs after StartFrame.
			sceGuFinish();
			frameSubmitTime = (unsigned int)sceKernelGetSystemTimeLow();
			framePending = true;
		}

		void RenderManager::StartDialog()
		{
			FinishPendingFrame();
			sceGuStart(GU_DIRECT,list);

			// The clear colour is set here rather than inherited, and this is not a detail. Without it
			// the clear uses whatever colour the game last set for its own sky, so every part of the
			// screen a system dialog does not paint comes back as that colour -- on the update screen a
			// pale, washed-out band rather than a neutral background. Every reference implementation of
			// these dialogs (the SDK's own Osk sample, apollo-psp's dialog.c) sets both of these first
			// for the same reason.
			sceGuClearColor(0xFF000000);
			sceGuClearDepth(0);
			sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
			sceGuFinish();
			sceGuSync(0,0);
		}

		void RenderManager::EndDialog()
		{
			sceDisplayWaitVblankStart();
			sceGuSwapBuffers();
		}

		void RenderManager::UseVerticalSync(bool Enabled)
		{
			mVerticalSync = Enabled;
		}

		void RenderManager::SetClearColor(float r,float g,float b,float a)
		{
			cleanColor = GU_COLOR(r,g,b,a);
			//GU_RGBA();
		}

		void RenderManager::SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar)
		{
			//some lame stuff XD
			sceGumMatrixMode(GU_PROJECTION);
			sceGumLoadIdentity();
			sceGumOrtho(0, 480, 272, 0, -30, 30);

			sceGumMatrixMode(GU_VIEW);
			sceGumLoadIdentity();

			sceGumMatrixMode(GU_MODEL);
			sceGumLoadIdentity();
		}

        void RenderManager::SetOrtho2(float left, float right, float bottom, float top, float zNear, float zFar)
		{
			//some lame stuff XD
			sceGumMatrixMode(GU_PROJECTION);
			sceGumLoadIdentity();
			sceGumOrtho(-1024, 1024, -1024, 1024, -1024, 1024);

			sceGumMatrixMode(GU_VIEW);
			sceGumLoadIdentity();

			sceGumMatrixMode(GU_MODEL);
			sceGumLoadIdentity();
		}

		void RenderManager::SetPerspective(float _fov,float _aspect,float _znear,float _zfar)
		{
			fov = _fov;
			aspect = _aspect;
			znear = _znear;
			zfar = _zfar;

			sceGumMatrixMode(GU_PROJECTION);
			sceGumLoadIdentity();

			sceGumPerspective(fovv,aspect,znear,zfar);
			sceGumStoreMatrix(&proj);

			sceGumMatrixMode(GU_VIEW);
			sceGumLoadIdentity();

			sceGumStoreMatrix(&view);

			sceGumMatrixMode(GU_MODEL);
			sceGumLoadIdentity();
		}



		void  RenderManager::DrawToTexture(Image* offscreenTexture )
		{
			//draw to texture
			sceGuDrawBufferList(GU_PSM_8888,(void*)offscreenTexture->ImageData,offscreenTexture->Width);

			// setup viewport
			sceGuOffset(2048 - (offscreenTexture->Width/2),2048 - (offscreenTexture->Height/2));
			sceGuViewport(2048,2048,offscreenTexture->Width,offscreenTexture->Height);

			//CleanBuffers();
		}

		void RenderManager::SetRTT()
		{
			// set frame buffer
			sceGuDrawBufferList(GU_PSM_8888,SCEGU_VRAM_BP32_0,BUF_WIDTH);

			// setup viewport
			sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
			sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);

			CleanBuffers();
		}

		void RenderManager::Draw(ObjModel *model)
		{
			sceGuEnable(GU_TEXTURE_2D);

			unsigned int count = model->mMeshes.size();

			for(unsigned int i = 0;i < count;i++)
			{
				ObjMesh *mesh = model->mMeshes[i];

				if(mesh->mMaterial != -1)
				{
					ObjMaterial *material = model->mMaterials[mesh->mMaterial];

					if(material->mipmapping)
					{
						TextureManager::Instance()->SetMipMapsTextures(material->texturID,material->mipmap1,material->mipmap2,material->mipmap3);

					}else
					{
						TextureManager::Instance()->SetTexture(material->texturID,GU_NEAREST,GU_NEAREST);
					}
				}

				if (mesh->mMaterial != -1 && model->mMaterials[mesh->mMaterial]->lightmapping)
				{
					ObjMaterial *material = model->mMaterials[mesh->mMaterial];
					TextureManager::Instance()->SetTexture(material->lightMapID);
				}

				if(mesh->triangles)

					sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,mesh->vertexCount,0,mesh->meshVertices);
				else
					sceGumDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D,mesh->indicesCount,0,mesh->meshVertices);
			}

			sceGuDisable(GU_TEXTURE_2D);
		}

		void RenderManager::SetActiveCamera(Camera *camera)
		{
			mCam = camera;
			mCam->needUpdate = true;
		}

		void RenderManager::UpdateFrustumMatrix()
		{
			// view is relative to worldOrigin; culling works on world-space boxes, so
			// fold the origin shift back in for the frustum planes.
			ScePspFMatrix4 shiftMatrix;
			ScePspFMatrix4 worldView;
			ScePspFVector3 shift = {-worldOrigin.x, -worldOrigin.y, -worldOrigin.z};
			gumLoadIdentity(&shiftMatrix);
			gumTranslate(&shiftMatrix, &shift);
			gumMultMatrix(&worldView, &view, &shiftMatrix);
			gumMultMatrix(&projection_view_matrix, &proj, &worldView);
		}

		void RenderManager::SetClearColour(unsigned int color)
		{
			cleanColor = color;
		}

		void RenderManager::TakeNextScreenshot()
		{
			char name[20];
			//sprintf(name,"Screenshots/screen%d.png",screenNumber);
			sprintf(name,"ms0:/PICTURE/screen%d.png",screenNumber);
			TakeScreenshot(name);
			screenNumber++;
		}

		void RenderManager::TakeScreenshot(const char* filename)
		{
			u32* vram32;
			u16* vram16;
			int bufferwidth;
			int pixelformat;
			int unknown = 0;
			int i, x, y;
			png_structp png_ptr;
			png_infop info_ptr;
			FILE* fp;
			u8* line;

			// The frame being captured may still be in flight.
			FinishPendingFrame();

			fp = fopen(filename, "wb");
			if (!fp) return;
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!png_ptr) return;
			info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr)
			{
					png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
					fclose(fp);
					return;
			}
			png_init_io(png_ptr, fp);
			png_set_IHDR(png_ptr, info_ptr, 480, 272,8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			png_write_info(png_ptr, info_ptr);
			line = (u8*) malloc(480 * 3);
			sceDisplayWaitVblankStart();  // if framebuf was set with PSP_DISPLAY_SETBUF_NEXTFRAME, wait until it is changed
			sceDisplayGetFrameBuf((void**)&vram32, &bufferwidth, &pixelformat, unknown);
			vram16 = (u16*) vram32;
			for (y = 0; y < 272; y++)
			{
					for (i = 0, x = 0; x < 480; x++)
					{
							u32 color = 0;
							u8 r = 0, g = 0, b = 0;
							switch (pixelformat)
							{
									case PSP_DISPLAY_PIXEL_FORMAT_565:
											color = vram16[x + y * bufferwidth];
											r = (color & 0x1f) << 3;
											g = ((color >> 5) & 0x3f) << 2 ;
											b = ((color >> 11) & 0x1f) << 3 ;
											break;
									case PSP_DISPLAY_PIXEL_FORMAT_5551:
											color = vram16[x + y * bufferwidth];
											r = (color & 0x1f) << 3;
											g = ((color >> 5) & 0x1f) << 3 ;
											b = ((color >> 10) & 0x1f) << 3 ;
											break;
									case PSP_DISPLAY_PIXEL_FORMAT_4444:
											color = vram16[x + y * bufferwidth];
											r = (color & 0xf) << 4;
											g = ((color >> 4) & 0xf) << 4 ;
											b = ((color >> 8) & 0xf) << 4 ;
											break;
									case PSP_DISPLAY_PIXEL_FORMAT_8888:
											color = vram32[x + y * bufferwidth];
											r = color & 0xff;
											g = (color >> 8) & 0xff;
											b = (color >> 16) & 0xff;
											break;
							}
							line[i++] = r;
							line[i++] = g;
							line[i++] = b;
					}
					png_write_row(png_ptr, line);
			}
			free(line);
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
			fclose(fp);
		}

		RenderManager RenderManager::m_RenderManager;

		RenderManager * RenderManager::InstancePtr()
		{
			return &m_RenderManager;
		}
	}
}
