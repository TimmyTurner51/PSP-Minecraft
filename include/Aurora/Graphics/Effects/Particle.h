#ifndef PARTICLE_H_
#define PARTICLE_H_

#include <Aurora/Math/Vector3.h>
#include <Aurora/Graphics/Vertex.h>
#include <pspmath.h>
#include <Scripts/CraftWorld2.h>
#include <Scripts/TextureHelper.h>
#include <math.h>
#include <psptypes.h>

namespace Aurora
{
	namespace Graphics
	{
		class Particle
		{
		public:
			enum Kind
			{
				None = 0,
				Smoke,
				BlackSmoke,
				Note,
				Explosion,
				Flame,
				Bubble,
				BlockCrack,
				ItemCrack,
				Heart
			};

			Particle();
			~Particle();

			// Never returns NULL: if the pool is full the particle closest to expiring is
			// evicted to make room. The old NULL return was dereferenced unchecked by every
			// caller (48 of them) and crashed as a write at address 0x10.
			static Particle* Spawn(CraftWorld* world, const char* name, Vector3 position);
			static void QueueBlockCrack(CraftWorld* world, Vector3 position, int blockId);
			static void Release(Particle* p);
			static int ActiveCount();
			static void PrecacheTextures(CraftWorld* world);
			static void UpdateAll(CraftWorld* world, float dt, Vector3 camRight, Vector3 camUp);
			static void RenderAll(Frustum &camFrustum);

			void Init(CraftWorld* world, int kind, Vector3 position);
			void Update(CraftWorld* crtf, float dt, Vector3 camRight, Vector3 camUp);
			void Render(Frustum &camFrustum);

			bool CheckCollision(CraftWorld* crtf);

			void SetVelocity(Vector3 _velocity);
			void SetFramesAmount(int _totalFrames);
			void SetScale(float scale);
			void SetScale(float minScale, float maxScale);

            void SetMaxLiveTime(int _maxLiveTime);
            void SetLiveTime(int _liveTime);
            void SetLiveTime(int minLiveTime, int maxLiveTime);
            void SetGravity(float _gravity);
            void SetColor(Vector3 _color);
            void SetBrightness(float brightness);
            void SetBrightness(float minBrightness, float maxBrightness);

			void BuildVerticies();
			void BuildVerticiesForBlockCrack(CraftWorld* crtf, int blockId);
			void BuildVerticiesForItemCrack(CraftWorld* crtf, int itemId);

			Vector3 position;
			Vector3 velocity;

			bool toDestroy;
            float friction;

            int var1;
            int var2;
            int var3;

            float varf1;
            float varf2;

		private:

		    CraftPSPVertex *particleVertices;
			int particleSlot;

			float horizontalScale;
			float verticalScale;
			int texture;

			float timeToLive;
			float maxTimeToLive;
			int frame;
			int totalFrames;

			Vector3 color;

			float gravity;
			float myLight;
			bool rederMe;

			int kind;
			bool inUse;
			BoundingBox bBox;

			Vector3 billboardRight;
			Vector3 billboardUp;
			bool vertsBuilt;
		};
	}
}

#endif
