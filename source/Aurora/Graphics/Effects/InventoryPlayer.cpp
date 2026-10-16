#include <Aurora/Graphics/Effects/InventoryPlayer.h>
#include <Aurora/Graphics/TextureManager.h>

#define PI 3.1415926535897f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f

// Third-person bow pose. These are the numbers that describe how a drawn bow is held in the world,
// and they exist because the arm's walk swing has no way to express "aiming": at rest the arm hangs
// down the body, so without them a drawing bow sits at the player's hip no matter where the camera
// points.
//
// kBowArmRaise is the lift that puts the arm *parallel to the head* -- literally, and it is the only
// value that does. The arm is modelled pointing up out of its pivot and hung down by a PI, so an arm
// angle of PI + p points along (sin p, -cos p) while the head's facing is (cos h, sin h); the two are
// the same vector exactly when p = PI/2 + h, and since the arm's pitch *is* kBowArmRaise + the
// camera's pitch, that makes kBowArmRaise = PI/2. Anything less and the arm hangs below the aim by
// the difference -- which at the old 0.95 was 36 degrees, and is what "the arm does not come up to
// the head" was looking at.
static const float kBowArmRaise = PI / 2.0f;

// A safety net and nothing else. The camera clamps its own pitch to +/-1.561 rad (Camera.cpp), so
// PI/2 + that stays inside [0, PI] for every angle the player can actually reach: these two can only
// bite if something upstream starts handing over a pitch the camera would not.
static const float kBowArmPitchMin = 0.0f;
static const float kBowArmPitchMax = PI;

// The bow's own in-plane tilt while it is being drawn.
//
// The held-item mesh is one flat quad drawn inside the arm's own matrix, so the sprite's tilt is the
// arm's rotation plus whatever it is handed here -- which means the arm's *raise* is in the bow's
// tilt unless it is cancelled. With the constant PI this file used to pass, the bow inherited the
// whole thing and leaned by kBowArmRaise + the pitch, so raising the arm to parallel would have laid
// the bow flat: at PI/2 the sprite's own up ends up pointing along the aim, which from behind the
// player, where a flat quad is edge-on already, collapses it to nothing.
//
// Writing the tilt as the difference between the head and the arm is what makes the bow's lean the
// camera's pitch and nothing else: headPitch and the arm's headPitch cancel, leaving -kBowArmRaise,
// which is the constant that puts the sprite's limbs vertical when the camera is level. The bow
// leans back as the player looks up, so the face of the bow tips up with the arrow, and the tilt
// cannot drift the next time kBowArmRaise changes -- it is stated in the numbers the arm is made of.

namespace Aurora
{
	namespace Graphics
	{
		InventoryPlayer::InventoryPlayer()
		{
            rHandAngle = PI;
            lHandAngle = PI;
            rLegAngle = PI;
            lLegAngle = PI;

            mainAngle = 0.0f;
            scale = 1.0f;

            position = Vector3(0,0,0);
            animT = 0.0f;
		}

		InventoryPlayer::~InventoryPlayer()
		{

		}

		void  InventoryPlayer::Update()
		{

		}

		void InventoryPlayer::Render(CraftWorld *world, float dt, float angle, int texNum, int handItemId)
		{
            animT += PI*dt*0.5f;
            if(animT >= 2*PI)
            {
                animT = 0.0f;
            }
            sceGumPushMatrix();

            ScePspFVector3 loc22 = {position.x,position.y,position.z};
            sceGumTranslate(&loc22);

            sceGumRotateZ(0.0f);
            sceGumRotateY(mainAngle-PI/2);

            sceGuEnable(GU_TEXTURE_2D);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuEnable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);


            // right hand
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
            sceGumTranslate(&loc3);

            sceGumRotateX(-0.045+sinf(animT)*0.045);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rHandAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);
            sceGumPopMatrix();


            // left hand
            sceGumPushMatrix();
            ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.045+sinf(animT+PI)*0.045);
            sceGumRotateY(0.0f);

            if(handItemId != -1)
            {
                sceGumRotateZ(lHandAngle+PI/9.0f);
            }
            else
            {
                sceGumRotateZ(lHandAngle);
            }

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);

            // ITEM IN OUR HAND
            sceGumPushMatrix();

            if(handItemId != -1)
            {
                if(IsItemId(handItemId))
                {
                    world->ItemHaveTerrainTexture(handItemId) == true ? TextureManager::Instance()->SetTextureModeulate(world->textureTerrainId) : TextureManager::Instance()->SetTextureModeulate(world->textureItemsId);

                    ScePspFVector3 loc40 = {-PIXEL*5.0f,PIXEL*7.3,0};
                    sceGumTranslate(&loc40);

                    /*sceGumRotateX(PI/6.5f);
                    sceGumRotateY(PI/2.0f);
                    sceGumRotateZ(PI/6.5f); */

                    sceGumRotateZ(-PI/3.5f);
                    sceGumRotateY(PI/2.0f);
                    sceGumRotateX(0.18f);

                    ScePspFVector3 scale = {0.53,0.53,0.53};
                    sceGumScale(&scale);

                    world->drawFull3DItem(handItemId,1.0f);
                }
                else
                {
                    TextureManager::Instance()->SetTextureModeulate(world->textureTerrainId);

                    ScePspFVector3 loc40 = {-PIXEL*2.4,PIXEL*8.4,0.0};
                    sceGumTranslate(&loc40);

                    sceGumRotateX(0.0f);
                    sceGumRotateY(PI/4.0f);

                    sceGumRotateZ(-0.15);

                    ScePspFVector3 scale = {0.3,0.3,0.3};
                    sceGumScale(&scale);

                    world->drawCubes(handItemId,1.0f);
                }
            }

            sceGumPopMatrix();

            sceGumPopMatrix();

            sceGuEnable(GU_TEXTURE_2D);
            // right leg
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc5 = {0,-PIXEL*8,PIXEL*2};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();


            // left leg
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc6 = {0,-PIXEL*8,-PIXEL*2};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();


            // body
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc = {0,0,0};
            sceGumTranslate(&loc);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->bodyVertices);
            sceGumPopMatrix();



            // head
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc2 = {0,PIXEL*10,0};
            sceGumTranslate(&loc2);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->headVertices);
            sceGumPopMatrix();


            // hat
            /*TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            sceGumTranslate(&loc2);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(0.0f);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->hatVertices);
            sceGumPopMatrix();
            */

            // boot
            if(world->armorId[3] != 327)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc11 = {0,-PIXEL*8,PIXEL*2};
                sceGumTranslate(&loc11);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rLegAngle);

                world->drawArmor(world->armorId[3],1.0f);
                sceGumPopMatrix();
            }


            // boot
            if(world->armorId[3] != 327)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc12 = {0,-PIXEL*8,-PIXEL*2};
                sceGumTranslate(&loc12);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(lLegAngle);

                world->drawArmor(world->armorId[3],1.0f);
                sceGumPopMatrix();
            }


            // legging
            if(world->armorId[2] != 326)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc9 = {0,-PIXEL*8,PIXEL*2};
                sceGumTranslate(&loc9);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rLegAngle);

                world->drawArmor(world->armorId[2],1.0f);
                sceGumPopMatrix();
            }


            // legging
            if(world->armorId[2] != 326)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc10 = {0,-PIXEL*8,-PIXEL*2};
                sceGumTranslate(&loc10);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(lLegAngle);

                world->drawArmor(world->armorId[2],1.0f);
                sceGumPopMatrix();
            }

            // belly
            if(world->armorId[2] != 326)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc14 = {0,0,0};
                sceGumTranslate(&loc14);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                world->drawBelly(world->armorId[2],1.0f);
                sceGumPopMatrix();
            }


            // helmet
            if(world->armorId[0] != 324)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc7 = {0,PIXEL*10,0};
                sceGumTranslate(&loc7);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                if(world->armorId[0] == Pumpkin3::getID())
                {
                    ScePspFVector3 sca = {0.6f,0.6f,0.6f};
                    sceGumScale(&sca);
                    TextureManager::Instance()->SetTextureModeulate(TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1));
                    world->drawCubes(Pumpkin2::getID(),1.0f);
                }
                else
                {
                    world->drawArmor(world->armorId[0],1.0f);
                }

                sceGumPopMatrix();
            }


            // chestplate
            if(world->armorId[1] != 325)
            {
                sceGumPushMatrix();
                ScePspFVector3 loc8 = {0,0,0};
                sceGumTranslate(&loc8);

                sceGumRotateX(0.0f);
                sceGumRotateY(0.0f);
                sceGumRotateZ(0.0f);

                world->drawArmor(world->armorId[1],1.0f);
                sceGumPopMatrix();

                // right shoulder
                sceGumPushMatrix();
                ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
                sceGumTranslate(&loc3);

                sceGumRotateX(-0.045+sinf(animT)*0.045);
                sceGumRotateY(0.0f);
                sceGumRotateZ(rHandAngle);
                world->drawShoulder(world->armorId[1],1.0f);
                sceGumPopMatrix();

                // left shoulder
                sceGumPushMatrix();
                ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
                sceGumTranslate(&loc4);

                sceGumRotateX(0.045+sinf(animT+PI)*0.045);
                sceGumRotateY(0.0f);
                if(handItemId != -1)
                {
                    sceGumRotateZ(lHandAngle+PI/9.0f);
                }
                else
                {
                    sceGumRotateZ(lHandAngle);
                }

                world->drawShoulder(world->armorId[1],1.0f);
                sceGumPopMatrix();
            }


            sceGuDisable(GU_DEPTH_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_TEXTURE_2D);
            sceGuColor(0xFFFFFFFF);

            sceGumPopMatrix();
		}

		void InventoryPlayer::RenderInWorld(CraftWorld *world, Vector3 feetPosition, float faceAngle,
		                                    float legSwing, float armSwing, float light,
		                                    int texNum, int handItemId,
		                                    float headYaw, float headPitch, float hitSwing, int bowStage)
		{
            // Placement, not animation. Every mesh here is shared with the mobs and is built around the
            // hip (see buildmobVerts): the body straddles the origin, the head pivots at +10 px and the
            // legs/arms at -8 px and +4 px, all with their 12 px boxes modelled *upward* from the pivot
            // and flipped by the PI in the rotation below. That puts the sole at -18 px and the crown at
            // +14 px, so a 32 px player hangs from hip to foot exactly as far as the feet are below the
            // hip. Passing the feet is therefore the honest interface, and the caller only has to know
            // where the player is standing.
            sceGumPushMatrix();

            ScePspFVector3 loc = {feetPosition.x, feetPosition.y + PIXEL*18.0f, feetPosition.z};
            sceGumTranslate(&loc);

            // Same convention as LameMob: forward is (sin(faceAngle), 0, cos(faceAngle)) and the mesh
            // itself faces +X, hence the quarter turn back.
            sceGumRotateY(faceAngle-PI/2);

            sceGuEnable(GU_TEXTURE_2D);
            sceGuEnable(GU_DEPTH_TEST);
            sceGuEnable(GU_BLEND);
            sceGuDisable(GU_ALPHA_TEST);
            // Block light, as the mobs do. The armour and held item passes below overwrite the colour
            // themselves and get the same value handed to them.
            sceGuColor(GU_COLOR(light,light,light,1.0f));

            // right leg
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc5 = {0,-PIXEL*8,PIXEL*2};
            sceGumTranslate(&loc5);
            sceGumRotateZ(PI+legSwing);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();

            // left leg, opposite phase
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc6 = {0,-PIXEL*8,-PIXEL*2};
            sceGumTranslate(&loc6);
            sceGumRotateZ(PI-legSwing);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->legVertices);
            sceGumPopMatrix();

            // body
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->bodyVertices);
            sceGumPopMatrix();

            // head
            // Turning and nodding. The body has already been placed at faceAngle, so headYaw is the
            // amount the head is turned past it, and headPitch is the camera's own. Y before Z: the
            // point is pitched about the head's own Z axis (the mesh faces +X, so Z is what it nods
            // around) and only then turned, which is how the mob models are driven.
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc2 = {0,PIXEL*10,0};
            sceGumTranslate(&loc2);
            sceGumRotateY(headYaw);
            sceGumRotateZ(headPitch);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->headVertices);
            sceGumPopMatrix();

            // A bow in hand that is being drawn takes the right arm (and so the bow it carries) out of
            // the walk pose. bowStage is -1 for "not drawing", so anything at or above 0 means the
            // string is pulled; see the note on the parameter. Pairing it with the item check keeps the
            // arm down if the slot is switched away from the bow mid-draw. Both values stay zero for
            // every other case, which leaves the arm exactly as it was.
            const bool holdingBow = IsBowItem(handItemId);
            const bool drawingBow = (bowStage >= 0) && (holdingBow == true);
            float armYaw = 0.0f;
            float armPitch = 0.0f;
            if(drawingBow == true)
            {
                // Parallel to the head, by construction: the arm's own pitch is this lift plus the
                // camera's, and PI/2 is the value that makes the arm's direction equal the head's.
                // The yaw is the head's outright, so the arm is aimed wherever the head is looking.
                armYaw = headYaw;
                armPitch = kBowArmRaise + headPitch;
                if(armPitch < kBowArmPitchMin)
                    armPitch = kBowArmPitchMin;
                if(armPitch > kBowArmPitchMax)
                    armPitch = kBowArmPitchMax;
            }

            // right arm
            // The hit swing is added, giving PI + armSwing + hitSwing. The arm's geometry is modelled
            // *upward* from its pivot and turned round by the PI, so with the PI in place a positive
            // angle on top of it tips the arm from hanging-down toward +X, which is the model's
            // forward -- so positive is a forward punch, the same sense as legSwing. The walk swing
            // and the hit swing stack, as they do in Java, so a punch mid-stride keeps the stride.
            // Y before Z, the same order the head uses, so a bow aimed left or right stays aimed
            // while it pitches.
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
            sceGumTranslate(&loc3);
            sceGumRotateY(armYaw);
            sceGumRotateZ(PI+armSwing+hitSwing+armPitch);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);
            sceGumPopMatrix();

            // left arm, opposite phase
            TextureManager::Instance()->SetTextureModeulate(texNum);
            sceGumPushMatrix();
            ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
            sceGumTranslate(&loc4);
            sceGumRotateZ(PI-armSwing);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->handVertices);
            sceGumPopMatrix();

            // Armour, on the same pivots the bare model uses so it follows the walk. The sentinel ids
            // and the pumpkin-on-the-head special case are the same ones InventoryPlayer::Render uses.
            //
            // Each of these helpers turns depth testing *off* on the way out (they were written for the
            // inventory's floating model, where nothing needs to occlude), and each one rebinds the
            // texture. In the world that would leave the armour and everything after it drawn over the
            // terrain, so the state is put back after every call.
            if(world->armorId[3] != 327) // boots
            {
                for(int leg = 0; leg < 2; leg++)
                {
                    sceGumPushMatrix();
                    ScePspFVector3 bootLoc = {0,-PIXEL*8, leg == 0 ? PIXEL*2 : -PIXEL*2};
                    sceGumTranslate(&bootLoc);
                    sceGumRotateZ(leg == 0 ? PI+legSwing : PI-legSwing);
                    world->drawArmor(world->armorId[3],light);
                    sceGumPopMatrix();

                    sceGuEnable(GU_DEPTH_TEST);
                    sceGuEnable(GU_BLEND);
                    sceGuColor(GU_COLOR(light,light,light,1.0f));
                }
            }

            if(world->armorId[2] != 326) // leggings, plus the belly that goes with them
            {
                for(int leg = 0; leg < 2; leg++)
                {
                    sceGumPushMatrix();
                    ScePspFVector3 leggingLoc = {0,-PIXEL*8, leg == 0 ? PIXEL*2 : -PIXEL*2};
                    sceGumTranslate(&leggingLoc);
                    sceGumRotateZ(leg == 0 ? PI+legSwing : PI-legSwing);
                    world->drawArmor(world->armorId[2],light);
                    sceGumPopMatrix();

                    sceGuEnable(GU_DEPTH_TEST);
                    sceGuEnable(GU_BLEND);
                    sceGuColor(GU_COLOR(light,light,light,1.0f));
                }

                sceGumPushMatrix();
                world->drawBelly(world->armorId[2],light);
                sceGumPopMatrix();

                sceGuEnable(GU_DEPTH_TEST);
                sceGuEnable(GU_BLEND);
                sceGuColor(GU_COLOR(light,light,light,1.0f));
            }

            if(world->armorId[1] != 325) // chestplate and both shoulders
            {
                sceGumPushMatrix();
                world->drawArmor(world->armorId[1],light);
                sceGumPopMatrix();

                for(int arm = 0; arm < 2; arm++)
                {
                    sceGumPushMatrix();
                    ScePspFVector3 shoulderLoc = {0,PIXEL*4, arm == 0 ? PIXEL*6 : -PIXEL*6};
                    sceGumTranslate(&shoulderLoc);
                    // arm 0 is the right one, so it carries the hit swing with the bare arm above.
                    sceGumRotateZ(arm == 0 ? PI+armSwing+hitSwing : PI-armSwing);
                    world->drawShoulder(world->armorId[1],light);
                    sceGumPopMatrix();

                    sceGuEnable(GU_DEPTH_TEST);
                    sceGuEnable(GU_BLEND);
                    sceGuColor(GU_COLOR(light,light,light,1.0f));
                }
            }

            if(world->armorId[0] != 324) // helmet
            {
                sceGumPushMatrix();
                ScePspFVector3 helmetLoc = {0,PIXEL*10,0};
                sceGumTranslate(&helmetLoc);
                // The helmet sits on the head, so it turns and nods with it.
                sceGumRotateY(headYaw);
                sceGumRotateZ(headPitch);

                if(world->armorId[0] == Pumpkin3::getID())
                {
                    ScePspFVector3 helmetScale = {0.6f,0.6f,0.6f};
                    sceGumScale(&helmetScale);
                    TextureManager::Instance()->SetTextureModeulate(TextureHelper::Instance()->GetTexture(TextureHelper::Terrain1));
                    world->drawCubes(Pumpkin2::getID(),light);
                }
                else
                {
                    world->drawArmor(world->armorId[0],light);
                }

                sceGumPopMatrix();

                sceGuEnable(GU_DEPTH_TEST);
                sceGuEnable(GU_BLEND);
                sceGuColor(GU_COLOR(light,light,light,1.0f));
            }

            // Held item, from the right arm's matrix so it swings with the arm -- including the hit
            // swing, so the block or tool follows a punch. The item hangs at the far end of the arm
            // (10 px down it) and the second PI cancels the arm's own flip, so the block or sprite
            // stays upright in the world while the arm moves.
            if(handItemId != -1)
            {
                // A held bow is a weapon, and while it is being drawn it is drawn as one of its three
                // pulling frames rather than as the bow itself -- the frames are item types of their
                // own, which is how a different atlas cell reaches the mesh. Same three rules as the
                // skeleton and the first-person hand; see kHeldBowScale. holdingBow comes from above,
                // where the arm's pose is decided from it.
                sceGumPushMatrix();
                ScePspFVector3 itemLoc = {0,PIXEL*4,PIXEL*6};
                sceGumTranslate(&itemLoc);
                // The same two rotations the arm above takes, so the bow travels with the hand it is
                // in: while the string is pulled it turns with the head's yaw and pitches with the
                // camera. See armYaw / armPitch.
                sceGumRotateY(armYaw);
                sceGumRotateZ(PI+armSwing+hitSwing+armPitch);
                ScePspFVector3 downArm = {0,PIXEL*10,0};
                sceGumTranslate(&downArm);
                // The item's own rotation, which is the *difference* between the tilt the bow should
                // have and the arm it is hanging from -- both are turns about this same Z, so they
                // add. While drawing, that difference is the camera's pitch minus the arm's, which
                // leaves the sprite's limbs vertical when the camera is level and leaning with the
                // camera when it is not; see the note on kBowArmRaise. Carried and not drawn, the
                // item stays rigid on the arm like every other held thing, which is a flat PI.
                //
                // kHeldBowAimTilt rides on top of both. The limbs being vertical is not the same as
                // the bow aiming level: the arrow drawn inside the artwork is 45 degrees off the
                // sprite's up, so an upright sprite has it aimed at the sky. The tilt is what lands
                // that arrow on the arm -- and because the arm and the sprite are turned about this
                // same Z, the two RotZs above cancel *out of the difference*, leaving the artwork's
                // own angle as the whole of the answer: see the definition in Items.cpp. The
                // skeleton and the first-person hand need different numbers, because their chains
                // are different chains.
                if(drawingBow == true)
                    sceGumRotateZ(headPitch - armPitch + (holdingBow ? kHeldBowAimTilt : 0.0f));
                else
                    sceGumRotateZ(PI + (holdingBow ? kHeldBowAimTilt : 0.0f));
                float heldScale = (holdingBow == true) ? kHeldBowScale : 0.42f;
                ScePspFVector3 itemScale = {heldScale,heldScale,heldScale};
                sceGumScale(&itemScale);

                if(IsItemId(handItemId))
                {
                    world->ItemHaveTerrainTexture(handItemId) == true ? TextureManager::Instance()->SetTextureModeulate(world->textureTerrainId)
                                                                      : TextureManager::Instance()->SetTextureModeulate(world->textureItemsId);
                    if(holdingBow == true)
                        world->drawBowInHand(bowStage,light);
                    else
                        world->drawFull3DItem(handItemId,light);
                }
                else
                {
                    TextureManager::Instance()->SetTextureModeulate(world->textureTerrainId);
                    world->drawCubes(handItemId,light);
                }

                sceGumPopMatrix();
            }

            sceGuDisable(GU_DEPTH_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_TEXTURE_2D);
            sceGuColor(0xFFFFFFFF);

            sceGumPopMatrix();
		}
	}
}

