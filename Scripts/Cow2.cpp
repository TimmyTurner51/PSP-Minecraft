#include "Cow2.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f

Cow::Cow(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Cow);
    debugName = "Cow";

    legs = Vector3(0,-6*PIXEL,0);

    SetCollideBox(Vector3(PIXEL*7,PIXEL*17,PIXEL*7),Vector3(PIXEL*7,PIXEL*10,PIXEL*7));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    animLegAngle = 0.0f;
    animLegAngleT = 0.0f;

    headAngleY = 0.0f;

    rLegAngle = PI;
    lLegAngle = PI;

    mainAngle = (rand()%360)/180.0f*PI;
    deathAngle = 0.0f;

    senselessJumpsTimer = 0.0f;
    senselessJumpsTimerMax = 20.0f + (rand() % 11) * 3;
    senselessJumps = false;
    rotate1 = false;

    handItemId = -1;
    breedItemId = Wheat::getID();

    UpdateBBox();
    myLight = 1.0f;

    lastCommand = 0;
    freezedTimerMax = 500.0f;

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::cowTexture);
}

Cow::~Cow()
{

}

void Cow::BoxCollision()
{
    for(int x = 0; x < world->mCows.size(); x++)
    {
        Cow *TestCow2 = world->mCows[x];
        if(TestCow2->number != number && (TestCow2->position.x != position.x || TestCow2->position.z != position.z || TestCow2->position.y != position.y))
        {
            if(TestCow2->bBox.intersect(bBox))
            {
                float angle1 = ATAN2_fast(position.x-TestCow2->position.x,position.z-TestCow2->position.z); // super secret function

                TryToMove(Vector3(sinf(angle1),0.0f,cosf(angle1)),SPD*0.05f);
            }
        }
    }
}

void Cow::TakeDamage(float damage, float power, float dt)
{
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Cow::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Cow::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
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
                SetCollideBox(Vector3(PIXEL*7,PIXEL*6,PIXEL*7),Vector3(PIXEL*7,PIXEL*10,PIXEL*7));
                return;
            }

            UpdateBBox();
            UpdateMobTimers(dt, playerHitBox);

            // Cow-only: occasional random jumps while idle.
            if(killed == false)
            {
                senselessJumpsTimer += dt;
                if(senselessJumpsTimer >= senselessJumpsTimerMax)
                {
                    senselessJumpsTimer = 0.0f;
                    if(senselessJumps == true)
                    {
                        senselessJumps = false;
                        senselessJumpsTimerMax = 15.0f + (rand() % 21) * 1.5f;
                        if(rand() % 3 == 0)
                            senselessJumpsTimerMax = 60.0f + rand() % 80;
                    }
                    else
                    {
                        senselessJumps = true;
                        senselessJumpsTimerMax = 2.0f + (rand() % 21) / 10.0f;
                    }
                }
                if(senselessJumps && jumpReady)
                {
                    TryToJump();
                    jumpReady = false;
                }
            }

            Cow* mate = InLove() ? FindKin(world->mCows, true, 8.0f) : NULL;
            Cow* parent = IsBaby() ? FindKin(world->mCows, false, 8.0f) : NULL;
            if(UpdatePassiveAI(dt, handItemId, mate, parent, &headAngleY) && mate != NULL && world->CanSpawnPassive())
            {
                world->SpawnCow((position.x + mate->position.x) * 0.5f, position.y + 0.3f, (position.z + mate->position.z) * 0.5f);
                world->mCows.back()->SetAge(-1200.0f);
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
        }
        else
        {
            UpdateOutOfRangeFreeze(dt);
        }
    }
}

void Cow::Render(Frustum &camFrustum, float dt)
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
                ScePspFVector3 babyScale = {0.5f,0.5f,0.5f};
                sceGumScale(&babyScale);
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
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);

            sceGumPushMatrix();
            ScePspFVector3 loc = {0,PIXEL,0};
            sceGumTranslate(&loc);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowBodyVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc2 = {PIXEL*12,PIXEL*4,0};
            sceGumTranslate(&loc2);
            if(IsBaby())
            {
                ScePspFVector3 babyHead = {1.6f,1.6f,1.6f};
                sceGumScale(&babyHead);
            }

            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowHeadVertices);

            // horns drawing
            if(DistanceToPlayer() < 15.0f)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc52 = {PIXEL*0.5f,PIXEL*4,-PIXEL*4.56f};
                sceGumTranslate(&loc52);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);
                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowHornsVertices);
                sceGumPopMatrix();

                sceGumPushMatrix();
                ScePspFVector3 loc53 = {PIXEL*0.5f,PIXEL*4,PIXEL*4.55f};
                sceGumTranslate(&loc53);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);
                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowHornsVertices);
                sceGumPopMatrix();
            }

            sceGumPopMatrix();

            sceGumPushMatrix();
            ScePspFVector3 loc3 = {-PIXEL*6,-PIXEL*6,PIXEL*4};
            sceGumTranslate(&loc3);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowLegVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc4 = {PIXEL*6,-PIXEL*6,PIXEL*4};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowLegVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc5 = {-PIXEL*6,-PIXEL*6,-PIXEL*4};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowLegVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc6 = {PIXEL*6,-PIXEL*6,-PIXEL*4};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->cowLegVertices);
            sceGumPopMatrix();


            //sceGuDisable(GU_BLEND);
            //sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_DEPTH_TEST);
            sceGuDisable(GU_TEXTURE_2D);
            sceGuColor(0xFFFFFFFF);

            sceGumPopMatrix();

            RenderFire();
        }
    }
}