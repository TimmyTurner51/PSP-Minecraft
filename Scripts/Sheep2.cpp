#include "Sheep2.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f

Sheep::Sheep(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Sheep);
    debugName = "Sheep";

    legs = Vector3(0,-4*PIXEL,0); // sheep legs sit higher than cow

    SetCollideBox(Vector3(PIXEL*7,PIXEL*16,PIXEL*7),Vector3(PIXEL*7,PIXEL*9,PIXEL*7));
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
    sheared = false;

    handItemId = -1;
    breedItemId = Wheat::getID();
    myLight = 1.0f;
    furTime = 60+(rand()%5)*30;

    color = 14;
    int colorRandom = rand() % 100;
    if(colorRandom > 80)
    {
        switch(rand() % 3)
        {
            case 0 : color = 1; break;
            case 1 : color = 13; break;
            case 2 : color = 0; break;
        }
        if(colorRandom  > 93)
        {
            color = 6;
        }
        if(colorRandom  > 97)
        {
            color = 3;
        }
    }

    SetFurColor(color);
    UpdateBBox();

    lastCommand = 0;
    freezedTimerMax = 600.0f;

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::sheepTexture);
    furTexture = TextureHelper::Instance()->GetTexture(TextureHelper::sheepFurTexture);
}

Sheep::~Sheep()
{

}

void Sheep::FetchFurColor()
{
    switch(color)
    {
        case 0 :  furColor = Vector3(0.19f,0.16f,0.16f); break;  // black
        case 1 :  furColor = Vector3(0.38f,0.38f,0.38f); break;  // gray
        case 2 :  furColor = Vector3(0.73f,0.2f,0.2f); break; // red
        case 3 :  furColor = Vector3(1,0.67f,0.67f); break; // pink
        case 4 :  furColor = Vector3(0.19f,0.38f,0.1f); break; // dark green
        case 5 :  furColor = Vector3(0.19f,0.77f,0.13f); break; // green
        case 6 :  furColor = Vector3(0.35f*1.1f,0.20f*1.1f,0.13f*1.1f); break; // brown
        case 7 :  furColor = Vector3(0.73f,0.73f,0.1f); break; // yellow
        case 8 :  furColor = Vector3(0.24f,0.24f,0.68f); break; // blue
        case 9 :  furColor = Vector3(0.45f,0.52f,0.9f); break; // light blue
        case 10 :  furColor = Vector3(0.63f,0.29f,0.73f); break; // purple
        case 11 :  furColor = Vector3(0.73f,0.42f,0.73f); break; // magneta
        case 12 :  furColor = Vector3(0.8f,0.4f,0.1f); break; // orange
        case 13 :  furColor = Vector3(0.73f,0.73f,0.73f); break; // light gray
        case 14 : furColor = Vector3(1,1,1); break;
        default : furColor = Vector3(1,1,1); break;
    }
}

void Sheep::SetFurColor(int newColor)
{
    color = newColor;

    FetchFurColor();
}

int Sheep::GetFurColor()
{
    return color;
}


void Sheep::BoxCollision()
{
    for(int x = 0; x < (int)world->mSheeps.size(); x++)
    {
        Sheep *Other = world->mSheeps[x];
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

void Sheep::TakeDamage(float damage, float power, float dt)
{
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Sheep::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Sheep::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
{
    if(!BeginMobUpdate(myWorld, playerPos, dt))
        return;

    UpdateDeathPose(dt);

    // Fur regrows after shearing.
    if(sheared == true)
    {
        furTime -= dt;
        if(furTime < 0.0f)
        {
            furTime = 60+(rand()%5)*30;
            sheared = false;
        }
    }

    if(toDestroy == false)
    {
        // Slow HP regen back to profile max.
        HP += dt/10.0f;
        if(HP > profile.hp)
            HP = profile.hp;

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

            Sheep* mate = InLove() ? FindKin(world->mSheeps, true, 8.0f) : NULL;
            Sheep* parent = IsBaby() ? FindKin(world->mSheeps, false, 8.0f) : NULL;
            if(UpdatePassiveAI(dt, handItemId, mate, parent, &headAngleY) && mate != NULL && world->CanSpawnPassive())
            {
                world->SpawnSheep((position.x + mate->position.x) * 0.5f, position.y + 0.3f, (position.z + mate->position.z) * 0.5f);
                world->mSheeps.back()->SetAge(-1200.0f);
                world->mSheeps.back()->SetFurColor(rand() % 2 == 0 ? GetFurColor() : mate->GetFurColor());
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

void Sheep::Render(Frustum &camFrustum, float dt)
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
                sceGuColor(GU_COLOR(myLight*furColor.x,myLight*furColor.y,myLight*furColor.z,1.0f));
            }

            TextureManager::Instance()->SetTextureModeulate(mainTexture);

            sceGuEnable(GU_TEXTURE_2D);
            sceGuEnable(GU_DEPTH_TEST);
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);



            if(sheared)
            {
                if((damaged == true && damagedTimer > 0.25f) || killed == true)
                {
                    sceGuColor(GU_COLOR(0.8f,0.1,0.1,1.0f));
                }
                else
                {
                    sceGuColor(GU_COLOR(myLight,myLight,myLight,1.0f));
                }

                sceGumPushMatrix();

                ScePspFVector3 loc = {0,0,0};
                sceGumTranslate(&loc);

                ScePspFVector3 sca = {1,1,1};
                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->sheepBodyVertices);

                sceGumPopMatrix();
            }
            else
            {
                if((damaged == true && damagedTimer > 0.25f) || killed == true)
                {
                    sceGuColor(GU_COLOR(myLight*furColor.x*0.8f,myLight*furColor.y*0.1,myLight*furColor.z*0.1,1.0f));
                }
                else
                {
                    sceGuColor(GU_COLOR(myLight*furColor.x,myLight*furColor.y,myLight*furColor.z,1.0f));
                }

                TextureManager::Instance()->SetTextureModeulate(furTexture);
                // body fur
                sceGumPushMatrix();

                ScePspFVector3 loc = {-PIXEL*0.2,-PIXEL*0.2,0};
                sceGumTranslate(&loc);

                ScePspFVector3 sca = {1.225,1.6,1.4};
                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->sheepBodyVertices);

                sceGumPopMatrix();

                // head fur
                sceGumPushMatrix();
                ScePspFVector3 loc2 = {PIXEL*9,PIXEL*4,0};
                sceGumTranslate(&loc2);
                if(IsBaby())
                {
                    ScePspFVector3 babyHead = {1.6f,1.6f,1.6f};
                    sceGumScale(&babyHead);
                }

                ScePspFVector3 sca2 = {1.24,1.24,1.24};
                sceGumScale(&sca2);

                sceGumRotateX(0.0f);
                sceGumRotateY(headAngleY);
                sceGumRotateZ(0.0f);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->sheepHeadFurVertices);

                sceGumPopMatrix();
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

            sceGumPushMatrix();
            ScePspFVector3 loc2 = {PIXEL*10,PIXEL*4,0};
            sceGumTranslate(&loc2);
            if(IsBaby())
            {
                ScePspFVector3 babyHead = {1.6f,1.6f,1.6f};
                sceGumScale(&babyHead);
            }

            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30+sheared*6, 0, world->sheepHeadVertices);

            sceGumPopMatrix();

            if(!sheared)
            {
                TextureManager::Instance()->SetTextureModeulate(furTexture);
                if((damaged == true && damagedTimer > 0.25f) || killed == true)
                {
                    sceGuColor(GU_COLOR(myLight*furColor.x*0.8f,myLight*furColor.y*0.1,myLight*furColor.z*0.1,1.0f));
                }
                else
                {
                    sceGuColor(GU_COLOR(myLight*furColor.x,myLight*furColor.y,myLight*furColor.z,1.0f));
                }

                ScePspFVector3 sca = {1.23f,1.23f,1.23f};

                sceGumPushMatrix();
                ScePspFVector3 loc3 = {-PIXEL*7,-PIXEL*5,PIXEL*3};
                sceGumTranslate(&loc3);

                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(PI);
                sceGumRotateZ(rLegAngle-animLegAngle);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30, 0, world->sheepLegFurVertices);
                sceGumPopMatrix();




                sceGumPushMatrix();
                ScePspFVector3 loc4 = {PIXEL*5,-PIXEL*5,PIXEL*3};
                sceGumTranslate(&loc4);

                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(PI);
                sceGumRotateZ(lLegAngle+animLegAngle);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30, 0, world->sheepLegFurVertices);
                sceGumPopMatrix();



                sceGumPushMatrix();
                ScePspFVector3 loc5 = {-PIXEL*7,-PIXEL*5,-PIXEL*3};
                sceGumTranslate(&loc5);

                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(PI);
                sceGumRotateZ(rLegAngle+animLegAngle);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30, 0, world->sheepLegFurVertices);
                sceGumPopMatrix();




                sceGumPushMatrix();
                ScePspFVector3 loc6 = {PIXEL*5,-PIXEL*5,-PIXEL*3};
                sceGumTranslate(&loc6);

                sceGumScale(&sca);

                sceGumRotateX(0.0f);
                sceGumRotateY(PI);
                sceGumRotateZ(lLegAngle-animLegAngle);

                sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30, 0, world->sheepLegFurVertices);
                sceGumPopMatrix();
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

            sceGumPushMatrix();
            ScePspFVector3 loc3 = {-PIXEL*7,-PIXEL*5,PIXEL*3};
            sceGumTranslate(&loc3);

            sceGumRotateX(0.0f);
            sceGumRotateY(PI);
            sceGumRotateZ(rLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30+sheared*6, 0, world->cowLegVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc4 = {PIXEL*5,-PIXEL*5,PIXEL*3};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.0f);
            sceGumRotateY(PI);
            sceGumRotateZ(lLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30+sheared*6, 0, world->cowLegVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc5 = {-PIXEL*7,-PIXEL*5,-PIXEL*3};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(PI);
            sceGumRotateZ(rLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30+sheared*6, 0, world->cowLegVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc6 = {PIXEL*5,-PIXEL*5,-PIXEL*3};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(PI);
            sceGumRotateZ(lLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 30+sheared*6, 0, world->cowLegVertices);
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

