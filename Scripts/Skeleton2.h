#ifndef SKELETON2_H_
#define SKELETON2_H_

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <psptypes.h>
#include <Aurora/Math/Frustum.h>
#include <Aurora/Math/BoundingBox.h>
#include "LameMob.h"

class CraftWorld;

// The skeleton. It is the zombie's rig: it draws the shared biped bodyVertices and headVertices and
// the skeleton's own thin 2x12x2 limbs (world->skeletonHandVertices / skeletonLegVertices), on
// entity/skeleton/skeleton.png. What differs is the brain -- a bow instead of fists -- and that it
// cannot wear armour.
class Skeleton : public LameMob
{
public:

    Skeleton(Vector3 position1, int number1);
    ~Skeleton();

    void BoxCollision();
    void Update(CraftWorld *myWorld, Vector3 playerPos, float dt);
    void Render(Frustum &camFrustum, float dt);
    void TakeDamage(float damage, float power, float dt);
    void TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point);

    // --- Debug stick -----------------------------------------------------------
    // The bow leads the list because it is what this mob is being tested for; the generic set
    // follows in the base's order, so DebugAction delegates anything past index 0 back down.
    int DebugOptions(std::vector<std::string>& options);
    void DebugAction(int index);

    // Starts a draw immediately, exactly as if the AI had called for a shot -- the arrow still
    // leaves at the end of the draw, so this plays the animation rather than spawning a bare
    // projectile. A no-op while a draw is already running or after death.
    void TriggerShootDraw();

private:

    // Height above the feet that a shot leaves from. The skeleton is 1.99 blocks tall, so its eyes
    // sit just under the 1.74 the vanilla biped uses for a 1.8-tall model.
    static const float EYE_HEIGHT = 1.5f;

    float rHandAngle;
    float lHandAngle;
    float rLegAngle;
    float lLegAngle;

    float headAngleY;
    float headAngleZ;

    float animHandAngle;
    float animHandAngleT;

    float animLegAngle;
    float animLegAngleT;

    // The shooting animation. `UpdateHostileRangedAI` reports a due shot with a single-frame pulse,
    // and that pulse now *starts a draw* instead of firing on the spot: the bow slides back toward
    // the shoulder as the string is pulled, both arms tighten, and the arrow leaves at the end of
    // the draw with a short recoil. Only the bow's own movements are available here -- a skeleton's
    // arm is one rigid 2x12x2 box turning about the shoulder, so the hand can only travel on a
    // circle and cannot fold in toward the chest. A draw is therefore shown by the bow moving back
    // along the arm, which is the one part of the motion the rig can actually perform.
    //
    // These are the bow's timings, not the mob's: 0.65 s is vanilla's ~20 ticks of draw.
    static const float SHOOT_DRAW_TIME = 0.65f;
    static const float SHOOT_RECOIL_TIME = 0.25f;

    float shootDraw;      // 0..1 while the string is being pulled
    float shootRecoil;    // 1 at the moment of release, decaying to 0
    bool shootDrawing;    // true between the AI's call and the arrow leaving

    // Whether the draw in progress came from the ranged AI rather than from the debug stick's button.
    // Only an AI draw leaves with the elevation to the player solved for it; an unaided draw flies
    // level along whatever the skeleton was already facing. See ShootArrow.
    bool shootAimed;

    // Looses one Arrow2 from the eye, into world->mArrows so the player's bow and the skeleton share
    // one list and one block-collision test. Called at the end of the draw, from Update.
    //
    // The arrow leaves along the skeleton's own facing -- the body's heading plus the head's turn past
    // it -- so the bow is what aims, and the mob is never rotated to make a shot look right. 'aimed'
    // says whether anything turned that facing onto the player: the ranged AI has, so an AI shot still
    // carries the elevation and the lift that make it land; the debug stick's button has not, so that
    // shot goes out in front of wherever the skeleton happened to be looking.
    void ShootArrow(CraftWorld* myWorld, bool aimed);
};

#endif
