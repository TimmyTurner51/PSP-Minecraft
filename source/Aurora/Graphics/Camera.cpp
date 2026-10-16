#include <Aurora/Graphics/Camera.h>
#include <math.h>
#define PI 3.1415926535897f

#define PI_FLOAT     3.14159265f
#define PIBY2_FLOAT  1.5707963f

namespace Aurora
{
	namespace Graphics
	{
		Camera::Camera()
		{
			needUpdate = true;
			upDownAngle2 = 0.0f;
			upDownAngle = 0.0f;
			horAngle = 270.0f;
			rollAngle = 0.0f;
			lookRateYaw = 0.0f;
			lookRatePitch = 0.0f;

			m_vOffset = Vector3(0.0f, 0.0f, 0.0f);
			m_vBoom = Vector3(0.0f, 0.0f, 0.0f);
			m_vMirror = false;
			m_vPosition = Vector3(0.0f, 0.0f, 0.0f);
			m_vView = Vector3(0.0f, 0.0f, 0.0f);
			m_vUpVector = Vector3(0.0f, 1.0f, 0.0f);
			m_vStrafe = Vector3(0.0f, 0.0f, 0.0f);
			vVector = Vector3(0.0f, 0.0f, 0.0f);
			m_vVelocity = Vector3(0.0f, 0.0f, 0.0f);
		}

		Camera::~Camera()
		{

		}

		void Camera::PositionCamera(float positionX, float positionY, float positionZ,
								 		    float viewX,     float viewY,     float viewZ,
											float upVectorX, float upVectorY, float upVectorZ)
		{
			m_vPosition	= Vector3(positionX, positionY, positionZ);
			m_vView		= Vector3(viewX, viewY, viewZ);
			m_vUpVector	= Vector3(upVectorX, upVectorY, upVectorZ);
			vVector = m_vView - m_vPosition;
			float len = vVector.magnitude();
			if(len > 0.0001f)
				vVector = vVector * (1.0f / len);
			else
				vVector = Vector3(0.0f, 0.0f, -1.0f);
			float py = vVector.y;
			if(py > 1.0f) py = 1.0f;
			if(py < -1.0f) py = -1.0f;
			upDownAngle = asinf(py);
			horAngle = (PI + atan_(-vVector.z, -vVector.x)) / PI * 180.0f;
			RebuildView();
		}

		void Camera::RebuildView()
		{
			if(upDownAngle > 1.561f)
				upDownAngle = 1.561f;
			if(upDownAngle < -1.561f)
				upDownAngle = -1.561f;
			while(horAngle >= 360.0f)
				horAngle -= 360.0f;
			while(horAngle < 0.0f)
				horAngle += 360.0f;

			float yaw = horAngle * PI / 180.0f;
			float cp = cosf(upDownAngle);
			vVector.x = cp * cosf(yaw);
			vVector.y = sinf(upDownAngle);
			vVector.z = cp * sinf(yaw);
			m_vView = m_vPosition + vVector;
			m_vUpVector = Vector3(0.0f, 1.0f, 0.0f);
			if(rollAngle != 0.0f)
			{
				// Roll about the view direction, which tips the horizon without touching vVector -- so
				// the player's aim, and every pick ray derived from it, is completely unaffected. This
				// is Rodrigues' rotation of world-up about the forward axis.
				const Vector3 fwd = vVector;   // RebuildView always normalises vVector below
				const Vector3 up(0.0f, 1.0f, 0.0f);
				const Vector3 axis = Vector3::cross(fwd, up);
				const float s = sinf(rollAngle);
				const float c = cosf(rollAngle);
				const float d = fwd.x * up.x + fwd.y * up.y + fwd.z * up.z;
				m_vUpVector = up * c + axis * s + fwd * (d * (1.0f - c));
			}
			needUpdate = true;
		}

		void Camera::AddYaw(float radians)
		{
			horAngle += radians * 180.0f / PI;
			RebuildView();
		}

		void Camera::AddPitch(float radians)
		{
			upDownAngle += radians;
			RebuildView();
		}

		void Camera::ApplyLook(float targetYaw, float targetPitch, float dt)
		{
			if(dt < 0.0f)
				dt = 0.0f;
			// About a tenth of a second to reach most of the stick's rate, and the same to stop.
			// A full-speed release therefore coasts roughly ten degrees, then sits.
			const float keep = expf(-10.0f * dt);
			lookRateYaw = targetYaw + (lookRateYaw - targetYaw) * keep;
			lookRatePitch = targetPitch + (lookRatePitch - targetPitch) * keep;
			if(targetYaw == 0.0f && fabsf(lookRateYaw) < 0.02f)
				lookRateYaw = 0.0f;
			if(targetPitch == 0.0f && fabsf(lookRatePitch) < 0.02f)
				lookRatePitch = 0.0f;
			if(lookRateYaw != 0.0f)
				AddYaw(lookRateYaw * dt);
			if(lookRatePitch != 0.0f)
				AddPitch(lookRatePitch * dt);
		}

		void Camera::CancelLookEase()
		{
			lookRateYaw = 0.0f;
			lookRatePitch = 0.0f;
		}

		void Camera::Move(float speed)
		{
			RebuildView();
			m_vVelocity = vVector * speed;
			m_vPosition = m_vPosition + m_vVelocity;
			RebuildView();
		}

		void Camera::MoveTo(Vector3 newPos)
		{
			m_vPosition = newPos;
			RebuildView();
		}

		void Camera::MovePhysic(float speed)
		{
			vVector = m_vView - m_vPosition;
			vVector.normalize();

			m_vVelocity += vVector * speed;
			needUpdate = true;
		}

		void Camera::MovePhysicNoY(float speed)
		{
			vVector = m_vView - m_vPosition;
			vVector.y = 0.0f;
			float xz = vVector.x * vVector.x + vVector.z * vVector.z;
			if(xz < 0.0001f)
			{
				float a = horAngle * PI / 180.0f;
				vVector.x = cosf(a);
				vVector.z = sinf(a);
			}
			else
				vVector.normalize();

			m_vVelocity += vVector * speed;
			needUpdate = true;
		}

		Vector3 Camera::MoveFoCollision(float speed)
		{
			vVector = m_vView - m_vPosition;
			vVector.normalize();

			m_vVelocity = vVector * speed;

			Vector3 newPos = m_vPosition + m_vVelocity;

			return newPos;
		}

		void Camera::MoveAfterCollision(Vector3 newPos)
		{
			m_vPosition = newPos + m_vVelocity;
			RebuildView();
		}

		void Camera::Strafe(float speed)
		{
			m_vStrafe = Vector3::cross(m_vView - m_vPosition, m_vUpVector);

			// Normalize the strafe vector
			m_vStrafe.normalize();

			// Add the strafe vector to our position
			m_vVelocity = m_vStrafe * speed;
			m_vPosition.x += m_vVelocity.x;
			m_vPosition.z += m_vVelocity.z;

			// Add the strafe vector to our view
			m_vView.x += m_vVelocity.x;
			m_vView.z += m_vVelocity.z;

			needUpdate = true;
		}

		void Camera::StrafePhysic(float speed)
		{
			Vector3 fwd = m_vView - m_vPosition;
			fwd.y = 0.0f;
			float xz = fwd.x * fwd.x + fwd.z * fwd.z;
			if(xz < 0.0001f)
			{
				float a = horAngle * PI / 180.0f;
				fwd.x = cosf(a);
				fwd.z = sinf(a);
			}
			else
				fwd.normalize();

			m_vStrafe = Vector3::cross(fwd, Vector3(0.0f, 1.0f, 0.0f));
			m_vStrafe.y = 0.0f;
			if(m_vStrafe.magnitudeSq() < 0.0001f)
				m_vStrafe = Vector3(1.0f, 0.0f, 0.0f);
			else
				m_vStrafe.normalize();
			m_vVelocity += m_vStrafe * speed;
			needUpdate = true;
		}

		/*Vector3 Camera::StrafeFoCollision(float speed)
		{
			m_vStrafe = Vector3::cross(m_vView - m_vPosition, m_vUpVector);

			// Normalize the strafe vector
			m_vStrafe.normalize();

			// Add the strafe vector to our position
			m_vVelocity = m_vStrafe * speed;

			Vector3 newPos;

			m_vPosition.x += m_vVelocity.x;
			m_vPosition.z += m_vVelocity.z;
		}

		void Camera::StrafeAfterCollision(Vector3 newPos)
		{

		}*/

		void Camera::RotateView(float angle, float x, float y, float z)
		{
			(void)z;
			if(y != 0.0f && x == 0.0f)
				AddYaw(-angle);
			else
				AddPitch(angle);
		}

		void Camera::PitchView(float speed)
		{
			AddPitch(speed);
		}

		void Camera::ApplyWalkBob(float amount, float phase)
		{
			if(amount < 0.0f)
				amount = 0.0f;
			if(amount > 1.0f)
				amount = 1.0f;

			float f2 = amount * 0.1f;
			m_vOffset.x = 0.0f;
			m_vOffset.y = -fabsf(cosf(phase) * f2);
			m_vOffset.z = 0.0f;
			needUpdate = true;
		}

		float Camera::atan_(float y, float x)
		{
            if ( x == 0.0f )
            {
                if ( y > 0.0f ) return PIBY2_FLOAT;
                if ( y == 0.0f ) return 0.0f;
                return -PIBY2_FLOAT;
            }
            float atan;
            float z = y/x;
            if ( fabs( z ) < 1.0f )
            {
                atan = z/(1.0f + 0.28f*z*z);
                if ( x < 0.0f )
                {
                    if ( y < 0.0f ) return atan - PI_FLOAT;
                    return atan + PI_FLOAT;
                }
            }
            else
            {
                atan = PIBY2_FLOAT - z/(z*z + 0.28f);
                if ( y < 0.0f ) return atan - PI_FLOAT;
            }
            return atan;
		}
	}
}

