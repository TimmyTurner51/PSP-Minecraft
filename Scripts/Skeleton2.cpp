#include "Skeleton2.h"
#include <Aurora/Graphics/TextureManager.h>
#include <Aurora/Graphics/Effects/Arrow2.h>

#define PI 3.14159f
#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f
#include "CraftWorld2.h"
#include "SoundManager.h"

Skeleton::Skeleton(Vector3 position1, int number1) : LameMob(position1, number1)
{
    ApplyProfile(MobProfiles::Skeleton);
    debugName = "Skeleton";

    legs = Vector3(0,PIXEL*-6,0);

    SetCollideBox(Vector3(PIXEL*4,PIXEL*18,PIXEL*4),Vector3(PIXEL*4,PIXEL*12,PIXEL*4));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    // Arms forward, the pose the zombie uses -- a skeleton holds its bow out in front, so the same
    // two angles read correctly on the shared rig.
    rHandAngle = PI / 2 + PI;
    lHandAngle = PI / 2 + PI;

    animHandAngle = 0.0f;
    animHandAngleT = 0.0f;

    animLegAngle = 0.0f;
    animLegAngleT = 0.0f;

    rLegAngle = PI;
    lLegAngle = PI;

    headAngleY = 0.0f;
    headAngleZ = 0.0f;

    shootDraw = 0.0f;
    shootRecoil = 0.0f;
    shootDrawing = false;
    shootAimed = false;

    // Stagger the first shot so a group of skeletons does not loose in unison, and so none of them
    // fires the instant the player comes into view.
    rangedShotTimer = profile.rangedCooldownMin +
        (profile.rangedCooldownMax - profile.rangedCooldownMin) * (rand() / (float)RAND_MAX);

    UpdateBBox();
    UpdateBBox();

    mainTexture = TextureHelper::Instance()->GetTexture(TextureHelper::skeletonTexture);
    myLight = 1.0f;
}

Skeleton::~Skeleton()
{

}

int Skeleton::DebugOptions(std::vector<std::string>& options)
{
    // The bow first: it is the reason this mob is worth poking, and the only option here that does
    // something unique. The generic set follows in the base's own order.
    options.push_back("Trigger shoot animation");
    return LameMob::DebugOptions(options);
}

void Skeleton::DebugAction(int index)
{
    if(index == 0)
    {
        TriggerShootDraw();
        return;
    }
    // Everything after the bow is the base set, shifted by one.
    LameMob::DebugAction(index - 1);
}

void Skeleton::TriggerShootDraw()
{
    // Ignore the request while a draw is already running: the point of the button is to see the
    // animation from the start, and restarting a draw that is halfway through would show it from
    // the wrong place. A dead skeleton has no bow to draw.
    if(killed == true || shootDrawing == true)
        return;

    shootDrawing = true;
    shootDraw = 0.0f;
    shootRecoil = 0.0f;
    // Nothing is aiming this one, so the arrow it looses goes out along the skeleton's own facing
    // instead of at the player. The mob is not turned onto the player either -- see ShootArrow.
    shootAimed = false;
}

void Skeleton::BoxCollision()
{
    for(int x = 0; x < (int)world->mSkeletons.size(); x++)
    {
        Skeleton *Other = world->mSkeletons[x];
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

void Skeleton::TakeDamage(float damage, float power, float dt)
{
    // No armour: a skeleton cannot wear any, so unlike the zombie there is no FetchArmorPoints here.
    ApplyKickDamage(damage, power, dt, 1, &animLegAngleT);
}

void Skeleton::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{
    explosionPos = point;
    ApplyKickDamage(damage, power, dt, 2, NULL);
}

void Skeleton::ShootArrow(CraftWorld* myWorld, bool aimed)
{
    Vector3 eye(position.x, position.y + EYE_HEIGHT, position.z);

    // The shot leaves along the mob's own facing: the body's heading plus however far the head is
    // turned past it, which is exactly where the bow is pointing. The ranged AI turns both onto the
    // player well inside the 0.65 s draw that precedes every shot, so an aimed shot is still aimed --
    // but it is aimed by *looking*, not by the arrow bending toward the player, and the mob is never
    // turned to make a shot look right. A draw that nothing was aiming (the debug stick's "Trigger
    // shoot animation") therefore goes out in front of wherever the skeleton happened to be facing.
    const float lookYaw = mainAngle + headAngleY;

    // Pitch. The skeleton only ever shoots at the player, so when the AI is driving the shot the
    // arrow keeps its real elevation: the angle up to the player's chest, plus the lift that pays for
    // the drop, because the arrow is ballistic and a flat aim at bow range lands short. An unaided
    // draw has no range to solve for and simply flies level.
    float pitch = 0.0f;
    if(aimed == true)
    {
        Vector3 target(myWorld->playerPos.x, myWorld->playerPos.y - 0.55f, myWorld->playerPos.z);
        Vector3 toTarget = target - eye;
        const float flat = sqrtf(toTarget.x*toTarget.x + toTarget.z*toTarget.z);
        if(flat > 0.25f)
            pitch = atanf(toTarget.y / flat) + flat * 0.035f;
    }

    Vector3 aim(cosf(pitch) * sinf(lookYaw), sinf(pitch), cosf(pitch) * cosf(lookYaw));

    Arrow2* arrow = new Arrow2(eye.x, eye.y, eye.z);
    arrow->SetTexture(TextureHelper::Instance()->GetTexture(TextureHelper::arrowProjectileTexture));
    arrow->SetVelocity(aim, 1.0f);
    // 1.7.3 on normal difficulty lands a skeleton arrow at 2, up to 4 on hard. This port has one
    // difficulty, so it is the normal value.
    arrow->damage = 2.0f;
    arrow->fromMob = true;
    myWorld->mArrows.push_back(arrow);

    const float sdx = position.x - myWorld->playerPos.x;
    const float sdy = position.y - myWorld->playerPos.y;
    const float sdz = position.z - myWorld->playerPos.z;
    SoundManager::Instance()->PlayBowSoundAt(sqrtf(sdx*sdx + sdy*sdy + sdz*sdz));
}

void Skeleton::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
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

            // The bow. UpdateHostileRangedAI walks the skeleton into range and reports the frame a
            // shot is due; there is no melee kick because the profile leaves attackRange at 0. That
            // report no longer fires on the spot -- it starts the draw, and the arrow leaves when the
            // draw completes, so the shot is telegraphed instead of appearing out of nothing.
            bool wantShoot = false;
            UpdateHostileRangedAI(dt, &wantShoot);

            if(wantShoot && killed == false && shootDrawing == false && shootRecoil <= 0.0f)
            {
                shootDrawing = true;
                shootDraw = 0.0f;
                // The AI asked for this one, so it is a real shot at the player and leaves with the
                // proper elevation. See ShootArrow.
                shootAimed = true;
            }

            if(shootDrawing == true)
            {
                if(killed == true)
                {
                    // Dropped dead mid-draw, so the string is let down and no arrow is loosed.
                    shootDrawing = false;
                    shootDraw = 0.0f;
                }
                else
                {
                    shootDraw += dt / SHOOT_DRAW_TIME;
                    if(shootDraw >= 1.0f)
                    {
                        shootDraw = 0.0f;
                        shootDrawing = false;
                        shootRecoil = 1.0f;
                        ShootArrow(myWorld, shootAimed);
                    }
                }
            }
            else if(shootRecoil > 0.0f)
            {
                shootRecoil -= dt / SHOOT_RECOIL_TIME;
                if(shootRecoil < 0.0f)
                    shootRecoil = 0.0f;
            }

            // A draw the ranged AI started has already been committed to a shot at the player, so it
            // keeps the body and the head on them for its whole length even if the AI lets go -- the
            // player stepping out of bow range, or out of aggro range altogether, must not turn a shot
            // that is half loosed into one out of the side of the skeleton's head. This is the *only*
            // thing a draw adds to the aim, and it is keyed on shootAimed, so it applies to the AI's
            // shots and never to the debug stick's.
            const bool committedShot = (shootAimed == true && (shootDrawing == true || shootRecoil > 0.0f));
            if(committedShot == true && kicked == false)
                AimAtPlayer(dt);

            // Look at the player while engaged (the same head maths as the zombie). `angred` on its own
            // is not enough: the ranged AI clears it the moment the mob is inside bow range and holds
            // its ground, which is exactly when it is aiming. `rangedEngaged` is that missing state.
            //
            // A draw is deliberately *not* in the plain condition: aiming is the AI's job, so a draw
            // started anywhere else -- the debug stick's "Trigger shoot animation" -- leaves the head
            // (and the body) exactly where they were. That shot follows the facing instead, so it still
            // reads correctly from any angle. See ShootArrow.
            if((angred == true || rangedEngaged == true || committedShot == true) && kicked == false)
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

            // Hand idle anim (the same slight sway the zombie has).
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

void Skeleton::Render(Frustum &camFrustum, float dt)
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
            // The skeleton's skin is the second one here with genuinely unpainted texels: the ribs and
            // the spine leave the gaps between them transparent, and a 2x12x2 limb is thin enough that
            // its front face can be empty while the far one carries the art. That needs the same two
            // pieces of state the chicken needs, for the same reason (Chicken2::Render spells it out):
            //
            //   * alpha test ON, threshold 0. With it off the transparent texels draw as their RGB,
            //     which for these PNGs is black, and they still write depth -- so the near face paints
            //     over the far one instead of being discarded.
            //   * backface culling OFF. Vanilla does not cull entity models, so a discarded near face
            //     reveals the far face. With culling left on (the global default) the far face is wound
            //     away from the camera and goes too, which is what turned the body's back into holes
            //     punched through to the world behind the mob.
            sceGuAlphaFunc(GU_GREATER, 0.0f, 0xff);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDisable(GU_CULL_FACE);

            // How far through the draw, and how hard the bow is still kicking from the last shot.
            // `shootDraw` is zeroed the instant the arrow leaves, so the two are never both live.
            const float shootPull = (shootDrawing == true) ? shootDraw : 0.0f;
            const float shootKick = shootRecoil;

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

            // Right arm -- the bow arm. 2x12x2 buffers rather than the zombie's 4x12x4. It tightens
            // upward through the draw and kicks with the release, which is the whole of the arm's
            // contribution: the box pivots on the shoulder, so it has no elbow to fold.
            sceGumPushMatrix();
            ScePspFVector3 loc3 = {0,PIXEL*4,PIXEL*6};
            sceGumTranslate(&loc3);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rHandAngle-animHandAngle + 0.10f*shootPull + 0.16f*shootKick);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->skeletonHandVertices);
            sceGumPopMatrix();

            // Left arm -- the string hand. It rises toward the bow as the string comes back.
            sceGumPushMatrix();
            ScePspFVector3 loc4 = {0,PIXEL*4,-PIXEL*6};
            sceGumTranslate(&loc4);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lHandAngle+animHandAngle + 0.22f*shootPull + 0.10f*shootKick);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->skeletonHandVertices);
            sceGumPopMatrix();

            sceGumPushMatrix();
            ScePspFVector3 loc5 = {0,-PIXEL*8,PIXEL*2};
            sceGumTranslate(&loc5);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(rLegAngle+animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->skeletonLegVertices);
            sceGumPopMatrix();

            sceGumPushMatrix();
            ScePspFVector3 loc6 = {0,-PIXEL*8,-PIXEL*2};
            sceGumTranslate(&loc6);

            sceGumRotateX(0.0f);
            sceGumRotateY(0.0f);
            sceGumRotateZ(lLegAngle-animLegAngle);

            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 36, 0, world->skeletonLegVertices);
            sceGumPopMatrix();

            // The bow. Until now the skeleton shot arrows out of an empty hand: nothing in Render ever
            // drew an item, so "no bow" was literal.
            //
            // The held-item mesh is a single flat 1x1 quad (GetItemVerts, one face at z = 0) and its
            // sprite is authored upside down -- every caller in the engine stands it up with its own
            // rotZ(PI), the first-person hand with rotZ ~= -PI and InventoryPlayer's third person with
            // rotZ(PI). That makes the in-plane orientation a two-flip problem, so this transform is
            // solved rather than guessed.
            //
            // RotX decides the *plane*, and RotZ decides the angle inside it. Both matter and they are
            // easy to confuse, so: RotZ rotates the sprite in its own plane, so it sets where the
            // upper limb points (the lean) and nothing else; only RotX turns the plane itself.
            //
            // RotX 1.5PI put the quad's normal along the mob's own forward, which lays the bow flat
            // across the chest with its string side tipped *inward* at the body -- read as the bow
            // facing the skeleton. 0 turns the plane a quarter turn about the vertical, which is the
            // arm's X: the face (and the opening) now point out along the side the arm is on, away
            // from the body, and the bow is edge-on to anyone standing in front. The limbs stay
            // vertical through it, because that plane turn is about the vertical axis.
            //
            // RotZ 1.375PI then leans the upright bow 22 degrees, upper limb up and outward, which is
            // the raised look. Measured, not eyeballed: `preview_skeleton_bow.py` prints the tip's
            // screen direction for a whole turn, and `_tmp_atlas/zoom/_bow_90.png` draws the plane
            // options, because a wrong value here is a single 0.125PI and looks like a bow hung off
            // the hand backwards.
            sceGumPushMatrix();
            ScePspFVector3 bowLoc = {0,PIXEL*4,PIXEL*6};
            sceGumTranslate(&bowLoc);
            sceGumRotateZ(rHandAngle-animHandAngle + 0.10f*shootPull + 0.16f*shootKick);

            // The draw. The bow stays put in the hand. It used to travel 3 px back up the arm as the
            // string was pulled, and lean a further 9 degrees into it, on the reasoning that a rigid
            // 2x12x2 arm has no elbow to fold so the bow moving was the only draw the rig could show.
            // It reads as the bow sliding out of the hand instead, so the bow is now fixed in position
            // and angle and the pull is told by the sprite itself: the item mesh changes through the
            // draw from bow_standby to the three pulling frames, which is what vanilla shows too. The
            // arm angles above still tighten and still kick, because those move the *arm*.
            ScePspFVector3 bowHand = {0,PIXEL*10,0};
            sceGumTranslate(&bowHand);
            sceGumRotateX(0.0f);
            // 1.375PI is the lean that leaves the bow's limbs upright (see the RotX/RotZ walk-through
            // above); kHeldBowSkeletonTilt is this hand's own solved angle, which lands the arrow
            // drawn inside the artwork on the arm's own axis -- the arm is out along rHandAngle, so
            // the artwork needs a quarter turn against its own diagonal plus the difference between
            // the two. The number is derived in Items.cpp, not tuned here, and it is *not* the angle
            // the player's hands use: the three hands put different rotations under the sprite.
            sceGumRotateZ(1.375f*PI + kHeldBowSkeletonTilt);

            // A weapon, not a held block: 0.42 is the generic held-item quad and leaves the bow
            // reading as a toy beside the skeleton. Shared with the player's hands, see kHeldBowScale.
            ScePspFVector3 bowScale = {kHeldBowScale,kHeldBowScale,kHeldBowScale};
            sceGumScale(&bowScale);

            TextureManager::Instance()->SetTextureModeulate(world->textureItemsId);
            world->drawBowInHand(BowDrawStage(shootPull),myLight);

            sceGumPopMatrix();

            DrawSetCulling(false);

            sceGuDisable(GU_BLEND);
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
