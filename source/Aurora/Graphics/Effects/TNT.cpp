#include <Aurora/Graphics/Effects/TNT.h>
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.1415926535897f
#define DEG_TO_RAD (PI / 180.0f)

// Java 1.7 EntityTNTPrimed / Explosion, converted from per-tick to per-second
namespace
{
    const float kGravity = 16.0f;            // 0.04 blocks/tick^2
    const float kAirDragLn = -0.40405f;      // ln(0.98) * 20 ticks
    const float kGroundFrictionLn = -7.1335f; // ln(0.7) * 20 ticks
    const float kWaterPush = 5.6f;           // 0.014 blocks/tick added each tick
    const float kHalf = 0.49f;               // 0.98 block box
    const float kExplosionReach = 8.0f;      // 2 * power 4
    // Player launch tuning. The blast throws the player with an impulse that falls off with distance
    // and is attenuated by anything solid in the way, exactly like the TNT-vs-TNT push above -- Java
    // pushes the player as just another entity. kLaunchGain is the vertical push at the centre of a
    // blast, in units of the player's jump velocity (4.6 blocks/s); ~6.5 throws you about 15 blocks,
    // which is the "water cannon" height. kPushGain is the sideways drift, in blocks per frame for the
    // 0.6s slide, kept small because that slide steps straight through TryToMove and would otherwise
    // tunnel a wall.
    const float kJumpUnit = 4.6f;            // StatePlay's JUMPVELOCITY
    const float kLaunchGain = 6.5f;
    const float kPushGain = 0.22f;
    // Water absorbs blast energy, so a player standing in it takes only a fraction of the damage while
    // still receiving the full impulse. That asymmetry is the whole trick behind the classic
    // water-bucket player cannon: it is what lets you be launched instead of killed.
    const float kWaterDamageScale = 0.2f;
}

TNT::TNT(CraftWorld* world, Vector3 position2, Vector3 velocity2)
{
    position = Vector3(0,0,0);
    velocity = Vector3(0,0,0);

    SetPosition(position2);
    SetVelociy(velocity2);

    // Ignition hop: 0.02 blocks/tick in a random direction, 0.2 blocks/tick up
    float hopAngle = (rand() % 360) * DEG_TO_RAD;
    velocity.x += -sinf(hopAngle) * 0.4f;
    velocity.z += -cosf(hopAngle) * 0.4f;
    velocity.y += 4.0f;

    onGround = false;
    toDestroy = false;

    blockScale = 1.0f;
    animationTime = 0.0f;
    timeToExplode = 4.0f; // 4 secs to explode

    TNTid = 110;
    myWorld = world;
    num = 0;
    explodeRadius = 2.75f;
    ignitedByPlayer = false;
}

TNT::~TNT()
{

}

float TNT::ExposureTo(Vector3 from, Vector3 center, float half)
{
    const float o = half * 0.8f;
    const float offs[9][3] = {{0,0,0},{-o,-o,-o},{o,-o,-o},{-o,-o,o},{o,-o,o},{-o,o,-o},{o,o,-o},{-o,o,o},{o,o,o}};
    int clear = 0;
    for(int r = 0; r < 9; r++)
    {
        float tx = center.x + offs[r][0];
        float ty = center.y + offs[r][1];
        float tz = center.z + offs[r][2];
        float dx = tx - from.x, dy = ty - from.y, dz = tz - from.z;
        float len = sqrtf(dx*dx + dy*dy + dz*dz);
        int steps = (int)(len / 0.25f) + 1;
        bool blocked = false;
        for(int s = 1; s < steps; s++)
        {
            float t = (float)s / (float)steps;
            if(myWorld->BlockSolid((int)floorf(from.x + dx*t), (int)floorf(from.y + dy*t), (int)floorf(from.z + dz*t)))
            {
                blocked = true;
                break;
            }
        }
        if(!blocked)
            clear++;
    }
    return clear / 9.0f;
}

// Java Explosion.doExplosionA entity push: impulse (1 - d/8) * exposure blocks/tick away
// from the blast, no same-axis exclusion (that is what made cannons fail).
void TNT::ActivateOtherTNTs()
{
    Vector3 origin(position.x, position.y + 0.061f, position.z);
    for(unsigned int j = 0; j < myWorld->mTNTs.size(); j++)
    {
        TNT* other = myWorld->mTNTs[j];
        if(other == NULL || other == this || other->toDestroy)
            continue;
        float dx = other->position.x - origin.x;
        float dy = other->position.y - origin.y;
        float dz = other->position.z - origin.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if(dist >= kExplosionReach || dist < 0.0001f)
            continue;
        float impact = (1.0f - dist / kExplosionReach) * ExposureTo(origin, other->position, kHalf);
        float k = impact * 20.0f / dist;
        other->velocity.x += dx * k;
        other->velocity.y += dy * k;
        other->velocity.z += dz * k;
    }
}

void TNT::HurtNearbyMobs(float dt)
{
    if(myWorld->mZombies.empty() == false)
    {
        for(unsigned int j = 0; j < myWorld->mZombies.size(); j++)
        {
            if(j < myWorld->mZombies.size())
            {
                Zombie* TestMob = myWorld->mZombies[j];
                if(TestMob != NULL)
                {
                    float dist = myWorld->FastDistance2d(abs(position.x-TestMob->position.x)*10,abs(position.z-TestMob->position.z)*10)/10.0f + abs(position.y-(TestMob->position.y-0.7))/2.0f;

                    if(dist < 5.3f)
                    {
                        TestMob->TakeDamageFromPointExplosion(30-(dist*4.7),0.75,dt,position);
                    }
                }
            }
        }
    }
    if(myWorld->mCreepers.empty() == false)
    {
        for(unsigned int j = 0; j < myWorld->mCreepers.size(); j++)
        {
            if(j < myWorld->mCreepers.size())
            {
                Creeper* TestMob = myWorld->mCreepers[j];
                if(TestMob != NULL)
                {
                    float dist = myWorld->FastDistance2d(abs(position.x-TestMob->position.x)*10,abs(position.z-TestMob->position.z)*10)/10.0f + abs(position.y-(TestMob->position.y-0.7))/2.0f;

                    if(dist < 5.3f)
                    {
                        TestMob->TakeDamageFromPointExplosion(30-(dist*4.7),0.75,dt,position);
                    }
                }
            }
        }
    }
    if(myWorld->mCows.empty() == false)
    {
        for(unsigned int j = 0; j < myWorld->mCows.size(); j++)
        {
            if(j < myWorld->mCows.size())
            {
                Cow* TestMob = myWorld->mCows[j];
                if(TestMob != NULL)
                {
                    float dist = myWorld->FastDistance2d(abs(position.x-TestMob->position.x)*10,abs(position.z-TestMob->position.z)*10)/10.0f + abs(position.y-(TestMob->position.y-0.7))/2.0f;

                    if(dist < 5.3f)
                    {
                        TestMob->TakeDamageFromPointExplosion(30-(dist*4.7),0.75,dt,position);
                    }
                }
            }
        }
    }
    for(unsigned int j = 0; j < myWorld->mPigs.size(); j++)
    {
        Pig* TestMob = myWorld->mPigs[j];
        if(TestMob != NULL)
        {
            float dist = myWorld->FastDistance2d(abs(position.x-TestMob->position.x)*10,abs(position.z-TestMob->position.z)*10)/10.0f + abs(position.y-(TestMob->position.y-0.7))/2.0f;

            if(dist < 5.3f)
            {
                TestMob->TakeDamageFromPointExplosion(30-(dist*4.7),0.75,dt,position);
            }
        }
    }
    if(myWorld->mSheeps.empty() == false)
    {
        for(unsigned int j = 0; j < myWorld->mSheeps.size(); j++)
        {
            if(j < myWorld->mSheeps.size())
            {
                Sheep* TestMob = myWorld->mSheeps[j];
                if(TestMob != NULL)
                {
                    float dist = myWorld->FastDistance2d(abs(position.x-TestMob->position.x)*10,abs(position.z-TestMob->position.z)*10)/10.0f + abs(position.y-(TestMob->position.y-0.7))/2.0f;

                    if(dist < 5.3f)
                    {
                        TestMob->TakeDamageFromPointExplosion(30-(dist*4.7),0.75,dt,position);
                    }
                }
            }
        }
    }
}

bool TNT::BoxHitsSolid(const Vector3& p)
{
    for(int c = 0; c < 8; c++)
    {
        float cx = p.x + ((c & 1) ? kHalf : -kHalf);
        float cy = p.y + ((c & 2) ? kHalf : -kHalf);
        float cz = p.z + ((c & 4) ? kHalf : -kHalf);
        if(myWorld->BlockSolid((int)floorf(cx), (int)floorf(cy), (int)floorf(cz)))
            return true;
    }
    return false;
}

// Axis-separated move like Java moveEntity: stop flush against the face, no bounce
void TNT::MoveAxis(int axis, float delta)
{
    if(delta == 0.0f)
        return;
    Vector3 p = position;
    float* comp = (axis == 0) ? &p.x : (axis == 1) ? &p.y : &p.z;
    *comp += delta;
    if(!BoxHitsSolid(p))
    {
        position = p;
        return;
    }
    Vector3 flush = position;
    float* fc = (axis == 0) ? &flush.x : (axis == 1) ? &flush.y : &flush.z;
    if(delta < 0.0f)
        *fc = floorf(*comp - kHalf) + 1.0f + kHalf + 0.001f;
    else
        *fc = floorf(*comp + kHalf) - kHalf - 0.001f;
    if(!BoxHitsSolid(flush))
        position = flush;
    if(axis == 0)
        velocity.x = 0.0f;
    else if(axis == 1)
    {
        if(delta < 0.0f)
            onGround = true;
        velocity.y = 0.0f;
    }
    else
        velocity.z = 0.0f;
}

void TNT::SetVelociy(Vector3 velocity2)
{
    velocity = velocity2;
}

void TNT::SetPosition(Vector3 position2)
{
    position = position2;
}

// Whether a charge is sitting in a liquid, which in Java means its blast breaks no blocks.
//
// Testing only the charge's own cell is not enough, and that is exactly why a "water-logged" cannon
// was still shattering its own redstone. Placing TNT into a pool *displaces* the water, so the cell is
// usually dry air by the time the fuse burns out -- flowing water only creeps back into it, and not at
// all when a pool is packed with charges, which is precisely how a cannon is built. So look at the
// volume the charge occupies and the cells around it instead: a charge inside a body of water is
// submerged, while a charge merely standing on a shoreline (one wet side) is not.
static bool ChargeIsSubmerged(CraftWorld* world, int x, int y, int z)
{
    block_t here = world->_GetBlock(x, y, z);
    if(world->BlockIsWater(here) || world->BlockIsLava(here))
    {
        return true;
    }
    // The charge is 0.98 tall and can straddle two cells, and a pool needs at least two layers of
    // water for a charge to fit inside it -- either of those is enough on its own.
    block_t above = world->_GetBlock(x, y + 1, z);
    if(world->BlockIsWater(above) || world->BlockIsLava(above))
    {
        return true;
    }
    block_t below = world->_GetBlock(x, y - 1, z);
    if(world->BlockIsWater(below) || world->BlockIsLava(below))
    {
        return true;
    }
    // A charge in a one-deep channel or surface layer has dry cells above and below but water all
    // around it, so require two wet sides rather than one -- a single wet side is a shoreline.
    const int hx[4] = {1,-1,0,0};
    const int hz[4] = {0,0,1,-1};
    int wetSides = 0;
    for(int d = 0; d < 4; d++)
    {
        block_t b = world->_GetBlock(x + hx[d], y, z + hz[d]);
        if(world->BlockIsWater(b) || world->BlockIsLava(b))
        {
            wetSides++;
        }
    }
    return wetSides >= 2;
}

void TNT::Update(float dt)
{
    if(position.x < 1 || position.x >= myWorld->worldLimit-1 || position.y < 1 || position.y >= myWorld->WORLD_HEIGHT-1 || position.z < 1 || position.z >= myWorld->worldLimit-1)
    {
        toDestroy = true;
    }

    if(toDestroy)
    {
        return;
    }

    if(dt > 0.1f)
        dt = 0.1f;

    int cellX = (int)floorf(position.x);
    int cellY = (int)floorf(position.y);
    int cellZ = (int)floorf(position.z);
    bool inWater = myWorld->BlockIsWater(myWorld->_GetBlock(cellX, cellY, cellZ));
    if(inWater)
    {
        Vector3 flow = myWorld->WaterFlowVector(cellX, cellY, cellZ);
        velocity.x += flow.x * kWaterPush * dt;
        velocity.z += flow.z * kWaterPush * dt;
    }

    velocity.y -= kGravity * dt;

    // Sub-step so a launched block cannot pass through a 1-block wall in one frame
    float fastest = fabsf(velocity.x);
    if(fabsf(velocity.y) > fastest) fastest = fabsf(velocity.y);
    if(fabsf(velocity.z) > fastest) fastest = fabsf(velocity.z);
    int steps = 1 + (int)(fastest * dt / 0.4f);
    if(steps > 8)
        steps = 8;
    float stepDt = dt / steps;
    onGround = false;
    for(int s = 0; s < steps; s++)
    {
        MoveAxis(1, velocity.y * stepDt);
        MoveAxis(0, velocity.x * stepDt);
        MoveAxis(2, velocity.z * stepDt);
    }
    if(!onGround)
        onGround = BoxHitsSolid(Vector3(position.x, position.y - 0.02f, position.z));

    float drag = expf(kAirDragLn * dt);
    velocity.x *= drag;
    velocity.y *= drag;
    velocity.z *= drag;
    if(onGround)
    {
        float friction = expf(kGroundFrictionLn * dt);
        velocity.x *= friction;
        velocity.z *= friction;
    }

    // timers
    timeToExplode -= dt;
    animationTime += dt*2.55f;

    if(timeToExplode < 0.25f) // if time before explosion less than 1/4 of second
    {
        blockScale += dt*0.5f; // our tnt block begin to grow
    }

    if(timeToExplode <= 0.0f && toDestroy == false) // if our time has ended
    {
        ActivateOtherTNTs();
        HurtNearbyMobs(dt);

        // Launch the player, Java Explosion style: the impulse falls off with distance and is blocked
        // by solid blocks, and the damage is shielded by water. A player surrounded by TNT in water
        // therefore goes up instead of dying -- the player cannon.
        Vector3 blastOrigin(position.x, position.y + 0.061f, position.z);
        Vector3 playerCenter(myWorld->playerPos.x, myWorld->playerPos.y - 0.7f, myWorld->playerPos.z);
        float pdx = playerCenter.x - blastOrigin.x;
        float pdy = playerCenter.y - blastOrigin.y;
        float pdz = playerCenter.z - blastOrigin.z;
        float pdist = sqrtf(pdx*pdx + pdy*pdy + pdz*pdz);

        if(pdist < kExplosionReach)
        {
            // Same 9-ray trace the TNT-vs-TNT push uses, so hiding behind a block shields you from both
            // the damage and the launch. Water is not solid and so does not block it -- on purpose.
            float exposure = ExposureTo(blastOrigin, playerCenter, kHalf);
            float falloff = 1.0f - pdist / kExplosionReach;
            float strength = falloff * exposure;

            if(strength > 0.02f)
            {
                float damage = (32.0f - pdist * 5.6f) * exposure;
                if(damage < 0.0f)
                    damage = 0.0f;

                // Check the chest and the legs: the cannon is built so the player sits in water, and
                // either body half being submerged is enough to shield.
                bool shielded = myWorld->PlayerInWater(playerCenter) ||
                                myWorld->PlayerInWater(Vector3(myWorld->playerPos.x, myWorld->playerPos.y - 1.2f, myWorld->playerPos.z));
                if(shielded)
                    damage *= kWaterDamageScale;

                myWorld->kickedStart = true;
                myWorld->kickedBy = 2;
                myWorld->kickedAngle = ATAN2_fast(blastOrigin.x - playerCenter.x, blastOrigin.z - playerCenter.z);
                myWorld->kickedVelY = (0.4f + kLaunchGain * strength) * kJumpUnit;
                myWorld->kickedPush = 0.04f + kPushGain * strength;
                myWorld->kickedDamage = (int)damage;
            }
        }
        // Java: a blast from TNT in any water/lava (source or flowing) breaks no blocks. The charge is
        // usually dry in its own cell by now (see ChargeIsSubmerged), so test the surrounding volume
        // rather than just this one cell.
        if(!ChargeIsSubmerged(myWorld, cellX, cellY, cellZ))
        {
            myWorld->BuildExplodeSphere(explodeRadius,position.x,position.y,position.z); // destroy blocks in radius of 3 meters

            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x+4,position.y+4,position.z+4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x-4,position.y+4,position.z+4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x-4,position.y+4,position.z-4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x+4,position.y+4,position.z-4)));

            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x+4,position.y-4,position.z+4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x-4,position.y-4,position.z+4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x-4,position.y-4,position.z-4)));
            myWorld->AddChunkToFastUpdate(myWorld->getChunkId(Vector3(position.x+4,position.y-4,position.z-4)));
        }
        toDestroy = true; // destroy TNT entity

    }
}

void TNT::Render()
{
    if(toDestroy)
    {
        return;
    }
    float myLight = myWorld->BlockFinalLight(position.x,position.y,position.z) * 1.35f; // fetching the light of position

    if(myLight > 1.0f)
    {
        myLight = 1.0f;
    }

    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);

    sceGumPushMatrix();

    ScePspFVector3 loc = {position.x,position.y,position.z};
    sceGumTranslate(&loc);

    ScePspFVector3 sca = {blockScale,blockScale,blockScale};
    sceGumScale(&sca);

    sceGumRotateX(0.0f);
    sceGumRotateY(0.0f);
    sceGumRotateZ(0.0f);

    (int)animationTime % 2 == 0 ? myWorld->drawCubes(TNTid,myLight) : myWorld->drawWhiteCube(TNTid,myLight); // our animation magic

    sceGumPopMatrix();

    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
}