#ifndef INVENTORYPLAYER_H_
#define INVENTORYPLAYER_H_

#include <Aurora/Math/Vector3.h>
#include <Aurora/Graphics/Vertex.h>
#include <pspmath.h>
#include <Scripts/CraftWorld2.h>
#include <Scripts/TextureHelper.h>

namespace Aurora
{
	namespace Graphics
	{
		class InventoryPlayer
		{
		public:
			InventoryPlayer();
			~InventoryPlayer();

			void Update();
			void Render(CraftWorld *world, float dt, float angle, int texNum, int handItemId);

			// Third-person view: the whole player standing in the world at 'feet' + 1.125 (the model's
			// own proportions -- 32 px from foot to crown, origin at the hip, which is what the shared
			// hand/leg/head meshes are built around). 'faceAngle' is the mob-style angle whose forward
			// is (sin, 0, cos); 'legSwing'/'armSwing' are the walk swings in radians, positive = forward.
			// 'headYaw' is how far the head is turned away from the body and 'headPitch' is the camera's
			// own pitch, both in radians: the head looks where the player looks while the body lags.
			// 'hitSwing' is the arm swing for a punch or a block placement, in radians, positive =
			// forward; it goes to the right arm and the held item only, as in Java.
			// 'bowStage' is the held bow's draw stage and is ignored unless the held item is a bow.
			// -1 means the bow is held but not being drawn, 0..3 means the string is pulled (0 is the
			// standby art on the frame the trigger goes down, 1..3 are the three pulling frames). A
			// stage of 0 or more is also what lifts the bow arm out of its walk pose and turns it with
			// the head -- see kBowArmRaise. It is last so the positional callers of the angles above
			// cannot bind an angle to it by mistake.
			void RenderInWorld(CraftWorld *world, Vector3 feetPosition, float faceAngle,
			                   float legSwing, float armSwing, float light,
			                   int texNum, int handItemId,
			                   float headYaw = 0.0f, float headPitch = 0.0f,
			                   float hitSwing = 0.0f, int bowStage = -1);

			float mainAngle;
			float scale;
			float animT;

		private:

            float rHandAngle;
            float lHandAngle;
            float rLegAngle;
            float lLegAngle;

            Vector3 position;
		};
	}
}

#endif

