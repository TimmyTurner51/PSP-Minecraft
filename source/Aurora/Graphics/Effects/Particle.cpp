#include <Aurora/Graphics/Effects/Particle.h>
#include <Aurora/Graphics/TextureManager.h>
#include <Aurora/Graphics/RenderManager.h>
#include <string.h>
#include <malloc.h>
#include <psputils.h>

#define PI 3.1415926535897f
#define DEG_TO_RAD (PI / 180.0f)

namespace
{
	const int kParticlePool = 64;
	Aurora::Graphics::CraftPSPVertex* sParticleVerts = NULL;
	unsigned char sParticleSlot[kParticlePool];
	int sTerrainTex = -1;
	int sItemsTex = -1;
	int sSmokeTex = -1;
	int sNoteTex = -1;
	int sExplosionTex = -1;
	int sFlameTex = -1;
	int sBubbleTex = -1;
	int sHeartTex = -1;

	Aurora::Graphics::Particle sParticlePool[kParticlePool];

	void EnsureParticleVerts()
	{
		if(sParticleVerts == NULL)
			sParticleVerts = (Aurora::Graphics::CraftPSPVertex*)memalign(16, kParticlePool * 4 * sizeof(Aurora::Graphics::CraftPSPVertex));
	}

	Aurora::Graphics::CraftPSPVertex* TakeParticleVerts(int& slot)
	{
		slot = -1;
		if(sParticleVerts == NULL)
			return NULL;
		for(int i = 0; i < kParticlePool; i++)
		{
			if(sParticleSlot[i] == 0)
			{
				sParticleSlot[i] = 1;
				slot = i;
				return &sParticleVerts[i * 4];
			}
		}
		return NULL;
	}

	void ReleaseParticleVerts(int slot)
	{
		if(slot >= 0 && slot < kParticlePool)
			sParticleSlot[slot] = 0;
	}

	struct CrackBit
	{
		float x, y, z;
		float vx, vy, vz;
		float life;
		float scale;
		float u0, v0, u1, v1;
	};
	CrackBit sBits[16];
	int sBitCount = 0;
	Aurora::Graphics::CraftPSPVertex* sCrackDrawVerts = NULL;
	Aurora::Math::Vector3 sCamRight(1,0,0);
	Aurora::Math::Vector3 sCamUp(0,1,0);
	// Tile of the block being broken, resolved when the break is queued. Ids above 511 (the extended
	// block range) used to fall off the end of a 512-entry cache and drew dirt instead.
	int sPendTexX = 2;
	int sPendTexY = 0;
	float sPendX = 0.0f;
	float sPendY = 0.0f;
	float sPendZ = 0.0f;
	int sPendN = 0;

	void EmitPendingCracks()
	{
		int n = sPendN;
		sPendN = 0;
		if(n <= 0)
			return;
		if(n > 9)
			n = 9;

		int tex_x = sPendTexX;
		int tex_y = sPendTexY;

		float percent = 1.0f / 16.0f;
		float pix = 1.0f / 256.0f;
		for(int k = 0; k < n && sBitCount < 16; k++)
		{
			CrackBit& b = sBits[sBitCount];
			b.x = sPendX + (-0.3f + (rand() % 7) * 0.1f);
			b.y = sPendY + (-0.2f + (rand() % 5) * 0.1f);
			b.z = sPendZ + (-0.3f + (rand() % 7) * 0.1f);
			b.vx = (-15 + rand() % 30) * 0.1f;
			b.vy = 2.5f;
			b.vz = (-15 + rand() % 30) * 0.1f;
			b.life = 0.6f + (rand() % 7) * 0.1f;
			b.scale = 0.05f + (rand() % 7) * 0.01f;
			b.u0 = tex_x * percent + (rand() % 12) * pix + pix * 0.5f;
			b.v0 = tex_y * percent + (rand() % 12) * pix + pix * 0.5f;
			b.u1 = b.u0 + pix * 4.0f;
			b.v1 = b.v0 + pix * 4.0f;
			sBitCount++;
		}
	}

	int ParseParticleKind(const char* name)
	{
		if(name == NULL)
			return Aurora::Graphics::Particle::None;
		if(strcmp(name, "smoke") == 0)
			return Aurora::Graphics::Particle::Smoke;
		if(strcmp(name, "black smoke") == 0)
			return Aurora::Graphics::Particle::BlackSmoke;
		if(strcmp(name, "note") == 0)
			return Aurora::Graphics::Particle::Note;
		if(strcmp(name, "explosion") == 0)
			return Aurora::Graphics::Particle::Explosion;
		if(strcmp(name, "flame") == 0)
			return Aurora::Graphics::Particle::Flame;
		if(strcmp(name, "bubble") == 0)
			return Aurora::Graphics::Particle::Bubble;
		if(strcmp(name, "block crack") == 0)
			return Aurora::Graphics::Particle::BlockCrack;
		if(strcmp(name, "item crack") == 0)
			return Aurora::Graphics::Particle::ItemCrack;
		if(strcmp(name, "heart") == 0)
			return Aurora::Graphics::Particle::Heart;
		return Aurora::Graphics::Particle::None;
	}
}

namespace Aurora
{
	namespace Graphics
	{
		Particle::Particle()
		{
		    particleVertices = NULL;
		    particleSlot = -1;
		    texture = -1;
			kind = None;
			inUse = false;
			toDestroy = true;
			vertsBuilt = false;
		}

		Particle::~Particle()
		{
		}

		Particle* Particle::Spawn(CraftWorld* world, const char* name, Vector3 position)
		{
			EnsureParticleVerts();

			int slot = -1;
			for(int i = 0; i < kParticlePool; i++)
			{
				if(sParticlePool[i].inUse == false)
				{
					slot = i;
					break;
				}
			}

			// The pool is a hard cap -- UpdateAll and RenderAll walk all 64 slots every
			// frame. This used to return NULL once every slot was busy, and every caller
			// dereferences the result immediately, which is the "Write Word at 00000010"
			// crash: Particle::SetVelocity writes `velocity`, and velocity sits at offset
			// 0x10, so a NULL Particle reads as address 0x10 exactly.
			//
			// Nothing holds a Particle* across frames (StatePlay's mParticles vector was
			// only ever pushed to and never read), so instead of returning a pointer the
			// caller will crash on, evict the particle closest to its natural end and give
			// the slot to the newcomer. The pool stays a cap rather than a cliff.
			if(slot < 0)
			{
				for(int i = 0; i < kParticlePool; i++)
				{
					if(sParticlePool[i].toDestroy)
					{
						slot = i;   // already on its way out, so nothing is interrupted
						break;
					}
				}

				if(slot < 0)
				{
					slot = 0;
					for(int i = 1; i < kParticlePool; i++)
					{
						// timeToLive counts down, so the smallest value is nearly expired
						if(sParticlePool[i].timeToLive < sParticlePool[slot].timeToLive)
							slot = i;
					}
				}

				Release(&sParticlePool[slot]);
			}

			sParticlePool[slot].Init(world, ParseParticleKind(name), position);
			sParticlePool[slot].inUse = true;
			return &sParticlePool[slot];
		}

		void Particle::QueueBlockCrack(CraftWorld* world, Vector3 position, int blockId)
		{
			// Resolve the tile now: the world knows every block type, including the extended ids a
			// fixed-size table could not hold.
			sPendTexX = 2;   // dirt, the fallback for an id the world does not have
			sPendTexY = 0;
			if(world != NULL && blockId >= 0 && blockId < (int)world->blockTypes.size())
			{
				sPendTexX = world->blockTypes[blockId].downPlane_x;
				sPendTexY = world->blockTypes[blockId].downPlane_y;
				if(world->mainOptions.fastRendering == true &&
				   (blockId == LeavesBlock::getID() || blockId == BirchLeaves::getID() || blockId == SpruceLeaves::getID()))
				{
					sPendTexX += 1;
				}
			}
			sPendX = (float)((int)position.x) + 0.5f;
			sPendY = (float)((int)position.y) + 0.5f;
			sPendZ = (float)((int)position.z) + 0.5f;
			sPendN = 9;
		}

		void Particle::Release(Particle* p)
		{
			if(p == NULL)
				return;
			ReleaseParticleVerts(p->particleSlot);
			p->particleVertices = NULL;
			p->particleSlot = -1;
			p->inUse = false;
			p->toDestroy = true;
			p->vertsBuilt = false;
		}

		int Particle::ActiveCount()
		{
			int count = sBitCount;
			for(int i = 0; i < kParticlePool; i++)
			{
				if(sParticlePool[i].inUse)
					count++;
			}
			return count;
		}

		void Particle::PrecacheTextures(CraftWorld* world)
		{
			(void)world;
			EnsureParticleVerts();
			sPendN = 0;
			sBitCount = 0;
			for(int i = 0; i < kParticlePool; i++)
			{
				sParticlePool[i].inUse = false;
				sParticlePool[i].toDestroy = true;
				sParticlePool[i].vertsBuilt = false;
				sParticlePool[i].particleVertices = NULL;
				sParticlePool[i].particleSlot = -1;
				sParticlePool[i].texture = -1;
			}
			if(sTerrainTex < 0)
				sTerrainTex = TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1);
			if(sItemsTex < 0)
				sItemsTex = TextureHelper::Instance()->GetTexture(TextureHelper::Items1);
			if(sSmokeTex < 0)
				sSmokeTex = TextureHelper::Instance()->GetTexture(TextureHelper::smokeTexture);
			if(sNoteTex < 0)
				sNoteTex = TextureHelper::Instance()->GetTexture(TextureHelper::noteTexture);
			if(sExplosionTex < 0)
				sExplosionTex = TextureHelper::Instance()->GetTexture(TextureHelper::explosionTexture);
			if(sFlameTex < 0)
				sFlameTex = TextureHelper::Instance()->GetTexture(TextureHelper::flameTexture);
			if(sBubbleTex < 0)
				sBubbleTex = TextureHelper::Instance()->GetTexture(TextureHelper::bubbleTexture);
			if(sHeartTex < 0)
				sHeartTex = TextureHelper::Instance()->GetTexture(TextureHelper::heartTexture);
		}

		void Particle::UpdateAll(CraftWorld* world, float dt, Vector3 camRight, Vector3 camUp)
		{
			sCamRight = camRight;
			sCamUp = camUp;
			EmitPendingCracks();
			for(int i = 0; i < sBitCount; )
			{
				CrackBit& b = sBits[i];
				b.life -= dt;
				b.x += b.vx * dt;
				b.y += b.vy * dt;
				b.z += b.vz * dt;
				b.vy -= 8.8f * dt;
				if(b.life <= 0.0f)
				{
					sBits[i] = sBits[sBitCount - 1];
					sBitCount--;
					continue;
				}
				i++;
			}

			for(int i = 0; i < kParticlePool; i++)
			{
				if(sParticlePool[i].inUse)
				{
					sParticlePool[i].Update(world, dt, camRight, camUp);
					if(sParticlePool[i].toDestroy)
						Release(&sParticlePool[i]);
				}
			}
		}

		void Particle::RenderAll(Frustum &camFrustum)
		{
			if(sBitCount > 0 && sTerrainTex >= 0)
			{
				TextureManager* tm = TextureManager::Instance();
				if(sTerrainTex < (int)tm->Images.size() && tm->Images[sTerrainTex] != NULL)
				{
					if(sCrackDrawVerts == NULL)
						sCrackDrawVerts = (CraftPSPVertex*)memalign(16, 16 * 4 * sizeof(CraftPSPVertex));
					if(sCrackDrawVerts != NULL)
					{
						int n = sBitCount;
						if(n > 16)
							n = 16;
						for(int i = 0; i < n; i++)
						{
							CrackBit& b = sBits[i];
							CraftPSPVertex* v = &sCrackDrawVerts[i * 4];
							unsigned int c = 0xFFFFFFFF;
							float rx = sCamRight.x * b.scale;
							float ry = sCamRight.y * b.scale;
							float rz = sCamRight.z * b.scale;
							float ux = sCamUp.x * b.scale;
							float uy = sCamUp.y * b.scale;
							float uz = sCamUp.z * b.scale;
							v[0].u = b.u0; v[0].v = b.v0; v[0].color = c;
							v[0].x = b.x - rx + ux; v[0].y = b.y - ry + uy; v[0].z = b.z - rz + uz;
							v[1].u = b.u0; v[1].v = b.v1; v[1].color = c;
							v[1].x = b.x - rx - ux; v[1].y = b.y - ry - uy; v[1].z = b.z - rz - uz;
							v[2].u = b.u1; v[2].v = b.v0; v[2].color = c;
							v[2].x = b.x + rx + ux; v[2].y = b.y + ry + uy; v[2].z = b.z + rz + uz;
							v[3].u = b.u1; v[3].v = b.v1; v[3].color = c;
							v[3].x = b.x + rx - ux; v[3].y = b.y + ry - uy; v[3].z = b.z + rz - uz;
						}
						sceKernelDcacheWritebackRange(sCrackDrawVerts, n * 4 * sizeof(CraftPSPVertex));
						tm->currentTexture = -1;
						tm->SetTextureModeulate(sTerrainTex);
						sceGuEnable(GU_TEXTURE_2D);
						sceGuEnable(GU_BLEND);
						sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
						sceGumMatrixMode(GU_MODEL);
						sceGumPushMatrix();
						RenderManager::InstancePtr()->LoadWorldModelMatrix();
						for(int i = 0; i < n; i++)
							sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, &sCrackDrawVerts[i * 4]);
						sceGumPopMatrix();
					}
				}
			}
			// Force one real bind for the first particle (earlier passes may have set
			// textures behind the cache's back); after that let the cache skip repeats.
			TextureManager::Instance()->currentTexture = -1;
			for(int i = 0; i < kParticlePool; i++)
			{
				if(sParticlePool[i].inUse)
					sParticlePool[i].Render(camFrustum);
			}
		}

		void Particle::Init(CraftWorld* world, int particleKind, Vector3 _position)
		{
		    ReleaseParticleVerts(particleSlot);
		    particleVertices = NULL;
		    particleSlot = -1;
		    texture = -1;
			kind = particleKind;

			position = _position;

			horizontalScale = 0.3f;
			verticalScale = 0.3f;

			gravity = 0.9;
			maxTimeToLive = 0.0f;

			color = Vector3(1,1,1);

            SetMaxLiveTime(0.0f);
			SetLiveTime(0.0f);
			SetScale(0.3f);
			SetGravity(-0.3f);
			SetFramesAmount(0);
			SetBrightness(1.0f);
			frame = 0;
			totalFrames = 0;


            var1 = -1;
            var2 = -1;
            var3 = -1;
            varf1 = -1.0f;
            varf2 = -1.0f;

			toDestroy = false;
			rederMe = true;
			friction = 0.0f;

			myLight = 1.0f;
			billboardRight = Vector3(1,0,0);
			billboardUp = Vector3(0,1,0);
			vertsBuilt = false;

			if(kind == Smoke)
            {
                texture = sSmokeTex;

                SetMaxLiveTime(3000);
                switch(rand() % 8)
                {
                    case 0 : SetLiveTime(1500,2000); break;
                    case 1 : SetLiveTime(2500,3000); break;
                    case 2 : SetLiveTime(2500,3000); break;
                    case 3 : SetLiveTime(3000,3500); break;
                    case 4 : SetLiveTime(3000,3500); break;
                    default: SetLiveTime(2000,2500); break;
                }

                SetScale(0.2f,0.4f);
                SetGravity(0.0f);
                SetFramesAmount(8);
                SetBrightness(0.8f, 1.01f);

                friction = 2.0f;

                float veloc_x_shift = 0.0f;
                float veloc_z_shift = 0.0f;

                switch(rand() % 4)
                {
                    case 0: veloc_x_shift = -1.2f; veloc_z_shift = -1.2f; break;
                    case 1: veloc_x_shift = 1.2f; veloc_z_shift = -1.2f; break;
                    case 2: veloc_x_shift = -1.2f; veloc_z_shift = 1.2f; break;
                    case 3: veloc_x_shift = 1.2f; veloc_z_shift = 1.2f; break;
                }

                velocity = Vector3(veloc_x_shift,0.0f,veloc_z_shift);
            }

			if(kind == BlackSmoke)
            {
                texture = sSmokeTex;

                SetMaxLiveTime(900);
                SetLiveTime(600,900);

                SetScale(0.10f,0.18f);
                SetGravity(0.0f);
                SetFramesAmount(8);
                SetBrightness(0.2f, 0.3f);

                friction = 2.0f;
                varf2 = (rand()%6)/5.0f;

                velocity = Vector3(0,0.0f,0);
            }
            if(kind == Note)
            {
                texture = sNoteTex;

                SetMaxLiveTime(370);
                SetLiveTime(370);

                SetScale(0.19f,0.26f);
                SetGravity(0.0f);
                friction = 4.0f;
                myLight = 1.0f;

                velocity = Vector3(0.0f,3.0f,0.0f);

                totalFrames = 1;
            }
			if(kind == Explosion)
            {
                texture = sExplosionTex;

                SetMaxLiveTime(600);
                SetLiveTime(500,600);

                SetScale(1.1f,1.3f);
                SetGravity(0.0f);
                SetFramesAmount(16);
                SetBrightness(0.8f, 1.01f);

                velocity = Vector3(0,0.0f,0);
            }
            if(kind == Flame)
            {
                texture = sFlameTex;

                SetMaxLiveTime(900);
                SetLiveTime(700,900);

                SetScale(0.13f,0.18f);
                SetGravity(0.0f);
                SetBrightness(1.0f);

                velocity = Vector3(0,0.0f,0);

                totalFrames = 1;
            }
            if(kind == Bubble)
            {
                texture = sBubbleTex;
                timeToLive = 0.6f + (rand() % 6) / 10.0f;
                gravity = 0.1f;
                totalFrames = 1;
            }
            if(kind == Heart)
            {
                // Java EntityHeartFX: 16 ticks, drifts up, slight drag.
                texture = sHeartTex;

                SetMaxLiveTime(800);
                SetLiveTime(800);

                SetScale(0.14f,0.18f);
                SetGravity(0.0f);
                SetBrightness(1.0f);
                friction = 1.5f;

                velocity = Vector3((-10+rand()%21)*0.02f,1.1f,(-10+rand()%21)*0.02f);

                totalFrames = 1;
            }
            if(kind == BlockCrack)
            {
                texture = sTerrainTex;

                SetLiveTime(600,1200);
                SetScale(0.05f, 0.11f);
                SetFramesAmount(1);
                SetBrightness(1.0f);
                SetGravity(-6.8f);

                friction = 0.0f;

                velocity = Vector3((-15+rand()%30)*0.1f,2.5f,(-15+rand()%30)*0.1f);
            }

            if(kind == ItemCrack)
            {
                texture = sItemsTex;

                SetLiveTime(600,1200);
                SetScale(0.04f, 0.09f);
                SetFramesAmount(1);
                SetBrightness(1.0f);
                SetGravity(-6.8f);

                friction = 0.0f;

                velocity = Vector3((-15+rand()%30)*0.1f,2.5f,(-15+rand()%30)*0.1f);
            }


            if(world->TaxicabDistance2d(position.x,position.y,world->playerPos.x,world->playerPos.y) >= 12.0f)
            {
                toDestroy = true;
            }

            if(toDestroy == false)
            {
                particleVertices = TakeParticleVerts(particleSlot);
                if(particleVertices == NULL)
                    toDestroy = true;
            }
            bBox =  BoundingBox(Vector3(position.x-0.33f,position.y-0.33f,position.z-0.33f),Vector3(position.x+0.33f,position.y+0.33f,position.z+0.33f));
		}

		void Particle::SetVelocity(Vector3 _velocity)
		{
		    velocity = _velocity;
		}

        void Particle::SetScale(float scale)
		{
		    horizontalScale = scale;
		    verticalScale = scale;
		}

        void Particle::SetScale(float minScale, float maxScale)
		{
		    int range = (int)((maxScale-minScale)*100.0f);
		    if(range < 1)
		        range = 1;
		    float scale = (minScale*100.0f + rand() % range) / 100.0f;
		    horizontalScale = scale;
		    verticalScale = scale;
		}

        void Particle::SetColor(Vector3 _color)
		{
		    color = _color;
		}

        void Particle::SetBrightness(float brightness)
		{
		    color = Vector3(brightness, brightness, brightness);
		}

        void Particle::SetBrightness(float minBrightness, float maxBrightness)
		{
		    int range = (int)((maxBrightness-minBrightness)*100.0f);
		    if(range < 1)
		        range = 1;
		    float brightness = (minBrightness*100.0f + rand() % range) / 100.0f;
		    color = Vector3(brightness, brightness, brightness);
		}

        void Particle::SetMaxLiveTime(int _maxLiveTime)
		{
		    maxTimeToLive = _maxLiveTime / 1000.0f;
		}

        void Particle::SetLiveTime(int _liveTime)
		{
		    timeToLive = _liveTime / 1000.0f;
		}

        void Particle::SetLiveTime(int minLiveTime, int maxLiveTime)
		{
		    int range = (maxLiveTime-minLiveTime)/10;
		    if(range < 1)
		        range = 1;
		    int _liveTime = minLiveTime/10 + rand() % range;
		    timeToLive = _liveTime / 100.0f;
		}

		void Particle::SetFramesAmount(int _totalFrames)
		{
		    frame = 0;
		    totalFrames = _totalFrames;
		}

        void Particle::SetGravity(float _gravity)
		{
            gravity = _gravity;
		}

		void Particle::BuildVerticies()
		{
		    if(particleVertices == NULL)
                return;

		    float frameSize = (totalFrames > 0) ? 1.0f/(totalFrames) : 1.0f;

            ScePspFVector3 up, right;

			right.x = billboardRight.x * horizontalScale;
			right.y = billboardRight.y * horizontalScale;
			right.z = billboardRight.z * horizontalScale;
			up.x = billboardUp.x * verticalScale;
			up.y = billboardUp.y * verticalScale;
			up.z = billboardUp.z * verticalScale;

			int i = 0;

			particleVertices[i].x = position.x - right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z + up.z;// * stepScale;
			particleVertices[i].u = frame*frameSize;// * textureScale;
			particleVertices[i].v = 0.f;// * textureScale;
			particleVertices[i].color = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);
			i++;

			// (x, y - 1, z)
			particleVertices[i].x = position.x - right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y - up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z - up.z;// * stepScale;
			particleVertices[i].u = frame*frameSize;// * textureScale;
			particleVertices[i].v = 1.f;// * textureScale;
			particleVertices[i].color = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);
			i++;

			// (x + 1, y, z)
			particleVertices[i].x = position.x + right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z + right.z + up.z;// * stepScale;
			particleVertices[i].u = frame*frameSize+frameSize;// * textureScale;
			particleVertices[i].v = 0.f;// * textureScale;
			particleVertices[i].color = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);
			i++;

			// (x + 1, y - 1, z)
			particleVertices[i].x = position.x + right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y - up.y;//* stepScale;
			particleVertices[i].z = position.z + right.z - up.z;// * stepScale;
			particleVertices[i].u = frame*frameSize+frameSize;// * textureScale;
			particleVertices[i].v = 1.f;// * textureScale;
			particleVertices[i].color = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);

            vertsBuilt = true;
		}

		void Particle::BuildVerticiesForBlockCrack(CraftWorld* crtf, int blockId)
		{
		    if(particleVertices == NULL || crtf == NULL || blockId < 0 || blockId >= (int)crtf->blockTypes.size())
                return;

		    float texturePixel = (float)1.0f/256.0f;
            float percent = (float)1.0f/16.0f;

            int tex_x, tex_y;
            if(crtf->mainOptions.fastRendering == true && (blockId == LeavesBlock::getID() || blockId == BirchLeaves::getID() || blockId == SpruceLeaves::getID()))
            {
                tex_x = crtf->blockTypes[blockId].downPlane_x+1;
                tex_y = crtf->blockTypes[blockId].downPlane_y;
            }
            else
            {
                tex_x = crtf->blockTypes[blockId].downPlane_x;
                tex_y = crtf->blockTypes[blockId].downPlane_y;
            }

            if(varf1 == -1.0f)
            {
                varf1 = tex_y * percent + (rand() % 12)*texturePixel + texturePixel*0.5f;
                varf2 = tex_x * percent + (rand() % 12)*texturePixel + texturePixel*0.5f;
            }
            float texCordY = varf1;
            float texScaleY = texturePixel*4 + texturePixel*0.5f;

            float texCordX = varf2;
            float texScaleX = texturePixel*4 + texturePixel*0.5f;

            ScePspFVector3 up, right;

			right.x = billboardRight.x * horizontalScale;
			right.y = billboardRight.y * horizontalScale;
			right.z = billboardRight.z * horizontalScale;
			up.x = billboardUp.x * verticalScale;
			up.y = billboardUp.y * verticalScale;
			up.z = billboardUp.z * verticalScale;

			int i = 0;

			particleVertices[i].x = position.x - right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z + up.z;// * stepScale;
            particleVertices[i].u = texCordX;// * textureScale;
			particleVertices[i].v = texCordY;// * textureScale;
			i++;

			// (x, y - 1, z)
			particleVertices[i].x = position.x - right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y - up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z - up.z;// * stepScale;
			particleVertices[i].u = texCordX;// * textureScale;
			particleVertices[i].v = texCordY+texScaleY;// * textureScale;
			i++;

			// (x + 1, y, z)
			particleVertices[i].x = position.x + right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z + right.z + up.z;// * stepScale;
			particleVertices[i].u = texCordX+texScaleX;// * textureScale;
			particleVertices[i].v = texCordY;// * textureScale;
			i++;

			// (x + 1, y - 1, z)
			particleVertices[i].x = position.x + right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y - up.y;//* stepScale;
			particleVertices[i].z = position.z + right.z - up.z;// * stepScale;
			particleVertices[i].u = texCordX+texScaleX;// * textureScale;
			particleVertices[i].v = texCordY+texScaleY;// * textureScale;

            {
                unsigned int col = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);
                particleVertices[0].color = col;
                particleVertices[1].color = col;
                particleVertices[2].color = col;
                particleVertices[3].color = col;
            }
            vertsBuilt = true;
		}

		void Particle::BuildVerticiesForItemCrack(CraftWorld* crtf, int itemId)
		{
		    if(particleVertices == NULL || crtf == NULL || itemId < 0 || itemId >= (int)crtf->itemTypes.size())
                return;

		    float texturePixel = (float)1.0f/256.0f;
            float percent = (float)1.0f/16.0f;

            if(varf1 == -1.0f)
            {
                varf1 = (1.0f) - (crtf->itemTypes[itemId].textureRow * percent + percent) + ((rand() % 12)*texturePixel + texturePixel*0.5f);
                varf2 = crtf->itemTypes[itemId].plane * percent + (rand() % 12)*texturePixel + texturePixel*0.5f;
            }
            float texCordY = varf1;
            float texScaleY = texturePixel*4 + texturePixel*0.5f;

            float texCordX = varf2;
            float texScaleX = texturePixel*4 + texturePixel*0.5f;

            ScePspFVector3 up, right;

			right.x = billboardRight.x * horizontalScale;
			right.y = billboardRight.y * horizontalScale;
			right.z = billboardRight.z * horizontalScale;
			up.x = billboardUp.x * verticalScale;
			up.y = billboardUp.y * verticalScale;
			up.z = billboardUp.z * verticalScale;

			int i = 0;

			particleVertices[i].x = position.x - right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z + up.z;// * stepScale;
            particleVertices[i].u = texCordX;// * textureScale;
			particleVertices[i].v = texCordY;// * textureScale;
			i++;

			// (x, y - 1, z)
			particleVertices[i].x = position.x - right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y - right.y - up.y;// * stepScale;
			particleVertices[i].z = position.z - right.z - up.z;// * stepScale;
			particleVertices[i].u = texCordX;// * textureScale;
			particleVertices[i].v = texCordY+texScaleY;// * textureScale;
			i++;

			// (x + 1, y, z)
			particleVertices[i].x = position.x + right.x + up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y + up.y;// * stepScale;
			particleVertices[i].z = position.z + right.z + up.z;// * stepScale;
			particleVertices[i].u = texCordX+texScaleX;// * textureScale;
			particleVertices[i].v = texCordY;// * textureScale;
			i++;

			// (x + 1, y - 1, z)
			particleVertices[i].x = position.x + right.x - up.x;// * stepScale;
			particleVertices[i].y = position.y + right.y - up.y;//* stepScale;
			particleVertices[i].z = position.z + right.z - up.z;// * stepScale;
			particleVertices[i].u = texCordX+texScaleX;// * textureScale;
			particleVertices[i].v = texCordY+texScaleY;// * textureScale;

            {
                unsigned int col = GU_COLOR(color.x*myLight,color.y*myLight,color.z*myLight,1.0f);
                particleVertices[0].color = col;
                particleVertices[1].color = col;
                particleVertices[2].color = col;
                particleVertices[3].color = col;
            }
            vertsBuilt = true;
		}

		void Particle::Update(CraftWorld* crtf, float dt, Vector3 camRight, Vector3 camUp)
		{
            billboardRight = camRight;
            billboardUp = camUp;

            if(position.x < 0 || position.x >= crtf->worldLimit || position.y < 0 || position.y >= crtf->WORLD_HEIGHT || position.z < 0 || position.z >= crtf->worldLimit)
            {
                toDestroy = true;
            }

            if(timeToLive <= 0.0f)
            {
                toDestroy = true;
            }

		    if(toDestroy == true)
            {
                return;
            }

            bBox =  BoundingBox(Vector3(position.x-0.33f,position.y-0.33f,position.z-0.33f),Vector3(position.x+0.33f,position.y+0.33f,position.z+0.33f));

		    myLight = 1.0f;
		    timeToLive -= dt;

            position += velocity*dt;
           // velocity.y += gravity*dt;

            if(crtf->TaxicabDistance2d(position.x,position.y,crtf->playerPos.x,crtf->playerPos.y) < 12.0f)
            {
                rederMe = true;
            }
            else
            {
                rederMe = false;
            }

            if(kind == Smoke)
            {
                if(CheckCollision(crtf) == true)
                {
                    velocity.y = 0.0f;
                }
                BuildVerticies();

                frame = (timeToLive/maxTimeToLive)*8;
                if(frame > 7)
                {
                    frame = 7;
                }

                position.y += 0.2*dt;
                velocity.x -= velocity.x*dt*friction;
                velocity.y -= velocity.y*dt*friction;
                velocity.z -= velocity.z*dt*friction;

                if(abs(velocity.x) < 0.01f && abs(velocity.y) < 0.01f && abs(velocity.z) < 0.01f)
                {
                    velocity = Vector3(0,0,0);
                }
            }

            if(kind == BlackSmoke)
            {
                BuildVerticies();

                frame = (timeToLive/maxTimeToLive)*8;
                if(frame > 7)
                {
                    frame = 7;
                }

                position.y += (0.2+varf2)*dt;
                velocity.x -= velocity.x*dt*friction;
                velocity.y -= velocity.y*dt*friction;
                velocity.z -= velocity.z*dt*friction;

                if(abs(velocity.x) < 0.01f && abs(velocity.y) < 0.01f && abs(velocity.z) < 0.01f)
                {
                    velocity = Vector3(0,0,0);
                }
            }

            if(kind == Flame)
            {
                frame = 0;
                BuildVerticies();

                horizontalScale -= dt*0.09f;
                verticalScale = horizontalScale;
            }

            if(kind == Explosion)
            {
                BuildVerticies();

                frame = (timeToLive/maxTimeToLive)*16;
                if(frame > 15)
                {
                    frame = 15;
                }
            }

            if(kind == Heart)
            {
                frame = 0;
                BuildVerticies();

                velocity.x -= velocity.x*dt*friction;
                velocity.y -= velocity.y*dt*friction;
                velocity.z -= velocity.z*dt*friction;
            }

            if(kind == Note)
            {
                BuildVerticies();

                velocity.y -= velocity.y*dt*friction;

                if(abs(velocity.y) < 0.01f)
                {
                    velocity = Vector3(0,0,0);
                }
            }

            if(kind == BlockCrack || kind == ItemCrack)
            {
                if(CheckCollision(crtf) == true)
                {
                    velocity.y = 0.0f;
                }
                if(kind == BlockCrack)
                {
                    BuildVerticiesForBlockCrack(crtf,var1);
                }

                if(kind == ItemCrack)
                {
                    BuildVerticiesForItemCrack(crtf,var1);
                }

                if(crtf->BlockSolid(position.x,position.y-0.2f,position.z) == false)
                {
                    velocity.y += (gravity*1.3 * dt)*1.6f;
                }
                else
                {
                    position.y = ((int)position.y)+0.15f;
                    velocity.y = 0.0f;

                    velocity.x *= 0.8f;
                    velocity.z *= 0.8f;
                }

                if(abs(velocity.x) < 0.1f)
                {
                    velocity.x = 0.0f;
                }
                if(abs(velocity.z) < 0.1f)
                {
                    velocity.z = 0.0f;
                }

                if(crtf->BlockSolid(position.x,position.y+0.15f,position.z) == true)
                {
                    velocity.y = 0;
                    position.y -= 0.1f;
                }

                if(crtf->BlockSolid(position.x+velocity.x*dt*2,position.y,position.z+velocity.z*dt*2) == true)
                {
                    velocity.x *= -0.9f;
                    velocity.z *= -0.9f;
                }
            }
		}

		bool Particle::CheckCollision(CraftWorld* crtf)
		{
            if (crtf->BlockSolid(position.x,position.y+horizontalScale*1.5f,position.z) != 0)
            {
                return 1;
            }
            else
            {
                return 0;
            }
		}

		void Particle::Render(Frustum &camFrustum)
		{
		    if(rederMe == false || toDestroy == true || vertsBuilt == false || particleVertices == NULL || texture < 0)
                return;

			TextureManager* tm = TextureManager::Instance();
			if(texture >= (int)tm->Images.size() || tm->Images[texture] == NULL)
				return;

			CraftPSPVertex* v = (CraftPSPVertex*)sceGuGetMemory(4 * sizeof(CraftPSPVertex));
			if(v == NULL)
				return;
			memcpy(v, particleVertices, 4 * sizeof(CraftPSPVertex));

			tm->SetTextureModeulate(texture);

			sceGuEnable(GU_TEXTURE_2D);
			sceGuEnable(GU_BLEND);
			sceGumMatrixMode(GU_MODEL);
			sceGumPushMatrix();
			RenderManager::InstancePtr()->LoadWorldModelMatrix();
			sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
			sceGumPopMatrix();
		}
	}
}

