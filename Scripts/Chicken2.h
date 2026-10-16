#ifndef CHICKEN_H_
#define CHICKEN_H_

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <psptypes.h>
#include <Aurora/Math/Frustum.h>

#include <Aurora/Math/BoundingBox.h>
#include <Aurora/Math/Frustum.h>
#include "LameMob.h"
#include "Blocks.h"
#include "CraftWorld2.h"

class CraftWorld;

class Chicken : public LameMob
{
public:

    Chicken(Vector3 position1, int number1);
    ~Chicken();

    void BoxCollision();
    void Update(CraftWorld *myWorld, Vector3 playerPos, float dt);
    void Render(Frustum &camFrustum, float dt);
    void TakeDamage(float damage, float power, float dt);
    void TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point);

    bool rotate1;
    int handItemId;

    // Set while the bird lays an egg; the play-state loops play the clip and clear it, the same
    // contract as playHurtSound / playSaySound on LameMob.
    bool playPlopSound;

private:

    int lastCommand;
    float rLegAngle;
    float lLegAngle;

    float headAngleY;

    float animLegAngle;
    float animLegAngleT;

    // Counts down in seconds to the next egg. Java uses rand.nextInt(6000) + 6000 ticks, which is
    // five to ten minutes, and only advances it for an adult.
    float eggTimer;

    // Chickens flap while airborne (Java ModelChicken drives the wing rotation from motionY).
    // wingAngle is the phase; it is only advanced while the bird is off the ground.
    float wingAngle;
};

#endif
