#ifndef TNT_H
#define TNT_H

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <Aurora/Math/Frustum.h>
#include <Scripts/CraftWorld2.h>
#include <Aurora/Math/BoundingBox.h>

class CraftWorld;

class TNT
{
public:
    TNT(CraftWorld* world, Vector3 position2, Vector3 velocity2);
    ~TNT();

    void Update(float dt);
    void Render();
    void SetVelociy(Vector3 velocity2);
    void SetPosition(Vector3 position2);
    void ActivateOtherTNTs();
    void HurtNearbyMobs(float dt);
    // Fraction (0..1) of rays from the explosion point that reach the target box unblocked
    float ExposureTo(Vector3 from, Vector3 center, float half);
    // Axis separated collision helpers used by the per-step movement integration
    bool BoxHitsSolid(const Vector3& p);
    void MoveAxis(int axis, float delta);

    float timeToExplode;
    float explodeRadius;
    Vector3 position;
    Vector3 velocity;
    bool toDestroy;
    int num;
    bool ignitedByPlayer;

private:

    float animationTime;
    int TNTid;

    float blockScale;
    bool onGround;

    CraftWorld* myWorld;
};

#endif

