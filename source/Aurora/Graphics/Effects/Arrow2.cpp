#include <Aurora/Graphics/Effects/Arrow2.h>
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.1415926535897f
#define DEG_TO_RAD (PI / 180.0f)

namespace Aurora
{
	namespace Graphics
	{
		// Gravity on a shot, in blocks per second squared. A full-charge shot leaves at maxSpeed and
		// this pulls it down; the two together are what give the bow its arc.
		static const float ARROW_GRAVITY = 16.0f;
		static const float ARROW_MAX_SPEED = 28.0f;

		// How long a landed arrow refuses to be collected, in seconds. Matched to Drop's own
		// `existingTime >= 0.5f` so a dropped stack and a landed arrow behave the same way underfoot.
		static const float ARROW_PICKUP_DELAY = 0.5f;

		// Vanilla's own flight limit, in seconds. Only flying arrows are subject to it.
		static const float ARROW_FLIGHT_LIFETIME = 60.0f;

		// entity/arrow.png is vanilla's 32x32 *model unwrap*, not a picture of an arrow: the shaft,
		// the head and the fletching are separate strips, and every strip is drawn twice -- once per
		// face of the model. Painting the whole sheet onto one quad therefore puts both copies on
		// screen at once, side by side, and a shot showed the lit arrow with an unlit pink/purple
		// twin hanging beside it. That twin is the "ghost arrow": there is only ever one arrow, it
		// was one sheet sampled too widely.
		//
		// One complete, lit arrow lives in the top-left corner of the sheet: 16 px of shaft and head
		// across, 5 px of fletching down (rows 0-1 and 3-4 above and below the shaft on row 2, which
		// is why the crop is 5 px and not 4). That rectangle, and nothing else, is what the geometry
		// samples; the rows of the mirrored copy start at 11, so there is no art left for the filter
		// to bleed in from either edge.
		//
		// `u` runs down the arrow (0 at the fletching, ARROW_TEX_U at the point) and `v` across it,
		// and the aspect below keeps the quad in the crop's own 16:5 proportion, so the shaft is as
		// thin as it is in the artwork instead of being stretched across a square.
		static const float ARROW_TEX_U = 16.0f / 32.0f;
		static const float ARROW_TEX_V = 5.0f / 32.0f;

		// Half the arrow's length, in blocks. Vanilla's own arrow model is about half a block long,
		// and this is the one dimension a player judges a shot by -- the width follows from the crop.
		static const float ARROW_HALF_LENGTH = 0.30f;

		Arrow2::Arrow2(float pos2x, float pos2y, float pos2z)
		{
			// Two crossed quads, like vanilla's own arrow model: a shaft you can see from any side
			// and a head at the far end, with the fletching's own width across it. The alternative --
			// one quad turned to face the camera -- cannot ever look like an arrow, because a shot
			// leaves along the view axis and a camera-facing quad is edge-on to it at exactly the
			// moment the player is looking at it.
			hScale = ARROW_HALF_LENGTH;
			vScale = hScale * (ARROW_TEX_V / ARROW_TEX_U);

			position = Vector3(pos2x,pos2y,pos2z);
			prevPosition = position;
			velocity = Vector3(0,0,0);

			// Only until SetVeloc/SetVelocity aim it. An arrow is aimed before it is ever drawn, but
			// BuildVertices reads this every frame and must never be handed a zero vector, which is
			// the one input that has no cross to build from.
			flightDir = Vector3(1,0,0);

			damage = 0.0f;
			fromMob = false;

			stuck = false;
			pickupDelay = 0.0f;
			flightTime = 0.0f;

			// 0 until SetTexture names the real one; Render binds whatever is here.
			textureNumber = 0;

			arrowVertices = (CraftPSPVertex*)memalign(16,12 * sizeof(CraftPSPVertex));

			startSpeed = ARROW_MAX_SPEED;
		}

		Arrow2::~Arrow2()
		{
			if(arrowVertices != NULL)
			{
				free(arrowVertices);
			}
		}

		void Arrow2::SetTexture(int texture)
		{
			textureNumber = texture;
		}

		void Arrow2::SetVeloc(float verAngle, float horAngle, float power)
		{
			if(power < 0.0f) power = 0.0f;
			if(power > 1.0f) power = 1.0f;

			// Vanilla never fires at a tap: the charge floor is what stops a zero-power shot from
			// dropping at the player's feet, so the speed curve starts at a quarter of full.
			const float speed = startSpeed * (0.25f + 0.75f * power);

			velocity.x = -sinf(horAngle-PI/2) * cosf(verAngle) * speed;
			velocity.y = sinf(verAngle) * speed;
			velocity.z = cosf(horAngle-PI/2) * cosf(verAngle) * speed;

			TrackFlightDir();
		}

		void Arrow2::SetVelocity(Vector3 dir, float power)
		{
			if(power < 0.0f) power = 0.0f;
			if(power > 1.0f) power = 1.0f;

			const float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
			if(len <= 0.0001f)
			{
				velocity = Vector3(0,0,0);
				return;
			}

			const float speed = startSpeed * (0.25f + 0.75f * power);
			velocity.x = (dir.x / len) * speed;
			velocity.y = (dir.y / len) * speed;
			velocity.z = (dir.z / len) * speed;

			TrackFlightDir();
		}

		void Arrow2::TrackFlightDir()
		{
			const float len = sqrtf(velocity.x*velocity.x + velocity.y*velocity.y + velocity.z*velocity.z);
			if(len <= 0.0001f)
			{
				// Stopped: the last heading stands, which is what leaves a landed arrow stuck in the
				// face it hit rather than snapping to some other angle as it settles.
				return;
			}

			flightDir.x = velocity.x / len;
			flightDir.y = velocity.y / len;
			flightDir.z = velocity.z / len;
		}

		void Arrow2::Update(CraftWorld* crtf, float dt)
		{
			// A landed arrow has stopped for good. The caller keeps it in the world and keeps drawing
			// it (BuildVertices is still run on it), but nothing here may move it again -- gravity on
			// a stuck arrow is how it would sink through the ground it is supposed to be stuck in.
			if(stuck == true)
				return;

			flightTime += dt;

			prevPosition = position;

			velocity.y -= ARROW_GRAVITY * dt;

			position.x += velocity.x * dt;
			position.y += velocity.y * dt;
			position.z += velocity.z * dt;

			// Gravity turns the heading down a little every step, so the model pitches over as the
			// shot drops -- an arrow that keeps the launch angle all the way is the thing that looks
			// wrong about a long shot, not the drop itself.
			TrackFlightDir();
		}

		void Arrow2::TickPickupDelay(float dt)
		{
			if(pickupDelay > 0.0f)
			{
				pickupDelay -= dt;
				if(pickupDelay < 0.0f)
					pickupDelay = 0.0f;
			}
		}

		void Arrow2::RestoreLanded(Vector3 pos, Vector3 dir, bool mobShot, float delay)
		{
			// Everything a landed arrow has: where it stopped, the heading it arrived on, whose shot
			// it was and how long before it may be collected. The velocity stays zero, which is what
			// `stuck` already means to Update -- nothing here may move it again.
			position = pos;
			prevPosition = pos;
			velocity = Vector3(0,0,0);

			// A save can hold a heading that is not usable: an older file from before this blob
			// existed has none at all, and a truncated one could hold a zero. BuildVertices has no
			// cross to work from at zero, so this falls back to east rather than drawing a degenerate
			// quad -- the arrow still appears, at the wrong angle, which beats not appearing.
			const float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
			if(len <= 0.0001f)
			{
				flightDir = Vector3(1,0,0);
			}
			else
			{
				flightDir.x = dir.x / len;
				flightDir.y = dir.y / len;
				flightDir.z = dir.z / len;
			}

			stuck = true;
			fromMob = mobShot;
			pickupDelay = delay;
		}

		bool Arrow2::Expired() const
		{
			return (stuck == false) && (flightTime >= ARROW_FLIGHT_LIFETIME);
		}

		void Arrow2::BuildVertices(CraftWorld* crtf)
		{
			float myLight = crtf->BlockFinalLight(position.x,position.y,position.z);
			const unsigned int light = GU_COLOR(myLight,myLight,myLight,1);

			// The arrow's own axes: `head` is the way it is travelling, and the two crossed quads are
			// laid along the two perpendiculars to it. Nothing here reads the camera, which is the
			// whole difference from the billboard this replaced -- the arrow holds one attitude in the
			// world, so a shot seen from the side shows its length, one seen from behind shows the
			// fletching, and one in a wall keeps pointing into the wall as the player walks around it.
			//
			// `side` is the heading crossed with the world's up rather than with a camera axis: it is
			// the same every frame, so the roll is stable and a landed arrow does not spin as the view
			// moves. Straight up or down is the one heading that has no such cross, and there the art
			// would be undefined anyway; north is the fallback and it keeps the maths honest.
			Vector3 head = flightDir;
			Vector3 side, other;

			if(head.y > 0.999f || head.y < -0.999f)
			{
				side = Vector3(head.y, -head.x, 0.0f);
			}
			else
			{
				side = Vector3(-head.z, 0.0f, head.x);
			}
			const float sideLen = sqrtf(side.x*side.x + side.y*side.y + side.z*side.z);
			if(sideLen <= 0.0001f)
			{
				// Cannot happen with a unit heading, but a zero here would draw a degenerate quad and
				// there is no caller that should ever see a garbage arrow instead of a drawn one.
				side = Vector3(1.0f, 0.0f, 0.0f);
			}
			else
			{
				side.x /= sideLen; side.y /= sideLen; side.z /= sideLen;
			}

			other.x = head.y*side.z - head.z*side.y;
			other.y = head.z*side.x - head.x*side.z;
			other.z = head.x*side.y - head.y*side.x;

			// Tail and point, once, for both quads. The quad is centred on the position, so a landed
			// arrow stands half in the block it hit and half out of it -- which, with the depth test
			// on, is what "stuck in the wall" looks like from the outside.
			const Vector3 tail(position.x - head.x*hScale, position.y - head.y*hScale, position.z - head.z*hScale);
			const Vector3 tip (position.x + head.x*hScale, position.y + head.y*hScale, position.z + head.z*hScale);

			// Two quads at right angles. Each is two triangles, and each carries the same slice of
			// entity/arrow.png: u from the fletching at the tail to the point at the tip, v across
			// the fletching's own 5 px, so the shaft stays as thin as it is in the artwork.
			const Vector3 axes[2] = { side, other };

			int i = 0;
			for(int q = 0; q < 2; q++)
			{
				const float bx = axes[q].x * vScale;
				const float by = axes[q].y * vScale;
				const float bz = axes[q].z * vScale;

				// 0 tail+side, 1 tail-side, 2 tip+side, 3 tip-side.
				const float vx[4] = { tail.x + bx, tail.x - bx, tip.x + bx, tip.x - bx };
				const float vy[4] = { tail.y + by, tail.y - by, tip.y + by, tip.y - by };
				const float vz[4] = { tail.z + bz, tail.z - bz, tip.z + bz, tip.z - bz };
				const float tu[4] = { 0.0f, 0.0f, ARROW_TEX_U, ARROW_TEX_U };
				const float tv[4] = { 0.0f, ARROW_TEX_V, 0.0f, ARROW_TEX_V };

				// (0,1,2) then (2,1,3). With culling off in Render the winding only decides which of
				// the two faces is front, and both of them are visible at some point in a flight.
				const int tri[6] = { 0, 1, 2, 2, 1, 3 };

				for(int t = 0; t < 6; t++)
				{
					const int c = tri[t];
					arrowVertices[i].x = vx[c];
					arrowVertices[i].y = vy[c];
					arrowVertices[i].z = vz[c];
					arrowVertices[i].u = tu[c];
					arrowVertices[i].v = tv[c];
					arrowVertices[i].color = light;
					i++;
				}
			}

			sceKernelDcacheWritebackInvalidateRange(arrowVertices,12 * sizeof(CraftPSPVertex));
		}

		bool Arrow2::CheckCollision(CraftWorld* crtf) const
		{
			// Sample the whole step it just took rather than only its tip. A full-charge arrow covers
			// about 0.9 blocks in one 30 Hz frame, which is enough to skip past a one-block wall if
			// only the leading point is tested -- so walk the segment.
			for(float s = 0.0f; s <= 1.0f; s += 0.25f)
			{
				float px = prevPosition.x + (position.x - prevPosition.x) * s;
				float py = prevPosition.y + (position.y - prevPosition.y) * s;
				float pz = prevPosition.z + (position.z - prevPosition.z) * s;
				if(crtf->BlockSolid(px,py,pz) == true)
					return true;
			}

			return crtf->BlockSolid(position.x,position.y,position.z) == true;
		}

		void Arrow2::Stick(CraftWorld* crtf)
		{
			// The step that hit ended *inside* a block: `position` is the buried tip and
			// `prevPosition` is the open-air end of that same step. Walk the step again and keep the
			// last sample that was still clear, so the arrow comes to rest against the face it hit.
			// Dropping it back to prevPosition instead would leave it hanging up to a block short of
			// the wall, which at bow range reads as an arrow that stopped in mid-air.
			//
			// The `BlockSolid` call is the same one, with the same float-to-int truncation, that
			// CheckCollision used to decide it hit, so "clear" here means exactly what it meant there.
			const Vector3 buried = position;

			Vector3 landed = prevPosition;
			for(float s = 0.25f; s <= 1.0f; s += 0.25f)
			{
				Vector3 p(prevPosition.x + (buried.x - prevPosition.x) * s,
				          prevPosition.y + (buried.y - prevPosition.y) * s,
				          prevPosition.z + (buried.z - prevPosition.z) * s);
				if(crtf->BlockSolid(p.x,p.y,p.z) == true)
					break;
				landed = p;
			}

			position = landed;
			velocity = Vector3(0,0,0);
			stuck = true;
			pickupDelay = ARROW_PICKUP_DELAY;
		}

		void Arrow2::Render()
		{
			// The arrow owns its texture (SetTexture), unlike the snowball whose texture the state
			// binds. Both the player's bow and the skeleton's create an arrow and name the art once.
			TextureManager::Instance()->SetTextureModeulate(textureNumber);

			sceGuEnable(GU_TEXTURE_2D);

			// Two-sided, so the cross reads from behind as well as in front: a quad's back face is
			// culled by default and half of a crossed pair is always a back face from any given view.
			// GU_CULL_FACE is on globally (DrawSetCulling only flips the winding, see drawFull3DItem),
			// so it is turned off for the draw and put back -- the same thing LameMob::RenderFire does.
			sceGuDisable(GU_CULL_FACE);

			// Cut-out rather than blended. The crop has transparent pixels around the shaft and the
			// fletching, and the two quads cross through each other, so alpha blending would darken
			// every texel the pair has in common. This is also what the item and mob art do.
			sceGuEnable(GU_ALPHA_TEST);

			sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF |GU_COLOR_8888| GU_VERTEX_32BITF | GU_TRANSFORM_3D, 12, 0, arrowVertices);

			sceGuDisable(GU_ALPHA_TEST);
			sceGuEnable(GU_CULL_FACE);
			sceGuDisable(GU_TEXTURE_2D);
		}
	}
}
