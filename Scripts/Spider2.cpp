#include "Spider2.h"
#include "Items.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f
#include "CraftWorld2.h"

// The rig. The mesh is authored with the ground at mesh Y 0 -- the eight leg tips land within two
// pixels of it -- so Render drops the whole thing by the collider's foot offset and the bone pivots
// below are then read straight off the generator's report (spider_gen.py):
//
//     abdomen (-9, 9, 0)   thorax (0, 9, 0)   head (3, 9, 0)   legs (y 9, x -2..+1, z +-4)
//
// The leg table is vanilla ModelSpider.setRotationAngles, read out of the 1.7.3 bytecode (class
// `bjb`) rather than guessed: leg1..8 sit at vanilla x -4/+4 and z 2,1,0,-1 (the pairs alternate
// left, right as vanilla builds them), the base rotateAngleZ is +-pi/4 outside and +-0.74*pi/4 in
// the middle, and the base rotateAngleY fans the four positions by 2h, h, -h, -2h (h = pi/8) so the
// legs splay front and back. The walk terms are
//
//     yaw  += c[phase] * side      c[k] = -cos(2t + off[k]) * 0.4 * amount
//     roll += s[phase] * side      s[k] = |sin(t + off[k])| * 0.4 * amount
//
// with off = {0, pi, pi/2, 3pi/2} rear to front and side = +1 on the vanilla -4 pair. That is the
// classic eight-legged gait: four legs planted, four lifted, the planted ones swapped every half
// cycle.
//
// Sign conventions, all verified against the bytecode and against the mesher's axis mapping
// (mesh X = -vanilla z, mesh Y = -vanilla y, mesh Z = vanilla x): vanilla rotateAngleZ maps to mesh
// RotateX and vanilla rotateAngleY to mesh RotateY with the same sign, and vanilla rotateAngleX maps
// to RotateZ negated. Order matters and vanilla applies Z, then Y, then X, which is the same order
// here with the axes renamed.
struct SpiderLegBone
{
    float pivotX;   // mesh px, fore-aft: -vanilla z
    float pivotZ;   // mesh px, width: vanilla x
    float baseRoll; // mesh RotateX, = vanilla rotateAngleZ
    float baseYaw;  // mesh RotateY, = vanilla rotateAngleY
    int phase;      // 0..3, rear to front
    float side;     // +1 = the vanilla -4 pair (mesh -Z, spiderLegVertices)
};

#define SPIDER_H (PI / 8.0f)

static const SpiderLegBone SPIDER_LEGS[8] =
{
    { -2.0f, -4.0f, -(PI / 4.0f),           +2.0f * SPIDER_H, 0, +1.0f },
    { -2.0f, +4.0f, +(PI / 4.0f),           -2.0f * SPIDER_H, 0, -1.0f },
    { -1.0f, -4.0f, -0.74f * (PI / 4.0f),   +1.0f * SPIDER_H, 1, +1.0f },
    { -1.0f, +4.0f, +0.74f * (PI / 4.0f),   -1.0f * SPIDER_H, 1, -1.0f },
    {  0.0f, -4.0f, -0.74f * (PI / 4.0f),   -1.0f * SPIDER_H, 2, +1.0f },
    {  0.0f, +4.0f, +0.74f * (PI / 4.0f),   +1.0f * SPIDER_H, 2, -1.0f },
    {  1.0f, -4.0f, -(PI / 4.0f),           -2.0f * SPIDER_H, 3, +1.0f },
    {  1.0f, +4.0f, +(PI / 4.0f),           +2.0f * SPIDER_H, 3, -1.0f },
};

static inline float spider_leg_cos(int phase, float t, float amount)
{
    static const float off[4] = { 0.0f, PI, PI * 0.5f, PI * 1.5f };
    return -cosf(2.0f * t + off[phase]) * 0.4f * amount;
}

static inline float spider_leg_sin(int phase, float t, float amount)
{
    static const float off[4] = { 0.0f, PI, PI * 0.5f, PI * 1.5f };
    return fabsf(sinf(t + off[phase])) * 0.4f * amount;
}

Spider::Spider(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Spider);
    debugName = "Spider";

    // 1.4 x 0.9 blocks in vanilla. The body centre is the rig's pivot height, 9 px up (colliderD.y),
    // and the head reaches 5.4 px above it, which is the 0.9 the profile's fire box already claims.
    // The width is deliberately below vanilla's half-width: a 1.4 block box can never fit through a
    // one block gap, and the corner probes below would leave the spider wedged in every cave mouth.
    legs = Vector3(0,-PIXEL*7,0);

    SetCollideBox(Vector3(PIXEL*6,PIXEL*9,PIXEL*6),Vector3(PIXEL*6,PIXEL*5.4f,PIXEL*6));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    animLegAngleT = 0.0f;
    legSwingAmount = 0.0f;
    stepDistance = 0.7f;

    headAngleY = 0.0f;
    headAngleZ = 0.0f;

    climbing = false;

    UpdateBBox();

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::spiderTexture);

    UpdateBBox();
    myLight = 1.0f;
}

Spider::~Spider()
{

}

void Spider::BoxCollision()
{
    for(int x = 0; x < (int)world->mSpiders.size(); x++)
    {
        Spider *Other = world->mSpiders[x];
        if(Other->number != number && (Other->position.x != position.x || Other->position.z != position.z || Other->position.y != position.y))
        {
            if(Other->bBox.intersect(bBox))
            {
                float angle1 = ATAN2_fast(position.x-Other->position.x,position.z-Other->position.z);
                TryToMove(Vector3(sinf(angle1),0.0f,cosf(angle1)),SPD*0.05f);
            }
        }
    }
}

void Spider::TakeDamage(float damage, float power, float dt)
{
    // Spiders wear no armour, so this is the zombie's path minus the armour points.
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Spider::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Spider::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
{
    if(!BeginMobUpdate(myWorld, playerPos, dt))
        return;

    UpdateDeathPose(dt);

    if(toDestroy == false)
    {
        BoundingBox playerHitBox = BoundingBox(Vector3(playerPos.x - 0.73f,playerPos.y - 1.65f,playerPos.z - 0.73f),Vector3(playerPos.x + 0.73f,playerPos.y + 0.2f,playerPos.z + 0.73f));

        if(fastDistanceToPlayer < world->mainOptions.horizontalViewDistance*14)
        {
            freezedTimer = 0.0f;
            if(CheckJustKilled())
            {
                return;
            }

            UpdateBBox();
            UpdateMobTimers(dt, playerHitBox);

            bool wantAttack = false;
            // Did the AI actually get anywhere this frame? This is the only honest reading of
            // "I am pressed against something", because neither existing signal works:
            //
            //   `moving` is set by TryToMove only when a step *succeeded*, and UpdateHostileAI
            //   then sets it true unconditionally, so it cannot mean "blocked". Worse,
            //   UpdateWander clears it at the top of every frame and never sets it, so a wandering
            //   spider pressed against a wall reads moving == false even while walking happily.
            //   In creative the mobs never chase, so wandering is the ONLY state a spider is ever
            //   in when it is being tested -- which is why the climb looked completely dead.
            //
            //   A forward probe cannot be made to work either. TryToMove refuses to advance while
            //   within (speed + 0.2) + colliderD.x of a wall -- 0.62 blocks for the spider -- so
            //   that is where it comes to rest, and a probe has to reach further than that to see
            //   the wall at all. The 0.55 probe used here was shorter than the stall distance, so
            //   it sampled the spider's own empty cell no matter what it was pushed against.
            //
            // Measuring the position brackets the AI call and is true for both paths, and
            // "tried to walk and did not move" is exactly Java's isCollidedHorizontally().
            Vector3 beforeAI = position;
            UpdateHostileAI(dt, &wantAttack);
            const float movedXZ = fabsf(position.x - beforeAI.x) + fabsf(position.z - beforeAI.z);
            const bool walkBlocked = (movedXZ < 0.0005f);

            // Melee kick (survival only; UpdateHostileAI already gated creative). Vanilla
            // EntitySpider does a plain 2.0 damage hit at 1.7.3 -- the poison is the cave spider.
            if(wantAttack && killed == false)
            {
                if(nearToPlayer == false)
                    kickTimer = 0.95f;
                nearToPlayer = true;
                kickTimer += dt;
                if(kickTimer >= profile.attackCooldown)
                {
                    Vector3 checkPos = (playerPos-position)*0.5f;
                    Vector3 checkPos2 = (playerPos-position)*0.75f;
                    Vector3 checkPos3 = (playerPos-position)*0.25f;
                    if(world->BlockSolid(position.x+checkPos.x,position.y+checkPos.y,position.z+checkPos.z) == false &&
                       world->BlockSolid(position.x+checkPos2.x,position.y+checkPos2.y,position.z+checkPos2.z) == false &&
                       world->BlockSolid(position.x+checkPos3.x,position.y+checkPos3.y,position.z+checkPos3.z) == false)
                    {
                        world->kickedStart = true;
                        world->kickedAngle = ATAN2_fast(position.x-myWorld->playerPos.x,position.z-myWorld->playerPos.z);
                        world->kickedBy = 1;
                        world->kickedDamage = (int)profile.attackDamage;
                        kickTimer = 0.0f;
                    }
                }
            }

            // Look at the player while chasing. Vanilla ModelSpider puts the head on rotateAngleY
            // and the pitch on rotateAngleX, both clamped the same way the zombie's are.
            if(angred == true && kicked == false)
            {
                float playerAngle = ATAN2_fast(position.x-relPlayerPos.x,position.z-relPlayerPos.z)+PI;
                headAngleY = playerAngle-mainAngle;
                headAngleZ = (relPlayerPos.y-position.y+0.45f)*0.25f;
                if(headAngleZ > PI/4.0f) headAngleZ = PI/4.0f;
                if(headAngleZ < -PI/4.0f) headAngleZ = -PI/4.0f;
            }
            else if(kicked == false)
            {
                headAngleY = 0.0f;
                headAngleZ = 0.0f;
            }

            UpdateKnockbackMove(dt);

            // The eight legs are driven straight off the shared phase, so animLegAngle itself is
            // not used; UpdateLegAnim is still called because it owns the phase and the settle.
            {
                float unusedLegAngle = 0.0f;
                UpdateLegAnim(dt, &animLegAngleT, &unusedLegAngle);
            }
            {
                float target = (moving == true && killed == false) ? 1.0f : 0.0f;
                float rate = 8.0f * dt;
                if(rate > 1.0f) rate = 1.0f;
                legSwingAmount += (target - legSwingAmount) * rate;
            }

            UpdateMobPhysics(dt);

            // Java EntitySpider.isOnLadder(): it climbs whatever wall it is pressed against, which
            // is why it can follow the player up a cliff face or out of a cave. The gate is
            // "wants to walk, and did not get anywhere" -- see the measurement above for why
            // neither `moving` nor a forward probe can be asked this question. (-1,-1,-1) is
            // UpdateWander's "stand and look around" idle and is deliberately not walking, so a
            // spider that has nowhere to be does not climb the walls it happens to be near.
            //
            // Note there is deliberately no walkingOnGround requirement: the foot clamp is skipped
            // while climbing (LameMob::UpdateMobPhysics), so as soon as the spider leaves the
            // ground its feet are in air and walkingOnGround goes false on the very first frame --
            // requiring it would abort the climb immediately.
            {
                const bool standingAndLooking = (hauntingPos.x == -1.0f && hauntingPos.z == -1.0f);
                const bool wanderWalking = (hauntingPos.x != 0.0f && hauntingPos.z != 0.0f) && !standingAndLooking;
                const bool walkIntent = killed == false && kicked == false && (angred == true || wanderWalking);

                climbing = false;
                if(walkIntent == true && walkBlocked == true)
                    climbing = true;

                if(climbing == true)
                {
                    position.y += 1.1f * dt;
                    velocity.y = 0.0f;
                    walkingOnGround = false;
                }
            }

            // Footfall clips. Java's spider plays mob.spider.step from moveEntity's step
            // accumulator, so this fires on distance walked rather than on a timer.
            if(moving == true && walkingOnGround == true && climbing == false && killed == false)
            {
                stepDistance += profile.spd * dt;
                if(stepDistance >= 1.4f)
                {
                    stepDistance = 0.0f;
                    playStepSound = true;
                }
            }

            // Vanilla spiders take no fall damage, and they are always about to fall off something.
            fallDistance = 0.0f;
        }
        else
        {
            UpdateOutOfRangeFreeze(dt);
        }
    }
}

// The head sits on its own bone just in front of the thorax, so it takes the yaw and pitch; the
// thorax and abdomen are rigid. Vanilla renders the whole model a second time with
// entity/spider_eyes.png on an additive pass, which on the PSP is two extra draw calls (the eyes
// are only painted on the head and the front of the thorax) at full brightness.
void Spider::DrawEyes()
{
    int eyesTexture = TextureHelper::Instance()->GetTexture(TextureHelper::spiderEyesTexture);
    if(eyesTexture <= 0 || eyesTexture == mainTexture)
        return;

    TextureManager::Instance()->SetTextureModeulate(eyesTexture);
    sceGuColor(GU_COLOR(1.0f,1.0f,1.0f,1.0f));

    sceGumPushMatrix();
    ScePspFVector3 locNeck = {0,PIXEL*9.0f,0};
    sceGumTranslate(&locNeck);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->spiderNeckVertices);
    sceGumPopMatrix();

    sceGumPushMatrix();
    ScePspFVector3 locHead = {PIXEL*3.0f,PIXEL*9.0f,0};
    sceGumTranslate(&locHead);
    sceGumRotateY(headAngleY);
    sceGumRotateZ(-headAngleZ);
    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->spiderHeadVertices);
    sceGumPopMatrix();

    TextureManager::Instance()->SetTextureModeulate(mainTexture);
    sceGuColor(GU_COLOR(myLight,myLight,myLight,1.0f));
}

void Spider::DrawLegs()
{
    for(int i = 0; i < 8; i++)
    {
        const SpiderLegBone& b = SPIDER_LEGS[i];
        float roll = b.baseRoll + spider_leg_sin(b.phase, animLegAngleT, legSwingAmount) * b.side;
        float yaw  = b.baseYaw  + spider_leg_cos(b.phase, animLegAngleT, legSwingAmount) * b.side;

        sceGumPushMatrix();
        ScePspFVector3 loc = {PIXEL*b.pivotX, PIXEL*9.0f, PIXEL*b.pivotZ};
        sceGumTranslate(&loc);
        sceGumRotateX(roll);
        sceGumRotateY(yaw);
        // side +1 is the vanilla -4 pair, whose box grows toward -Z
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0,
                        b.side > 0.0f ? world->spiderLegVertices : world->spiderLegLeftVertices);
        sceGumPopMatrix();
    }
}

void Spider::Render(Frustum &camFrustum, float dt)
{
    if(toDestroy == false)
    {
        if(fastDistanceToPlayer < world->mainOptions.horizontalViewDistance*14 && camFrustum.BoxInFrustum(bBox) == Frustum::Intersects)
        {
            sceGumPushMatrix();

            ScePspFVector3 loc22 = {position.x,position.y,position.z};
            sceGumTranslate(&loc22);

            sceGumRotateZ(0.0f);
            sceGumRotateY(mainAngle-PI/2);

            if(killed == true)
            {
                sceGumRotateX(deathAngle);
            }
            else
            {
                sceGumRotateX(0.0f);
            }

            if(IsBaby())
            {
                ScePspFVector3 spiderScale = {0.5f,0.5f,0.5f};
                sceGumScale(&spiderScale);
            }

            updateLightTimer += dt;
            if(updateLightTimer > 0.25f)
            {
                updateLightTimer = 0.0f;
                myLight = world->BlockFinalLight(position.x,position.y+1,position.z);
            }

            if((damaged == true && damagedTimer > 0.25f) || killed == true)
            {
                sceGuColor(GU_COLOR(0.8f,0.1,0.1,1.0f));
            }
            else
            {
                sceGuColor(GU_COLOR(myLight,myLight,myLight,1.0f));
            }

            TextureManager::Instance()->SetTextureModeulate(mainTexture);

            sceGuEnable(GU_TEXTURE_2D);
            sceGuEnable(GU_DEPTH_TEST);
            // The eye pass samples spider_eyes.png, which is transparent everywhere the eyes are
            // not, so alpha test is on for the whole mob -- and with it on, a near face that is
            // discarded would take the far face with it unless culling is off too (see Chicken2).
            sceGuAlphaFunc(GU_GREATER, 0.0f, 0xff);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDisable(GU_CULL_FACE);

            // Drop the rig so the mesh's ground plane (Y 0) is where the physics says the ground is.
            sceGumPushMatrix();
            ScePspFVector3 locGround = {0,-PIXEL*9.0f,0};
            sceGumTranslate(&locGround);

            // abdomen
            sceGumPushMatrix();
            ScePspFVector3 locBody = {PIXEL*-9.0f,PIXEL*9.0f,0};
            sceGumTranslate(&locBody);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->spiderBodyVertices);
            sceGumPopMatrix();

            // thorax
            sceGumPushMatrix();
            ScePspFVector3 locNeck = {0,PIXEL*9.0f,0};
            sceGumTranslate(&locNeck);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->spiderNeckVertices);
            sceGumPopMatrix();

            // head: vanilla rotateAngleY then rotateAngleX, which is mesh RotateY then -RotateZ
            sceGumPushMatrix();
            ScePspFVector3 locHead = {PIXEL*3.0f,PIXEL*9.0f,0};
            sceGumTranslate(&locHead);
            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(-headAngleZ);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->spiderHeadVertices);
            sceGumPopMatrix();

            DrawLegs();

            DrawEyes();

            sceGumPopMatrix();

            sceGuDisable(GU_DEPTH_TEST);
            sceGuDisable(GU_TEXTURE_2D);
            // hand the shared state back the way the other mobs expect to find it
            sceGuDisable(GU_ALPHA_TEST);
            sceGuEnable(GU_CULL_FACE);
            sceGuColor(0xFFFFFFFF);

            sceGumPopMatrix();

            RenderFire();
        }
    }
}
