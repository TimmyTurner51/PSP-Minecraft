#include "Zombie2.h"
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f
#include "CraftWorld2.h"

Zombie::Zombie(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Zombie);
    debugName = "Zombie";

    legs = Vector3(0,PIXEL*-6,0);

    SetCollideBox(Vector3(PIXEL*4,PIXEL*18,PIXEL*4),Vector3(PIXEL*4,PIXEL*12,PIXEL*4));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    rHandAngle = PI / 2 + PI;
    lHandAngle = PI / 2 + PI;

    animHandAngle = 0.0f;
    animHandAngleT = 0.0f;

    animLegAngle = 0.0f;
    animLegAngleT = 0.0f;

    rLegAngle = PI;
    lLegAngle = PI;

    helmetId = -1;
    chestplateId = -1;
    leggingsId = -1;
    bootsId = -1;

    bool armored = false;
    if(rand() % 100 <= 4) // 5 %
    {
        armored = true;
    }
    if(armored)
    {
        int armorChance = rand() % 100;
        if(armorChance < 37)
        {
            helmetId = LeatherHelmet::getID();
        }
        else if (armorChance >= 37 && armorChance < 65)
        {
            helmetId = GoldenHelmet::getID();
        }
        else if (armorChance >= 65 && armorChance < 98)
        {
            helmetId = ChainmailHelmet::getID();
        }
        else if (armorChance >= 95)
        {
            helmetId = IronHelmet::getID();
        }

        if(rand() % 100 >= 10)
        {
            armorChance = rand() % 100;
            if(armorChance < 37)
            {
                chestplateId = LeatherChestplate::getID();
            }
            else if (armorChance >= 37 && armorChance < 65)
            {
                chestplateId = GoldenChestplate::getID();
            }
            else if (armorChance >= 65 && armorChance < 98)
            {
                chestplateId = ChainmailChestplate::getID();
            }
            else if (armorChance >= 95)
            {
                chestplateId = IronChestplate::getID();
            }
        }

        if(rand() % 100 >= 10)
        {
            armorChance = rand() % 100;
            if(armorChance < 37)
            {
                leggingsId = LeatherLeggings::getID();
            }
            else if (armorChance >= 37 && armorChance < 65)
            {
                leggingsId = GoldenLeggings::getID();
            }
            else if (armorChance >= 65 && armorChance < 98)
            {
                leggingsId = ChainmailLeggings::getID();
            }
            else if (armorChance >= 95)
            {
                leggingsId = IronLeggings::getID();
            }
        }

        if(rand() % 100 >= 10)
        {
            armorChance = rand() % 100;
            if(armorChance < 37)
            {
                bootsId = LeatherBoots::getID();
            }
            else if (armorChance >= 37 && armorChance < 65)
            {
                bootsId = GoldenBoots::getID();
            }
            else if (armorChance >= 65 && armorChance < 95)
            {
                bootsId = ChainmailBoots::getID();
            }
            else if (armorChance >= 95)
            {
                bootsId = IronBoots::getID();
            }
        }
    }
    UpdateBBox();

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::zombieTexture);

    headAngleY = 0.0f;
    headAngleZ = 0.0f;
    UpdateBBox();
    myLight = 1.0f;
}

Zombie::~Zombie()
{

}

void Zombie::BoxCollision()
{
    for(int x = 0; x < (int)world->mZombies.size(); x++)
    {
        Zombie *Other = world->mZombies[x];
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

void Zombie::TakeDamage(float damage, float power, float dt)
{
    float AP = 2.0f + world->FetchArmorPoints(bootsId, leggingsId, chestplateId, helmetId);
    ApplyKickDamage(damage*(1.0f-(AP/25.0f)), power, dt, 1, &animLegAngleT);
}

void Zombie::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    float AP = 2.0f + world->FetchArmorPoints(bootsId, leggingsId, chestplateId, helmetId);
    ApplyKickDamage(damage*(1.0f-(AP/25.0f)), power, dt, 2, NULL);
}

void Zombie::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
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
                SetCollideBox(Vector3(PIXEL*4,PIXEL*5,PIXEL*4),Vector3(PIXEL*4,PIXEL*12,PIXEL*4));
                return;
            }

            UpdateBBox();
            UpdateMobTimers(dt, playerHitBox);

            bool wantAttack = false;
            UpdateHostileAI(dt, &wantAttack);

            // Melee kick (survival only; UpdateHostileAI already gated creative).
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

            // Look at player while chasing.
            if(angred == true && kicked == false)
            {
                float playerAngle = ATAN2_fast(position.x-relPlayerPos.x,position.z-relPlayerPos.z)+PI;
                headAngleY = playerAngle-mainAngle;
                headAngleZ = (relPlayerPos.y-position.y-0.315f)*0.25f;
                if(headAngleZ > PI/4.0f) headAngleZ = PI/4.0f;
                if(headAngleZ < -PI/4.0f) headAngleZ = -PI/4.0f;
            }
            else if(kicked == false)
            {
                headAngleY = 0.0f;
                headAngleZ = 0.0f;
            }

            UpdateKnockbackMove(dt);

            // Hand idle anim (zombie-specific).
            if(killed == false)
            {
                animHandAngleT += PI*dt*0.75f;
                if(animHandAngleT >= PI*2) animHandAngleT = 0.0f;
                animHandAngle = sinf(animHandAngleT) * 0.09f;
            }
            UpdateLegAnim(dt, &animLegAngleT, &animLegAngle);
            UpdateMobPhysics(dt);
        }
        else
        {
            UpdateOutOfRangeFreeze(dt);
        }
    }
}

void Zombie::Render(Frustum &camFrustum, float dt)
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
            sceGuEnable(GU_BLEND);;


            sceGumPushMatrix();
            ScePspFVector3 loc = {0,0,0};
            sceGumTranslate(&loc);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->bodyVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc2 = {0,PIXEL*10,0};
            sceGumTranslate(&loc2);

            sceGumRotateX(0.0f);
            sceGumRotateY(headAngleY);
            sceGumRotateZ(headAngleZ);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->headVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
            sceGumTranslate(&loc3);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rHandAngle-animHandAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lHandAngle+animHandAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);
            sceGumPopMatrix();



            sceGumPushMatrix();
            ScePspFVector3 loc5 = {0,-PIXEL*8,PIXEL*2};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();




            sceGumPushMatrix();
            ScePspFVector3 loc6 = {0,-PIXEL*8,-PIXEL*2};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();


            // boot
            if(bootsId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc11 = {0,-PIXEL*8.1f,PIXEL*2};
                sceGumTranslate(&loc11);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rLegAngle+animLegAngle);

                world->drawArmor(bootsId,myLight);
                sceGumPopMatrix();
            }


            // boot
            if(bootsId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc12 = {0,-PIXEL*8.1f,-PIXEL*2};
                sceGumTranslate(&loc12);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(lLegAngle-animLegAngle);

                world->drawArmor(bootsId,myLight);
                sceGumPopMatrix();
            }


            // legging
            if(leggingsId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc9 = {0,-PIXEL*7.9,PIXEL*2};
                sceGumTranslate(&loc9);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rLegAngle+animLegAngle);

                world->drawArmor(leggingsId,myLight);
                sceGumPopMatrix();
            }


            // legging
            if(leggingsId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc10 = {0,-PIXEL*7.9,-PIXEL*2};
                sceGumTranslate(&loc10);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(lLegAngle-animLegAngle);

                world->drawArmor(leggingsId,myLight);
                sceGumPopMatrix();
            }

            // helmet
            if(helmetId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc7 = {0,PIXEL*10,0};
                sceGumTranslate(&loc7);

                sceGumRotateX(0.0f);
                sceGumRotateY(headAngleY);
                sceGumRotateZ(headAngleZ);

                world->drawArmor(helmetId,myLight);
                sceGumPopMatrix();
            }


            // chestplate
            if(chestplateId != -1)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc8 = {0,0,0};
                sceGumTranslate(&loc8);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                world->drawArmor(chestplateId,myLight);
                sceGumPopMatrix();

                // right shoulder
                sceGumPushMatrix();
                ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
                sceGumTranslate(&loc3);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rHandAngle-animHandAngle);
                world->drawShoulder(chestplateId,myLight);
                sceGumPopMatrix();

                // left shoulder
                sceGumPushMatrix();
                ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
                sceGumTranslate(&loc4);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(lHandAngle+animHandAngle);
                world->drawShoulder(chestplateId,myLight);
                sceGumPopMatrix();
            }

            DrawSetCulling(false);

            sceGuDisable(GU_BLEND);
            //sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_DEPTH_TEST);
            sceGuDisable(GU_TEXTURE_2D);
            sceGuColor(0xFFFFFFFF);

            sceGumPopMatrix();

            RenderFire();
        }
    }
}

