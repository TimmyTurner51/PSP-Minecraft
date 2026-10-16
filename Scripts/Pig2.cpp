#include "Pig2.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f

Pig::Pig(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Pig);
    debugName = "Pig";

    legs = Vector3(0,-6*PIXEL,0);

    SetCollideBox(Vector3(PIXEL*6,PIXEL*12,PIXEL*5),Vector3(PIXEL*6,PIXEL*6,PIXEL*5));
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
    breedItemId = Carrot::getID();

    UpdateBBox();
    myLight = 1.0f;

    lastCommand = 0;
    freezedTimerMax = 500.0f;

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::pigTexture);
}

Pig::~Pig()
{

}

void Pig::BoxCollision()
{
    for(int x = 0; x < (int)world->mPigs.size(); x++)
    {
        Pig *Other = world->mPigs[x];
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

void Pig::TakeDamage(float damage, float power, float dt)
{
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Pig::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Pig::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
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

            Pig* mate = InLove() ? FindKin(world->mPigs, true, 8.0f) : NULL;
            Pig* parent = IsBaby() ? FindKin(world->mPigs, false, 8.0f) : NULL;
            if(UpdatePassiveAI(dt, handItemId, mate, parent, &headAngleY) && mate != NULL && world->CanSpawnPassive())
            {
                world->SpawnPig((position.x + mate->position.x) * 0.5f, position.y + 0.3f, (position.z + mate->position.z) * 0.5f);
                world->mPigs.back()->SetAge(-1200.0f);
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

void Pig::Render(Frustum &camFrustum, float dt)
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
            ScePspFVector3 loc = {0,0,0};
            sceGumTranslate(&loc);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigBodyVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc2 = {PIXEL*10,PIXEL*2,0};
            sceGumTranslate(&loc2);
            if(IsBaby())
            {
                ScePspFVector3 babyHead = {1.6f,1.6f,1.6f};
                sceGumScale(&babyHead);
            }

            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigHeadVertices);

            sceGumPushMatrix();
            ScePspFVector3 locSnout = {PIXEL*4,0,0};
            sceGumTranslate(&locSnout);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigSnoutVertices);
            sceGumPopMatrix();

            sceGumPopMatrix();

            sceGumPushMatrix();
            ScePspFVector3 loc3 = {-PIXEL*5,-PIXEL*4,PIXEL*3};
            sceGumTranslate(&loc3);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigLegVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc4 = {PIXEL*5,-PIXEL*4,PIXEL*3};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigLegVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc5 = {-PIXEL*5,-PIXEL*4,-PIXEL*3};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigLegVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc6 = {PIXEL*5,-PIXEL*4,-PIXEL*3};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->pigLegVertices);
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