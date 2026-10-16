#ifndef AURORA_CAMERA_H
#define AURORA_CAMERA_H

#include <Aurora/Math/Vector3.h>
#include <Aurora/Math/Frustum.h>
#include <pspmath.h>

using namespace Aurora::Math;

namespace Aurora
{
	namespace Graphics
	{
		class Camera
		{
		public:
			Camera();
			virtual ~Camera();

			void PositionCamera(float positionX, float positionY, float positionZ,
						 		    float viewX,     float viewY,     float viewZ,
									float upVectorX, float upVectorY, float upVectorZ);

			void Move(float speed);
			void MoveTo(Vector3 newPos);
			void MovePhysic(float speed);
			Vector3 MoveFoCollision(float speed);
			void MoveAfterCollision(Vector3 newPos);

			void Strafe(float speed);
			void StrafePhysic(float speed);

			void MovePhysicNoY(float speed);
			//Vector3 StrafeFoCollision(float speed);
			//void StrafeAfterCollision(Vector3 newPos);

			void RotateView(float angle, float x, float y, float z);
			void PitchView(float speed);
			void RebuildView();
			void AddYaw(float radians);
			void AddPitch(float radians);
			// targetYaw/targetPitch are radians per second. The rate eases toward them and the
			// angle advances by the eased rate times dt, so a held button still turns at a
			// constant speed and a released one coasts to a stop. A long frame does not add a
			// fixed number of degrees.
			void ApplyLook(float targetYaw, float targetPitch, float dt);
			void CancelLookEase();
			void ApplyWalkBob(float amount, float phase);

			float atan_(float y, float x);


			Frustum mFrustum;
			bool needUpdate;


		//private:

			// The camera's position
			Vector3 m_vPosition;

			// The camera's offset
			Vector3 m_vOffset;

			// Third-person boom: pushed back from m_vPosition along the view direction, applied only
			// when the view matrix is built. m_vPosition itself stays at the eye, so picking, physics
			// and every other system that reads the camera keep working from the player's eyes.
			Vector3 m_vBoom;

			// Third-person front view. The boom is mirrored through the eye and the view matrix is
			// turned round so it looks back at the player, which is the same as mirroring the whole
			// view through the eye: horAngle/upDownAngle stay the player's *own* aim -- the head
			// follows them, the body lags, and every pick ray still starts at the eyes -- and only
			// the matrix that renders is reversed. Java's thirdPersonView == 2 does the same.
			bool m_vMirror;

			// The camera's view
			Vector3 m_vView;

			// The camera's up vector
			Vector3 m_vUpVector;

			// The camera's strafe vector
			Vector3 m_vStrafe;

			//TODO wtf is this for??
			Vector3 m_vVelocity;

			Vector3 vVector;

			float horAngle;

			float upDownAngle;
			float upDownAngle2;

			// Camera roll, in radians, about the view direction. The portal's wobble is the only thing
			// that sets it; RebuildView leaves it alone so a caller can set it and rebuild.
			float rollAngle;

			// Radians per second, after easing. ApplyLook writes these; a menu zeroes them.
			// Kept after the fields above so a build that did not recompile every camera user
			// still reads horAngle, pitch and roll at the same offsets.
			float lookRateYaw;
			float lookRatePitch;
		};
	}
}

#endif
