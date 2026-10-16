#include "Chicken2.h"
#include "Items.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f

// The chicken mesh is authored with the body centre at the origin and the feet 8 px below it,
// which reads as a big bird, so the whole thing is drawn at three-quarter size. The collider is
// stated in the same units: 8 px of mesh leg * 0.75 = 6 px, which is the foot offset below.
// Chicken is 0.4 x 0.7 blocks in vanilla; this comes out at 0.375 x 0.69.
#define CHICKEN_SCALE 0.75f

Chicken::Chicken(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Chicken);
    debugName = "Chicken";

    legs = Vector3(0,-3*PIXEL,0);

    SetCollideBox(Vector3(PIXEL*3,PIXEL*6,PIXEL*2),Vector3(PIXEL*3,PIXEL*5,PIXEL*2));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    animLegAngle = 0.0f;
    animLegAngleT = 0.0f;

    headAngleY = 0.0f;

    // The legs hang down from their pivot, so no flip. A PI rotation would have swung them up and
    // mirrored their texture, which is what put the sole at the knee.
    rLegAngle = 0.0f;
    lLegAngle = 0.0f;

    wingAngle = 0.0f;

    playPlopSound = false;
    // Java seeds this once so a pen of birds does not all lay on the same tick.
    eggTimer = 300.0f + (rand()%300);

    mainAngle = (rand()%360)/180.0f*PI;
    deathAngle = 0.0f;

    rotate1 = false;

    handItemId = -1;
    breedItemId = WheatSeeds::getID();

    UpdateBBox();
    myLight = 1.0f;

    lastCommand = 0;
    freezedTimerMax = 500.0f;

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::chickenTexture);
}

Chicken::~Chicken()
{
}

void Chicken::BoxCollision()
{
    for(int x = 0; x < (int)world->mChickens.size(); x++)
    {
        Chicken *Other = world->mChickens[x];
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

void Chicken::TakeDamage(float damage, float power, float dt)
{
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Chicken::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Chicken::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
{
    if(!BeginMobUpdate(myWorld, playerPos, dt))
        return;

    UpdateDeathPose(dt);

    if(toDestroy == false)
    {
        BoundingBox playerHitBox = BoundingBox(Vector3(playerPos.x - 0.35f,playerPos.y - 1.65f,playerPos.z - 0.35f),Vector3(playerPos.x + 0.35f,playerPos.y + 0.2f,playerPos.z + 0.35f));

        if(fastDistanceToPlayer < world->mainOptions.horizontalViewDistance*14+5)
        {
            freezedTimer = 0.0f;
            if(CheckJustKilled())
            {
                SetCollideBox(Vector3(PIXEL*4,PIXEL*3,PIXEL*4),Vector3(PIXEL*4,PIXEL*6,PIXEL*4));
                return;
            }

            UpdateBBox();
            UpdateMobTimers(dt, playerHitBox);

            Chicken* mate = InLove() ? FindKin(world->mChickens, true, 8.0f) : NULL;
            Chicken* parent = IsBaby() ? FindKin(world->mChickens, false, 8.0f) : NULL;
            if(UpdatePassiveAI(dt, handItemId, mate, parent, &headAngleY) && mate != NULL && world->CanSpawnPassive())
            {
                world->SpawnChicken((position.x + mate->position.x) * 0.5f, position.y + 0.3f, (position.z + mate->position.z) * 0.5f);
                world->mChickens.back()->SetAge(-1200.0f);
                SpawnHearts(3);
            }

            if(kicked == true || panicTimer > 0.0f || InLove() || handItemId == breedItemId)
            {
                checkCollisionTimer += dt;
                if(checkCollisionTimer >= 0.1f)
                {
                    BoxCollision();
                    checkCollisionTimer = 0.0f;
                }
            }

            UpdateKnockbackMove(dt);
            UpdateLegAnim(dt, &animLegAngleT, &animLegAngle);
            UpdateMobPhysics(dt);

            // An adult hen lays an egg every five to ten minutes. Java drops it at the bird's feet
            // as an item, which is the only way to get an egg without killing something.
            if(IsBaby() == false)
            {
                eggTimer -= dt;
                if(eggTimer <= 0.0f)
                {
                    world->DropThis(Egg::getID(),1,true,Vector3(position.x,position.y,position.z));
                    playPlopSound = true;
                    eggTimer = 300.0f + (rand()%300);
                }
            }

            // Java ModelChicken beats its wings from the vertical motion, which in practice means
            // they only move while the bird is off the ground.
            if(walkingOnGround == true || killed == true)
                wingAngle = 0.0f;
            else
                wingAngle += dt * 16.0f;
        }
        else
        {
            UpdateOutOfRangeFreeze(dt);
        }
    }
}

void Chicken::Render(Frustum &camFrustum, float dt)
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

            // downsized bird; the collider in the ctor already accounts for this factor
            {
                ScePspFVector3 birdScale = {CHICKEN_SCALE,CHICKEN_SCALE,CHICKEN_SCALE};
                if(IsBaby())
                {
                    birdScale.x *= 0.5f;
                    birdScale.y *= 0.5f;
                    birdScale.z *= 0.5f;
                }
                sceGumScale(&birdScale);
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
            // The chicken is the one mob whose skin has unpainted texels: the leg net at (26,0) has
            // only its sole and a back stripe drawn, and the front/left/right of each leg are meant to
            // be see-through so the yellow back face reads as the leg. Two pieces of state have to line
            // up for that to work, and vanilla has both:
            //
            //   * alpha test ON, threshold 0. With it off the transparent texels draw as their RGB,
            //     which is black -- that is what showed up in-game as solid black legs.
            //   * backface culling OFF. Vanilla does not cull entity models, so when a near face is
            //     discarded you see straight through to the far face. With culling left on (the global
            //     default) the far face is wound away from the camera and gets culled too, so the leg
            //     disappears from the front entirely and only reappears from behind.
            //
            // Both are restored at the end of Render so the other mobs, whose skins are fully opaque,
            // keep the state they expect.
            sceGuAlphaFunc(GU_GREATER, 0.0f, 0xff);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDisable(GU_CULL_FACE);

            // body
            sceGumPushMatrix();
            ScePspFVector3 locBody = {0,0,0};
            sceGumTranslate(&locBody);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->chickenBodyVertices);
            sceGumPopMatrix();

            // head, with the beak and wattle baked in. Vanilla puts the head, bill and chin bones
            // all on setRotationPoint(0, 15, -4) -- 4 px in front of the body's own pivot at
            // (0, 16, 0) and 1 px above it -- so the whole group pivots there. In mesh space that
            // offset is (-dz, -dy, dx) = (+4, +1, 0).
            sceGumPushMatrix();
            ScePspFVector3 locHead = {PIXEL*4,PIXEL*1,0};
            sceGumTranslate(&locHead);
            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(0.0f);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 108, 0, world->chickenHeadVertices);
            sceGumPopMatrix();

    // two legs from one mesh, swinging out of phase. The mesh is authored hanging down from its
    // pivot, the way vanilla writes it, so it needs no flip -- a PI rotation here would mirror the
    // texture and put the flat sole at the knee. The pivot sits on the body's underside and the
    // legs end up 8 px down, which at the 0.75 draw scale is the collider's 6 px foot offset.
    for(int side = 0; side < 2; side++)
    {
        ScePspFVector3 locLeg = {PIXEL*0.5f,-PIXEL*3, side == 0 ? -PIXEL*1.5f : PIXEL*1.5f};
        sceGumPushMatrix();
        sceGumTranslate(&locLeg);
        sceGumRotateX(0.0f);
        sceGumRotateY(0.0f);
        sceGumRotateZ(side == 0 ? (rLegAngle - animLegAngle) : (lLegAngle + animLegAngle));
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->chickenLegVertices);
        sceGumPopMatrix();
    }

            // wings, against the sides. The pivot is at the shoulder -- mesh Y 0, 3 px above the
            // body's centre -- so a rotation about X swings the wing outward and up. wingAngle only
            // advances while the bird is off the ground.
            {
                float flap = 0.0f;
                if(wingAngle > 0.0f)
                    flap = 0.25f + 0.45f * (0.5f + 0.5f * sinf(wingAngle));

                for(int side = 0; side < 2; side++)
                {
                    // side 0 is the +Z wing, which swings out on a negative X rotation
                    ScePspFVector3 locWing = {0,PIXEL*3, side == 0 ? PIXEL*3.5f : -PIXEL*3.5f};
                    sceGumPushMatrix();
                    sceGumTranslate(&locWing);
                    sceGumRotateX(side == 0 ? -flap : flap);
                    sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->chickenWingVertices);
                    sceGumPopMatrix();
                }
            }

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
