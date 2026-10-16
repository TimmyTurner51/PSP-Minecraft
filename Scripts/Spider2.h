#ifndef SPIDER2_H_
#define SPIDER2_H_

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <psptypes.h>
#include <Aurora/Math/Frustum.h>
#include <Aurora/Math/BoundingBox.h>
#include "LameMob.h"

class CraftWorld;

class Spider : public LameMob
{
public:

    Spider(Vector3 position1, int number1);
    ~Spider();

    void BoxCollision();
    void Update(CraftWorld *myWorld, Vector3 playerPos, float dt);
    void Render(Frustum &camFrustum, float dt);
    void TakeDamage(float damage, float power, float dt);
    void TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point);

private:

    // Walk cycle phase (radians). LameMob::UpdateLegAnim advances it while moving and walks it
    // back to a rest position when the spider stops; the eight legs are all driven off it.
    float animLegAngleT;
    float legSwingAmount;   // 0..1, Java limbSwingAmount
    float stepDistance;     // blocks walked since the last footfall clip

    float headAngleY;
    float headAngleZ;

    // Both are called between the rig transform and its matching pop in Render.
    void DrawLegs();
    void DrawEyes();
};

#endif
