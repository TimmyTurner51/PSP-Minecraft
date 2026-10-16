#ifndef ARROW2_H_
#define ARROW2_H_

#include <Aurora/Math/Vector3.h>
#include <Aurora/Graphics/Vertex.h>
#include <pspmath.h>
#include <Scripts/CraftWorld2.h>
#include <math.h>
#include <psptypes.h>

// An arrow in flight. Split out from the bow and from the skeleton so there is exactly one piece of
// code that moves a shot: the player's bow and the skeleton's bow both push one of these, and both
// read the same block-collision test.
//
// Unlike SnowBall2, velocity here is in blocks per *second*, not blocks per frame. The snowball
// adds a fixed per-frame offset, so its speed silently doubles if the frame rate does; an arrow is
// the one projectile whose range and drop the player can feel, and the spider climb showed how much
// trouble frame-rate-dependent motion causes on a 30 Hz PSP. Every step below multiplies by dt.
//
// A landed arrow is *not* deleted. It stops where it hit, keeps being drawn, and -- if the player
// fired it -- can be walked over and collected. The consequences of a shot therefore live in two
// places and not three: the simulation and its resolution are `CraftWorld::UpdateArrows`, and the
// drawing is whatever the play state's render loop does with `BuildVertices`/`Render`. This class
// knows nothing about the player's inventory or about mobs.
namespace Aurora
{
	namespace Graphics
	{
		class Arrow2
		{
		public:
			Arrow2(float pos2x, float pos2y, float pos2z);
			~Arrow2();

			void SetTexture(int texture);

			// Player shot: the same two angles the camera reports, so the arrow leaves exactly where
			// the crosshair points. power is 0..1 -- vanilla scales both the speed and the drop by the
			// bow's charge, which is why a tapped shot falls short of a held one.
			void SetVeloc(float verAngle, float horAngle, float power);

			// Skeleton shot: an aim vector already worked out from the target, so the mob does not
			// have to express its aim as camera angles.
			void SetVelocity(Vector3 dir, float power);

			// One step of flight. A landed arrow is left where it is: nothing about it moves again
			// until it is collected or evicted, so this is a no-op for one (see stuck).
			void Update(CraftWorld* crtf, float dt);

			// Rebuilds the arrow's own geometry around the current position and heading. Split out of
			// Update because a landed arrow is still drawn -- it has stopped, it has not been taken out
			// of the world -- and because the state has to call it while the world is paused, when
			// Update deliberately is not called at all.
			void BuildVertices(CraftWorld* crtf);

			// True when the step it is about to take lands inside a solid block.
			bool CheckCollision(CraftWorld* crtf) const;

			// Land it. The velocity is dropped and the position is walked back to the last point on
			// the step that was clear of blocks, so it sits *against* the face it hit instead of a
			// whole step short of it or buried a block deep inside it. Starts the pickup delay.
			void Stick(CraftWorld* crtf);

			// Runs the "cannot be collected the instant it lands" timer down.
			void TickPickupDelay(float dt);

			// True for an arrow that has been in the air longer than vanilla lets one fly. The caller
			// removes it. Only ever true for one that is still flying: a landed arrow is meant to last.
			bool Expired() const;

			// A landed arrow the player fired, with its delay over: the caller may collect it. A mob's
			// arrow is never collectable, which is the whole of the shooter tag.
			bool Collectable() const { return stuck == true && fromMob == false && pickupDelay <= 0.0f; }

			// The heading a landed arrow is holding. A flying arrow's heading is just its velocity
			// direction and the save does not need it, but a landed one has no velocity left to read
			// it from -- Stick() drops it to zero -- so the orientation has to be asked for by name.
			// This is what lets a stuck arrow be written to the save and come back pointing the way
			// it went in.
			Vector3 FlightDir() const { return flightDir; }

			// Put a landed arrow back exactly as the save left it. The world's load path has a
			// position, a heading, whose shot it was and how much of the pickup delay is left, and
			// nothing else -- so this is the whole of what it needs to reconstruct one, rather than
			// re-aiming a fresh arrow at a target that is no longer there.
			void RestoreLanded(Vector3 pos, Vector3 dir, bool mobShot, float delay);

			void Render();

			Vector3 position;
			Vector3 velocity;

			// Where it was before the last step. The caller sweeps this segment against mobs so a fast
			// arrow cannot pass straight through one between frames, and Stick() uses it as the
			// outside-the-block end of that step.
			Vector3 prevPosition;

			// Damage this shot deals, so the bow and the skeleton can differ without a second class.
			float damage;

			// Set for a shot fired by a mob. Decides whether the player may collect it, and the state
			// uses it to skip the shooter's own kind when sweeping for a hit, and to choose the
			// knockback direction.
			bool fromMob;

			// Landed in a block and no longer simulated. Drawn wherever it fell until the player picks
			// it up or the landed-arrow cap in `CraftWorld::UpdateArrows` evicts it.
			bool stuck;

			// Seconds still to run before the player may collect this one.
			float pickupDelay;

		private:

			// Re-reads flightDir off the current velocity. Called whenever a shot is aimed and once
			// per step; a zero velocity leaves the last heading alone, so a landed arrow keeps the
			// direction it arrived on.
			void TrackFlightDir();

			// Unit vector along the way the arrow is travelling. The geometry below is laid along
			// this, so a shot points the way it is going and a landed one stays stuck in the face it
			// hit -- both from every angle, which a camera-facing billboard cannot do.
			Vector3 flightDir;

			// Seconds it has been in the air, flying only. Vanilla lets an arrow fly for a minute and
			// then removes it, and that is what stops a shot that never hits anything -- one fired
			// straight up over the world edge, say, where there is no block to land in -- from living
			// for the rest of the session.
			float flightTime;

			CraftPSPVertex* arrowVertices;
			int textureNumber;
			float hScale;
			float vScale;

			float startSpeed;
		};
	}
}

#endif
