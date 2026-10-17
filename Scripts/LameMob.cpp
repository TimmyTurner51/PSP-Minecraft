#include "LameMob.h"
#include "CraftWorld2.h"
#include "SoundManager.h"
#include "Items.h"
#include "LameFunctions.h"
#include <Aurora/Graphics/TextureManager.h>
#include <Aurora/Graphics/Effects/Particle.h>

#define DEG_TO_RAD (PI / 180.0f)
#define PIXEL 1.0f / 16.0f
#define GRAVITY -6.8f
#define JUMPVELOCITY 4.6f

LameMob::LameMob(Vector3 mobPosition, int mobNumber)
{
    world = NULL;

    age = 0.0f;
    lifeTime = 0.0f;
    loveTimer = 0.0f;
    breedTimer = 0.0f;
    heartTimer = 0.0f;
    breedItemId = -1;
    fallDistance = 0.0f;
    lastFallDistance = 0.0f;
    spawnGrace = 3.0f;
    spawnLanded = false;
    lastDamageCause = CauseNone;

    position = mobPosition;
    number = mobNumber;

    colliderD = Vector3(0,0,0);
    colliderU = Vector3(0,0,0);

    legs = Vector3(0,0,0);
    body = Vector3(0,0,0);
    head = Vector3(0,0,0);
    foot = Vector3(0,0,0);

    SetCollideBox(Vector3(-1,1,-1),Vector3(1,1,1));
    bBox = BoundingBox(Vector3(0,0,0),Vector3(0,0,0));

    relPlayerPos = Vector3(0,0,0);
    velocity = Vector3(0,0,0);
    explosionPos = Vector3(-1,-1,-1);

    deathAngle = 0.0f;
    mainAngle = (rand()%360)/180.0f*PI;

    SPD = 1.0f;
    HP = 20.0f;

    // No bow until the subclass applies a profile with a ranged attack.
    rangedShotTimer = 0.0f;

    toDestroy = false;
    freezed = false;
    angred = false;
    rangedEngaged = false;
    moving = false;
    jumping = false;
    jumpReady = true;
    footInWater = false;
    footInLava = false;
    headInWater = false;
    headInLava = false;
    walkingOnGround = true;
    climbing = false;
    kicked = false;
    damaged = false;
    nearToPlayer = false;
    killed = false;
    haunting = false;

    friendly = true;

    // Default passive profile; subclass ApplyProfile() overrides.
    profile = MobProfiles::Cow;

    playDieSound = false;
    playHurtSound = false;
    playSaySound = false;
    playStepSound = false;

    kickTimer = 0.0f;
    damagedTimer = 0.0f;
    knockbackTimer = 0.0f;
    panicTimer = 0.0f;
    panicRepick = 0.0f;
    panicAngle = 0.0f;
    moveSpeedMul = 1.0f;
    burning = false;
    fireTime = 0.0f;
    fireDamageTimer = 0.0f;
    freezedTimer = 0.0f;
    freezedTimerMax = 60.0f;
    jumpTimer = 0.0f;
    checkCollisionTimer = 0.0f;
    deathTimer = 0.75f;
    sayTimer = 10.0f + (rand() % 160)/10.0f;
    updateLightTimer = 0.3f;

    hauntingTimer = 0.0f;
    hauntingTimerMax = 0.0f;
    hauntingAngle = 0.0f;
    hauntingPos = Vector3(0,0,0);

    fastDistanceToPlayer = 999.0f;
    distanceToPlayer = 0.0f;

    UpdateBBox();

    tickCactusDamage = 0.0f;
    touchingCactus = false;

    myLight = 1.0f;

    lastKickType = -1;
    mainTexture = -1;
    poseLock = false;
}

LameMob::~LameMob()
{

}

float LameMob::DistanceToPlayer()
{
    return abs(relPlayerPos.x-position.x) + abs(relPlayerPos.z-position.z) + abs(relPlayerPos.y-0.7-position.y)*0.89f;
}

unsigned int LameMob::FastDistanceToPlayer()
{
    return world->FastDistance2d(abs(position.x-world->playerPos.x),abs(position.z-world->playerPos.z)) + abs(relPlayerPos.y-(position.y-0.5))*0.7f;
}

float LameMob::PreciseDistanceToPlayer() /// REAL DISTANCE
{
    return sqrt((relPlayerPos.x-position.x)*(relPlayerPos.x-position.x) + (relPlayerPos.z-position.z)*(relPlayerPos.z-position.z));
}

float LameMob::DistanceToHauntingPos() /// TAXICAB DISTANCE
{
    return abs(hauntingPos.x-position.x) + abs(hauntingPos.z-position.z);
}

void LameMob::TryToJump()
{
    if (headInWater || headInLava)
    {
        if (walkingOnGround)
        {
            jumping = true;
        }
        else
        {
            velocity.y = 0.2 * JUMPVELOCITY;
            walkingOnGround = false;
        }
    }
    if ((!headInWater || !headInLava) && !walkingOnGround && (footInWater || footInLava))
    {
        velocity.y = 1.25 * JUMPVELOCITY;
        walkingOnGround = false;
    }
    if(walkingOnGround)
    {
        jumping = true;
    }
    else
    {
        jumping = false;
    }
}

void LameMob::TryToMove(Vector3 movePosition1, float speed)
{
    if(poseLock)
        return;

    Vector3 movePosition = position + Vector3(movePosition1.x*(speed+0.2),0.0f,movePosition1.z*(speed+0.2));
    Vector3 movePositionHead = movePosition + head;
    Vector3 movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(movePosition1.x*speed,0.0f,movePosition1.z*speed);

        moving = true;
    }
    else
    {
        if(jumpReady == true && (!world->SolidAtPointForEntity(movePositionHead,colliderU) || friendly))
        {
            TryToJump();
            jumpReady = false;
        }
    }

    movePosition = position + Vector3(movePosition1.x*(speed+0.2),0.0f,0.0f);
    movePositionHead = movePosition + head;
    movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(movePosition1.x*speed,0.0f,0);

        //moving = true;
    }
    else
    {
        if(jumpReady == true && (!world->SolidAtPointForEntity(movePositionHead,colliderU) || friendly))
        {
            TryToJump();
            jumpReady = false;
        }
    }

    movePosition = position + Vector3(0.0f,0.0f,movePosition1.z*(speed+0.2));
    movePositionHead = movePosition + head;
    movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(0.0f,0.0f,movePosition1.z*speed);

        //moving = true;
    }
    else
    {
        if(jumpReady == true && (!world->SolidAtPointForEntity(movePositionHead,colliderU) || friendly))
        {
            TryToJump();
            jumpReady = false;
        }
    }
}

void LameMob::TryToMoveWithoutJump(Vector3 movePosition1, float speed)
{
    if(poseLock)
        return;

    Vector3 movePosition = position + Vector3(movePosition1.x*(speed+0.2),0.0f,movePosition1.z*(speed+0.2));
    Vector3 movePositionHead = movePosition + head;
    Vector3 movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(movePosition1.x*speed,0.0f,movePosition1.z*speed);

        moving = true;
    }
    else
    {
        moving = false;
    }

    movePosition = position + Vector3(movePosition1.x*(speed+0.2),0.0f,0.0f);
    movePositionHead = movePosition + head;
    movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(movePosition1.x*speed,0.0f,0);
    }

    movePosition = position + Vector3(0.0f,0.0f,movePosition1.z*(speed+0.2));
    movePositionHead = movePosition + head;
    movePositionFoot = movePosition + Vector3(foot.x,foot.y+0.1,foot.z);

    if (!world->SolidAtPointForEntity(movePosition,colliderD) && !world->SolidAtPointForEntity(movePositionHead,colliderU) && !world->SolidAtPointForEntity(movePositionFoot,colliderD))
    {
        position += Vector3(0.0f,0.0f,movePosition1.z*speed);
    }
}

void LameMob::SetPosition(Vector3 newPosition)
{
    position = newPosition;
}

void LameMob::SetCollideBox(Vector3 collider1, Vector3 collider2)
{
    baseColliderD = collider1;
    baseColliderU = collider2;
    const float s = ModelScale();
    collider1 = Vector3(collider1.x * s, collider1.y * s, collider1.z * s);
    collider2 = Vector3(collider2.x * s, collider2.y * s, collider2.z * s);
    colliderD = collider1;
    colliderU = collider2;

    foot = Vector3(0,-collider1.y,0);
    head = Vector3(0,collider2.y,0);
}

void LameMob::UpdateBBox()
{
    bBox = BoundingBox(Vector3(position.x-colliderD.x*1.45,position.y-colliderD.y*1.3,position.z-colliderD.z*1.45),Vector3(position.x+colliderU.x*1.45,position.y+colliderU.y*1.3,position.z+colliderU.z*1.45));
}

BoundingBox LameMob::HitBox() const
{
    // A little larger than the body so a glancing shot still counts, without
    // becoming the punch pad (MobAttackBox) that is meant for a short melee ray.
    const float pad = 0.15f;
    const float s = ModelScale();

    float minx = position.x - colliderD.x;
    float miny = position.y - colliderD.y;
    float minz = position.z - colliderD.z;
    float maxx = position.x + colliderU.x;
    float maxy = position.y + colliderU.y;
    float maxz = position.z + colliderU.z;

    // Vanilla living size, standing on the feet. The spider's collider is narrower
    // than the model (0.75 vs 1.4); fireWidth/Height are the Java setSize numbers
    // and are what you actually aim at.
    const float halfW = profile.fireWidth * s * 0.5f;
    const float tall = profile.fireHeight * s;
    const float feetY = position.y - colliderD.y;
    if(halfW > 0.0f && tall > 0.0f)
    {
        if(position.x - halfW < minx) minx = position.x - halfW;
        if(position.z - halfW < minz) minz = position.z - halfW;
        if(position.x + halfW > maxx) maxx = position.x + halfW;
        if(position.z + halfW > maxz) maxz = position.z + halfW;
        if(feetY < miny) miny = feetY;
        if(feetY + tall > maxy) maxy = feetY + tall;
    }

    return BoundingBox(Vector3(minx - pad, miny - pad, minz - pad),
                       Vector3(maxx + pad, maxy + pad, maxz + pad));
}

void LameMob::BoxCollision()
{

}

void LameMob::TakeDamage(float damage, float power, float dt)
{

}

void LameMob::TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point)
{

}

void LameMob::Update(CraftWorld *myWorld, Vector3 playerPos, float dt)
{

}

void LameMob::Render(Frustum &camFrustum, float dt)
{

}

void LameMob::ApplyProfile(const MobProfile& p)
{
    profile = p;
    HP = p.hp;
    SPD = p.spd;
    freezedTimerMax = p.freezedDestroy;
    friendly = !p.hostile;
    // A caller that forgets to name its mob still gets a usable title rather than a null string.
    debugName = "Mob";
}

// The options every living mob can be poked with. Deliberately small and blunt: this is a testing
// tool, and "kill it" / "heal it" are the two things you most often want while chasing a rendering or
// AI bug. A mob with real behaviour of its own appends to this list.
int LameMob::DebugOptions(std::vector<std::string>& options)
{
    options.push_back("Set on fire");
    options.push_back("Extinguish");
    options.push_back("Damage 5");
    options.push_back("Kill");
    options.push_back("Heal to full");
    return (int)options.size();
}

void LameMob::DebugAction(int index)
{
    switch(index)
    {
        case Debug_SetFire:
            SetOnFire(8.0f);
            break;

        case Debug_Extinguish:
            burning = false;
            fireTime = 0.0f;
            break;

        case Debug_Damage5:
            // Straight to HP rather than through TakeDamage. TakeDamage is the mob's own hit
            // reaction, so it also applies knockback and a hurt flash -- which means the thing you
            // are looking at slides away from you every time you poke it. The flash is kept because
            // seeing that the damage registered is the point; the shove is not.
            HP -= 5.0f;
            damaged = true;
            damagedTimer = 0.5f;
            break;

        case Debug_Kill:
            // Zero HP and let the mob's own Update notice, via CheckJustKilled. Setting `killed`
            // here instead would skip the death clip, the death sound and the drops, all of which
            // live in each mob's own death branch.
            HP = 0.0f;
            lastDamageCause = CauseNone;
            damaged = true;
            damagedTimer = 0.5f;
            break;

        case Debug_Heal:
            HP = profile.hp;
            damagedTimer = 0.0f;
            break;

        default:
            break;
    }
}

void LameMob::RenderFire()
{
    RenderFire(profile.fireWidth * ModelScale(), profile.fireHeight * ModelScale());
}

void LameMob::ApplyKickDamage(float damage, float power, float dt, int kickType, float* animLegAngleT)
{
    HP -= damage;
    if(poseLock)
    {
        damaged = true;
        damagedTimer = 0.5f;
        playHurtSound = true;
        lastDamageCause = (kickType == 2) ? CauseExplosion : CauseHit;
        return;
    }
    kicked = true;
    damaged = true;
    damagedTimer = power * 0.6f + 0.15f;
    knockbackTimer = damagedTimer;
    velocity.y = (power + (kickType == 2 ? 0.25f : 0.15f)) * JUMPVELOCITY;
    walkingOnGround = false;
    if(animLegAngleT != NULL)
        *animLegAngleT = 0.5f;
    position += velocity * dt;
    playHurtSound = true;
    lastKickType = kickType;
    lastDamageCause = (kickType == 2) ? CauseExplosion : CauseHit;

    if(profile.panicSpeedMul > 0.0f)
    {
        panicTimer = profile.panicTime;
        panicRepick = 0.0f;
    }
}

bool LameMob::BeginMobUpdate(CraftWorld* myWorld, Vector3 playerPos, float dt)
{
    // Counted here rather than in the physics step, so it runs for every mob the game updates and is
    // the honest answer to "how long has this mob existed" even when it is not in range of a player.
    lifeTime += dt;

    world = myWorld;
    relPlayerPos = playerPos;
    moveSpeedMul = 1.0f;
    distanceToPlayer = DistanceToPlayer();
    fastDistanceToPlayer = FastDistanceToPlayer();
    if(myWorld == NULL || !myWorld->ColumnLoadedAt((int)position.x, (int)position.z))
        return false;
    return true;
}

void LameMob::UpdateDeathPose(float dt)
{
    if(killed == false)
        return;
    if(deathAngle < PI / 2)
        deathAngle += (PI / 2) * dt * 1.75f + ((PI / 2 - deathAngle) / 24.0f);
    if(deathAngle > PI / 2)
        deathAngle = PI / 2;
    deathTimer -= dt;
}

bool LameMob::CheckJustKilled()
{
    if(HP <= 0.0f && killed == false)
    {
        killed = true;
        // The play-state loops already gate a death clip on this flag for every mob that has one
        // (zombie, creeper); without setting it here only the creeper ever made a sound when it
        // died, because it sets the flag itself in its own death branch.
        playDieSound = true;
        // life is the real elapsed lifetime, not `age` (the breeding timer, 0 for adults); fall and
        // grace are what the fall branch saw. Together they answer the one question this line exists
        // for: was this a death during the spawn grace, or an ordinary fall a long time after it?
        Aurora::Utils::Logger::Instance()->LogMessage("mob %d died cause=%d hp=%.1f pos=%.1f,%.1f,%.1f age=%.0f life=%.1f fall=%.1f grace=%.2f\n",
            number, lastDamageCause, HP, position.x, position.y, position.z, age, lifeTime, lastFallDistance, spawnGrace);
        return true;
    }
    return false;
}

void LameMob::UpdateMobTimers(float dt, const BoundingBox& playerHitBox)
{
    if(dt >= 0.1f)
        return;

    sayTimer += dt;
    if(sayTimer > 10.0f)
    {
        if(rand() % 2 == 0)
            playSaySound = true;
        sayTimer = 0.0f;
    }

    jumpTimer += dt;
    if(jumpTimer > 0.2f)
    {
        jumpReady = true;
        jumpTimer = 0.0f;
    }

    if(knockbackTimer > 0.0f)
    {
        knockbackTimer -= dt;
        if(knockbackTimer < 0.0f)
            knockbackTimer = 0.0f;
    }

    if(damaged == true)
    {
        if(damagedTimer > 0.0f)
            damagedTimer -= dt;
        else
        {
            damagedTimer = 0.0f;
            damaged = false;
        }
    }

    if(playerHitBox.intersect(bBox))
    {
        float angle2 = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z);
        TryToMove(Vector3(sinf(angle2), 0.0f, cosf(angle2)), 1.0f * dt);
    }
    nearToPlayer = (distanceToPlayer < profile.playerBumpRange);

    if(world != NULL && world->PlayerInWater(position) == true && jumpReady == true && killed == false)
    {
        TryToJump();
        jumpReady = false;
    }

    if(profile.sunBurns && killed == false && world != NULL)
    {
        const bool inSun = (world->worldDayTime > 6 && world->worldDayTime < 21 &&
                            world->GetShadowLevel(position.x, position.y - 0.2f, position.z) == 0) &&
                           world->worldWeather->GetWeatherType() == 0;
        if(inSun && world->worldWeather != NULL)
            SetOnFire(8.0f);
    }
}

// Shortest-path angular interpolation for a mob's heading.
//
// The old form was `mainAngle += sinf(target - mainAngle) * rate`, which cannot reach every
// direction: sinf peaks at a quarter turn and falls to *exactly zero* at half a turn, so a target
// directly behind the mob produces no turn at all (and one at 179 deg turns at 0.0175 rad/frame,
// i.e. never). Wrapping the difference into [-PI, PI] first makes the whole circle reachable, and
// the step is capped at `rate` so the turn speed still matches how it used to feel for the small
// and medium turns that did work.
static float TurnToward(float current, float target, float rate)
{
    float diff = target - current;
    while(diff > PI)  diff -= 2.0f * PI;
    while(diff < -PI) diff += 2.0f * PI;

    float step = diff * rate;
    if(step > rate)  step = rate;
    if(step < -rate) step = -rate;
    return current + step;
}

static void PickWanderTarget(LameMob* m, float radius)
{
    if(rand() % 100 < 80)
    {
        m->hauntingTimerMax = 4.0f + (rand() % 27) / 5.0f;
        if(rand() % 5 == 0)
            m->hauntingTimerMax = 1.0f + (rand() % 20) / 10.0f;

        m->hauntingPos.x = m->position.x - radius + (rand() % (int)(radius * 20.0f)) / 10.0f;
        m->hauntingPos.z = m->position.z - radius + (rand() % (int)(radius * 20.0f)) / 10.0f;

        if(m->world != NULL)
        {
            if(m->hauntingPos.x > m->world->worldLimit - 3)
                m->hauntingPos.x = m->world->worldLimit - 3;
            if(m->hauntingPos.x < 2)
                m->hauntingPos.x = 2;
            if(m->hauntingPos.z > m->world->worldLimit - 3)
                m->hauntingPos.z = m->world->worldLimit - 3;
            if(m->hauntingPos.z < 2)
                m->hauntingPos.z = 2;
        }
        m->hauntingAngle = ATAN2_fast(m->position.x - m->hauntingPos.x, m->position.z - m->hauntingPos.z) + PI;
    }
    else
    {
        m->hauntingTimerMax = 2.0f + (rand() % 13) / 5.0f;
        m->hauntingAngle = (rand() % 360) / 180.0f * PI;
        m->hauntingPos = Vector3(-1, -1, -1);
    }
}

void LameMob::UpdateWander(float dt)
{
    moving = false;
    if(killed == true || kicked == true || angred == true)
        return;

    if(hauntingTimerMax == 0.0f)
        hauntingTimerMax = 5.0f + rand() % 6;

    if(hauntingTimer < hauntingTimerMax)
        hauntingTimer += dt;
    if(hauntingTimer >= hauntingTimerMax)
    {
        hauntingTimer = 0.0f;
        PickWanderTarget(this, profile.wanderRadius);
    }

    if(hauntingPos.x != 0 && hauntingPos.z != 0)
    {
        if(DistanceToHauntingPos() > 0.5f || (hauntingPos.x == -1 && hauntingPos.z == -1))
        {
            if(hauntingPos != Vector3(-1, -1, -1))
            {
                if(mainAngle != hauntingAngle)
                    mainAngle = TurnToward(mainAngle, hauntingAngle, 0.07f);
                TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * dt);
            }
            else
            {
                if(mainAngle != hauntingAngle)
                    mainAngle = TurnToward(mainAngle, hauntingAngle, 0.07f);
                if(fabsf(mainAngle - hauntingAngle) < 0.02f)
                    mainAngle = hauntingAngle;
            }
        }
        else
        {
            hauntingPos.x = 0;
            hauntingPos.z = 0;
        }
    }
}

bool LameMob::UpdateTemptFollow(float dt, int heldItemId, int temptItemId, float* headAngleY)
{
    if(killed == true || kicked == true || temptItemId < 0)
        return false;
    if(heldItemId != temptItemId || fastDistanceToPlayer >= 10.0f)
    {
        if(headAngleY != NULL)
            *headAngleY = 0.0f;
        return false;
    }

    moving = false;
    hauntingTimer = 0.0f;
    float playerAngle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z) + PI;
    if(mainAngle != playerAngle)
        mainAngle = TurnToward(mainAngle, playerAngle, 0.07f);
    if(headAngleY != NULL)
        *headAngleY = playerAngle - mainAngle;

    const float stopRange = (profile.temptStopRange > 0.0f) ? profile.temptStopRange : 2.3f;
    if(distanceToPlayer > stopRange)
    {
        float mul = 1.0f;
        if(profile.temptSpeedMul > 1.0f && distanceToPlayer > profile.temptRunRange)
            mul = profile.temptSpeedMul;
        moveSpeedMul = mul;
        TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * mul * dt);
    }
    return true;
}

bool LameMob::UpdatePanic(float dt)
{
    if(profile.panicSpeedMul <= 0.0f || killed == true)
        return false;

    // Java EntityAIPanic also runs while the mob is burning.
    if(burning && panicTimer < 0.5f)
        panicTimer = 0.5f;

    if(panicTimer <= 0.0f)
        return false;

    panicTimer -= dt;
    if(panicTimer < 0.0f)
        panicTimer = 0.0f;

    // Knockback still owns movement until the mob lands.
    if(kicked == true)
        return true;

    moving = false;
    hauntingTimer = 0.0f;
    hauntingPos = Vector3(0, 0, 0);

    panicRepick -= dt;
    if(panicRepick <= 0.0f)
    {
        // Flee heading: away from the player, +/- 60 degrees.
        const float awayAngle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z);
        const float spread = ((rand() % 121) - 60) / 180.0f * PI;
        panicAngle = awayAngle + spread;
        panicRepick = 0.8f + (rand() % 8) / 10.0f;
    }

    mainAngle = TurnToward(mainAngle, panicAngle, 0.2f);
    moveSpeedMul = profile.panicSpeedMul;
    TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * profile.panicSpeedMul * dt);
    return true;
}

void LameMob::UpdateHostileAI(float dt, bool* callAttack)
{
    if(callAttack != NULL)
        *callAttack = false;
    if(killed == true || profile.hostile == false || world == NULL)
        return;

    const bool canAggro = (world->gameModeWorld != 1) && (world->HP > 0);
    if(canAggro && profile.attackRange > 0.0f && distanceToPlayer <= profile.attackRange)
    {
        if(callAttack != NULL)
            *callAttack = true;
    }

    if(canAggro && fastDistanceToPlayer < profile.aggroRange)
    {
        haunting = false;
        hauntingTimer = 0.0f;
        hauntingTimerMax = 0.0f;
        hauntingPos = Vector3(0, 0, 0);
        hauntingAngle = 0.0f;

        if(distanceToPlayer > profile.stopChaseRange)
            angred = true;
        else
        {
            // In melee range: stop walking but stay engaged (do not resume wander).
            angred = false;
            moving = false;
        }
    }
    else
    {
        angred = false;
        haunting = true;
        moving = false;
    }

    if(angred == true && kicked == false)
    {
        mainAngle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z) + PI;
        TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * dt);
        moving = true;
    }
    else if(haunting == true)
    {
        UpdateWander(dt);
    }
}

// How fast a bow mob turns its body onto its target, in radians per second. Fast enough that a half
// turn finishes inside the 0.65 s draw that precedes every skeleton shot, slow enough that it reads as
// turning rather than teleporting.
static const float kRangedTurnRate = 6.0f;

void LameMob::AimAtPlayer(float dt)
{
    // Turned at a limited rate rather than snapped: 1.7.3 aims through EntityLiving.faceEntity, which
    // clamps how far a mob may turn per tick.
    const float aimAngle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z) + PI;
    float turn = aimAngle - mainAngle;
    while(turn > PI) turn -= 2.0f * PI;
    while(turn < -PI) turn += 2.0f * PI;
    const float maxTurn = kRangedTurnRate * dt;
    if(turn > maxTurn) turn = maxTurn;
    if(turn < -maxTurn) turn = -maxTurn;
    mainAngle += turn;
}

void LameMob::UpdateHostileRangedAI(float dt, bool* callShoot)
{
    if(callShoot != NULL)
        *callShoot = false;
    rangedEngaged = false;
    if(killed == true || profile.hostile == false || world == NULL || profile.rangedRange <= 0.0f)
        return;

    const bool canAggro = (world->gameModeWorld != 1) && (world->HP > 0);

    if(canAggro && fastDistanceToPlayer < profile.aggroRange)
    {
        haunting = false;
        hauntingTimer = 0.0f;
        hauntingTimerMax = 0.0f;
        hauntingPos = Vector3(0, 0, 0);
        hauntingAngle = 0.0f;

        if(distanceToPlayer > profile.rangedRange)
        {
            // Out of bow range: close the gap, same as the melee chase.
            angred = true;
            if(kicked == false)
            {
                mainAngle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z) + PI;
                TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * dt);
                moving = true;
            }
        }
        else
        {
            // In range: 1.7.3 EntityAIArrowAttack stops the mob and shoots from where it stands.
            // (Strafing and backing away were added to later versions, not this one.)
            angred = false;
            moving = false;

            // ...but it does not stop aiming, and the arrow leaves along the skeleton's own facing, so
            // a mob that fires while still looking the way it happened to walk in throws its arrow out
            // of the side of its head. The body is turned onto the player here, and the draw that now
            // precedes every shot is 0.65 s long, enough for even a half turn to complete before the
            // arrow goes.
            rangedEngaged = true;
            if(kicked == false)
                AimAtPlayer(dt);
        }

        // The reload runs whether or not the mob is in range, but a shot only leaves the bow once it
        // is. That mirrors EntityAIArrowAttack: the delay is between attempts, not between hits.
        rangedShotTimer -= dt;
        if(rangedShotTimer <= 0.0f)
        {
            rangedShotTimer = profile.rangedCooldownMin +
                (profile.rangedCooldownMax - profile.rangedCooldownMin) * (rand() / (float)RAND_MAX);
            if(distanceToPlayer <= profile.rangedRange && callShoot != NULL)
                *callShoot = true;
        }
    }
    else
    {
        angred = false;
        haunting = true;
        moving = false;
        // Out of range: do not bank up a fully-charged shot, so re-engaging is never instant.
        if(rangedShotTimer > profile.rangedCooldownMin)
            rangedShotTimer = profile.rangedCooldownMin;
    }
}

void LameMob::UpdateKnockbackMove(float dt)
{
    if(kicked == false)
        return;

    haunting = false;
    hauntingTimer = 0.0f;
    hauntingPos.x = 0;
    hauntingPos.z = 0;

    float angle;
    if(lastKickType != 2)
        angle = ATAN2_fast(position.x - relPlayerPos.x, position.z - relPlayerPos.z) + PI;
    else
        angle = ATAN2_fast(position.x - explosionPos.x, position.z - explosionPos.z) + PI;

    TryToMoveWithoutJump(Vector3(sinf(angle + PI), 0.0f, cosf(angle + PI)),
                         (SPD + 0.18f) * 9.2f * (knockbackTimer * knockbackTimer) * dt);

    if((walkingOnGround == true || footInLava || footInWater) && knockbackTimer <= profile.knockbackEndTimer)
    {
        kicked = false;
        // Animals re-pick a wander target after landing; hostiles leave it to AI.
        if(profile.hostile == false)
        {
            hauntingTimerMax = 3.0f + rand() % 3;
            PickWanderTarget(this, 5.0f);
        }
    }
}

void LameMob::UpdateLegAnim(float dt, float* animLegAngleT, float* animLegAngle)
{
    if(animLegAngleT == NULL || animLegAngle == NULL || killed == true)
        return;

    if(kicked == true)
    {
        *animLegAngleT += PI * dt * profile.legKickRate;
        if(*animLegAngleT >= PI * 2.0f)
            *animLegAngleT -= PI * 2.0f;
        *animLegAngle = sinf(*animLegAngleT) * profile.legKickAmp;
    }
    else if(moving == true)
    {
        const float mul = (moveSpeedMul > 1.0f) ? moveSpeedMul : 1.0f;
        *animLegAngleT += PI * dt * profile.legWalkRate * mul;
        if(*animLegAngleT >= PI * 2.0f)
            *animLegAngleT -= PI * 2.0f;
        float amp = profile.legWalkAmp * (1.0f + (mul - 1.0f) * 0.35f);
        if(profile.legKickAmp > 0.0f && amp > profile.legKickAmp)
            amp = profile.legKickAmp;
        *animLegAngle = sinf(*animLegAngleT) * amp;
    }
    else
    {
        if(*animLegAngleT > 0 && *animLegAngleT < PI / 2)
        {
            *animLegAngleT -= *animLegAngleT * 0.3f;
            if(*animLegAngleT < 0.05f)
                *animLegAngleT = 0;
        }
        if(*animLegAngleT >= PI / 2 && *animLegAngleT < PI)
        {
            *animLegAngleT += (*animLegAngleT - PI) * 0.3f;
            if(*animLegAngleT > PI - 0.05f)
                *animLegAngleT = 0;
        }
        if(*animLegAngleT > PI && *animLegAngleT < PI * 1.5f)
        {
            *animLegAngleT -= (*animLegAngleT - PI) * 0.3f;
            if(*animLegAngleT < PI + 0.05f)
                *animLegAngleT = 0;
        }
        if(*animLegAngleT >= PI * 1.5f && *animLegAngleT < PI * 2)
        {
            *animLegAngleT += (*animLegAngleT - PI * 2) * 0.3f;
            if(*animLegAngleT > PI * 2 - 0.05f)
                *animLegAngleT = 0;
        }
        *animLegAngle = sinf(*animLegAngleT) * profile.legWalkAmp;
    }
}

void LameMob::UpdateMobPhysics(float dt)
{
    if(world == NULL)
        return;

    if(spawnGrace > 0.0f)
        spawnGrace -= dt;

    Vector3 footPosition = position + foot;
    Vector3 legsPosition = position + legs;
    Vector3 headPosition = position + head;

    footInWater = world->PlayerInWater(legsPosition);

    if(world->PlayerInWater(headPosition))
    {
        if(velocity.y > -3)
            velocity.y += (GRAVITY / 3.0f) * dt;
        else
            velocity.y = -3;
        headInWater = true;
    }
    else
    {
        headInWater = false;
        velocity.y += (GRAVITY * 1.3f * dt) * 1.6f;
    }

    if(world->PlayerInLava(legsPosition))
    {
        footInLava = true;
        HP -= 0.5f;
        damaged = true;
        damagedTimer = 0.55f;
        lastDamageCause = CauseLava;
    }
    else
        footInLava = false;

    if(world->PlayerInLava(headPosition))
    {
        velocity.y += (GRAVITY / 3.0f) * dt;
        headInLava = true;
        HP -= 1;
        damaged = true;
        damagedTimer = 0.55f;
        lastDamageCause = CauseLava;
    }
    else
    {
        headInLava = false;
        velocity.y += GRAVITY * dt;
    }

    // A climbing mob hangs on the wall the way a vanilla ladder does. The climb step is a fixed
    // 1.1 blocks per second added after the physics, so without zeroing the fall here gravity eats
    // a slice of it first -- about 20% of the step on a 60 Hz frame but 40% on a 30 Hz PSP frame,
    // which made the climb rate depend on how fast the game happened to be running. Floating mobs
    // keep their buoyancy.
    if(climbing == true && !(footInWater || headInWater))
        velocity.y = 0.0f;

    UpdateFire(dt);

    if(profile.checkCactus)
    {
        touchingCactus = false;
        if(world->PlayerCollisionWithCactus(headPosition) || world->PlayerCollisionWithCactus(legsPosition))
            touchingCactus = true;
    }

    if(world->SolidAtPointForEntity(footPosition, colliderD) || world->SolidAtPointForEntity(headPosition, colliderU))
    {
        if(kicked == true && velocity.y > 0.15f)
            walkingOnGround = false;
        else
        {
            walkingOnGround = true;
            if(world->SolidAtPointForEntity(headPosition, colliderU))
            {
                int blockIn = (int)(headPosition.y);
                position.y = (float)(blockIn - colliderU.y - 0.05);
            }
            // The foot clamp snaps a mob onto the top of the block its feet sample as solid. For a
            // climbing mob those feet are still buried in the block it pushed off from -- or in the
            // face of the wall it is holding -- so the clamp would undo the climb step every frame
            // and leave it stuck at the foot of the hill. The climb owns the height instead; the
            // head clamp above still runs, so it cannot pull itself through a ceiling.
            if(climbing == false && world->SolidAtPointForEntity(footPosition, colliderD))
            {
                int blockOn = (int)(footPosition.y);
                position.y = (float)(blockOn + 1 + colliderD.y - 0.05);
                if(profile.checkCactus && world->GetBlock(footPosition.x, footPosition.y, footPosition.z) == CactusBlock::getID())
                    touchingCactus = true;
            }

            // The mob is standing on something, so it has now landed at least once in its life. The
            // very first landing is exempt from fall damage below: a mob is spawned a little above
            // the ground, and the drop that brings it down is not a fall it should be able to die of.
            // The spawn grace usually covers it, but the grace is a timer and a long drop or a slow
            // frame can outlast it -- this is the part that cannot.
            const bool firstLanding = !spawnLanded;
            spawnLanded = true;

            // Java fall damage: ceil(fallDistance - 3) on landing. Distance based, so a
            // long frame or a spawn drop can't turn into a lethal landing speed.
            if(fallDistance > 3.0f && spawnGrace <= 0.0f && !firstLanding && !world->PlayerInWater(footPosition))
            {
                // Recorded before the reset below, because the death line is logged on a later frame
                // by which time fallDistance has already been cleared. Without this the log would
                // always say a fatal fall was 0.0 blocks.
                lastFallDistance = fallDistance;
                HP -= ceilf(fallDistance - 3.0f);
                damaged = true;
                damagedTimer = profile.hostile ? 0.75f : 0.5f;
                playHurtSound = true;
                lastDamageCause = CauseFall;
            }
            fallDistance = 0.0f;
            velocity.y = 0.0f;

            if(jumping == true)
            {
                velocity.y = 1.55f * JUMPVELOCITY;
                jumping = false;
                walkingOnGround = false;
            }
        }
    }
    else
        walkingOnGround = false;

    if(footInWater || headInWater || footInLava || headInLava)
        fallDistance = 0.0f;

    float dy = velocity.y * dt;
    // Long frame + fast fall: probe the path in < 1 block steps so the feet can't
    // skip over a floor between two checks.
    if(dy < -0.9f)
    {
        for(float s = -0.9f; s > dy; s -= 0.9f)
        {
            if(world->SolidAtPointForEntity(Vector3(footPosition.x, footPosition.y + s, footPosition.z), colliderD))
            {
                dy = s;
                break;
            }
        }
    }
    if(dy < 0.0f && walkingOnGround == false)
        fallDistance -= dy;

    position.x += velocity.x * dt;
    position.y += dy;
    position.z += velocity.z * dt;

    // Cactus tick damage (zombie/creeper path).
    if(profile.checkCactus && touchingCactus)
    {
        tickCactusDamage += dt;
        if(tickCactusDamage >= 0.5f)
        {
            HP -= 1;
            damaged = true;
            damagedTimer = 0.55f;
            tickCactusDamage -= 0.5f;
            lastDamageCause = CauseCactus;
        }
    }
    else
        tickCactusDamage = 0.0f;
}

bool LameMob::UpdateOutOfRangeFreeze(float dt)
{
    if(dt >= 0.1f)
        return false;
    // Keep growing up / cooling down while the player is away; babies never despawn.
    if(age < 0.0f)
    {
        float a = age + dt;
        SetAge(a >= 0.0f ? 0.0f : a);
        return false;
    }
    if(age > 0.0f)
        age = (age - dt > 0.0f) ? age - dt : 0.0f;
    if(poseLock)
        return false;
    freezedTimer += dt;
    if(freezedTimer >= freezedTimerMax)
    {
        toDestroy = true;
        return true;
    }
    return false;
}

static void ConsiderPunchMob(LameMob* mob, float power, const Vector3& origin, const Vector3& dir, float reach, LameMob** best, float* bestT, float* bestPower)
{
    // Gate on the knockback, not the hurt flash: sun/fire/lava/cactus damage keeps
    // `damaged` set most of the time and used to make burning mobs unhittable.
    if(mob == NULL || mob->killed == true || mob->knockbackTimer > 0.0f)
        return;

    float tHit = 0.0f;
    if(RayIntersectAABB(origin, dir, MobAttackBox(mob->bBox), reach, &tHit) && tHit < *bestT)
    {
        *bestT = tHit;
        *best = mob;
        *bestPower = power;
    }
}

static void ConsiderArrowMob(LameMob* mob, const Vector3& origin, const Vector3& dir, float reach, LameMob** best, float* bestT)
{
    if(mob == NULL || mob->killed == true || mob->toDestroy == true)
        return;

    float tHit = 0.0f;
    if(RayIntersectAABB(origin, dir, mob->HitBox(), reach, &tHit) && tHit < *bestT)
    {
        *bestT = tHit;
        *best = mob;
    }
}

LameMob* FindMobAlongSegment(CraftWorld* world, const Vector3& a, const Vector3& b, float* hitT)
{
    LameMob* best = NULL;
    float bestT = 1.0e9f;
    Vector3 delta(b.x - a.x, b.y - a.y, b.z - a.z);
    const float len = sqrtf(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
    Vector3 dir;
    float reach;
    if(len > 0.0001f)
    {
        dir = Vector3(delta.x / len, delta.y / len, delta.z / len);
        reach = len;
    }
    else
    {
        // A step that did not move still has to hit a body it is sitting inside.
        dir = Vector3(0.0f, 1.0f, 0.0f);
        reach = 0.02f;
    }

    if(world != NULL)
    {
        for(unsigned int f = 0; f < world->mZombies.size(); f++)
            ConsiderArrowMob(world->mZombies[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mCows.size(); f++)
            ConsiderArrowMob(world->mCows[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mSheeps.size(); f++)
            ConsiderArrowMob(world->mSheeps[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mCreepers.size(); f++)
            ConsiderArrowMob(world->mCreepers[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mPigs.size(); f++)
            ConsiderArrowMob(world->mPigs[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mChickens.size(); f++)
            ConsiderArrowMob(world->mChickens[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mSpiders.size(); f++)
            ConsiderArrowMob(world->mSpiders[f], a, dir, reach, &best, &bestT);
        for(unsigned int f = 0; f < world->mSkeletons.size(); f++)
            ConsiderArrowMob(world->mSkeletons[f], a, dir, reach, &best, &bestT);
    }

    if(hitT != NULL)
        *hitT = bestT;
    return best;
}

LameMob* FindMobAlongRay(CraftWorld* world, const Vector3& origin, const Vector3& dir, float reach, float* hitT, float* knockPower)
{
    LameMob* best = NULL;
    float bestT = reach + 1.0f;
    float bestPower = 0.7f;

    if(world != NULL)
    {
        for(unsigned int f = 0; f < world->mZombies.size(); f++)
            ConsiderPunchMob(world->mZombies[f], 0.75f, origin, dir, reach, &best, &bestT, &bestPower);
        for(unsigned int f = 0; f < world->mCows.size(); f++)
            ConsiderPunchMob(world->mCows[f], 0.7f, origin, dir, reach, &best, &bestT, &bestPower);
        for(unsigned int f = 0; f < world->mSheeps.size(); f++)
            ConsiderPunchMob(world->mSheeps[f], 0.7f, origin, dir, reach, &best, &bestT, &bestPower);
        for(unsigned int f = 0; f < world->mCreepers.size(); f++)
            ConsiderPunchMob(world->mCreepers[f], 0.7f, origin, dir, reach, &best, &bestT, &bestPower);
        for(unsigned int f = 0; f < world->mPigs.size(); f++)
            ConsiderPunchMob(world->mPigs[f], 0.7f, origin, dir, reach, &best, &bestT, &bestPower);
        for(unsigned int f = 0; f < world->mChickens.size(); f++)
            ConsiderPunchMob(world->mChickens[f], 0.6f, origin, dir, reach, &best, &bestT, &bestPower);
        // A spider stands lower than the animals but is a hostile, so it takes the zombie's punch.
        for(unsigned int f = 0; f < world->mSpiders.size(); f++)
            ConsiderPunchMob(world->mSpiders[f], 0.75f, origin, dir, reach, &best, &bestT, &bestPower);
        // Same for the skeleton: a biped on the zombie's collider, so the zombie's punch radius.
        for(unsigned int f = 0; f < world->mSkeletons.size(); f++)
            ConsiderPunchMob(world->mSkeletons[f], 0.75f, origin, dir, reach, &best, &bestT, &bestPower);
    }

    if(hitT != NULL)
        *hitT = bestT;
    if(knockPower != NULL)
        *knockPower = bestPower;
    return best;
}

static void ConsiderUseMob(LameMob* mob, const Vector3& origin, const Vector3& dir, float reach, LameMob** best, float* bestT)
{
    if(mob == NULL || mob->killed == true || mob->toDestroy == true)
        return;
    float tHit = 0.0f;
    if(RayIntersectAABB(origin, dir, MobAttackBox(mob->bBox), reach, &tHit) && tHit < *bestT)
    {
        *bestT = tHit;
        *best = mob;
    }
}

LameMob* FindMobForUse(CraftWorld* world, const Vector3& origin, const Vector3& dir, float reach)
{
    if(world == NULL)
        return NULL;
    LameMob* best = NULL;
    float bestT = reach + 1.0f;
    for(unsigned int f = 0; f < world->mZombies.size(); f++)
        ConsiderUseMob(world->mZombies[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mCows.size(); f++)
        ConsiderUseMob(world->mCows[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mSheeps.size(); f++)
        ConsiderUseMob(world->mSheeps[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mCreepers.size(); f++)
        ConsiderUseMob(world->mCreepers[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mPigs.size(); f++)
        ConsiderUseMob(world->mPigs[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mChickens.size(); f++)
        ConsiderUseMob(world->mChickens[f], origin, dir, reach, &best, &bestT);
    for(unsigned int f = 0; f < world->mSpiders.size(); f++)
        ConsiderUseMob(world->mSpiders[f], origin, dir, reach, &best, &bestT);
    // Nothing feeds or breeds a skeleton -- LameMob::Feed rejects it because breedItemId is -1 -- but
    // the use ray should still see every mob that exists, the same reason the spider is listed. A ray
    // that silently skips one list is a trap for the next item that has to touch what is aimed at.
    for(unsigned int f = 0; f < world->mSkeletons.size(); f++)
        ConsiderUseMob(world->mSkeletons[f], origin, dir, reach, &best, &bestT);
    if(best == NULL)
        return NULL;

    for(float i = 0; i < bestT; i += 0.15f)
    {
        Vector3 p = origin + (dir * i);
        BoundingBox c = world->GetBlockCollideBox(p.x,p.y,p.z, world->GetBlock(p.x,p.y,p.z));
        BoundingBox box = BoundingBox(floorf(p.x)+0.5f+c.min.x, floorf(p.y)+0.5f+c.min.y, floorf(p.z)+0.5f+c.min.z,
                                      floorf(p.x)+0.5f+c.max.x, floorf(p.y)+0.5f+c.max.y, floorf(p.z)+0.5f+c.max.z);
        if(world->BlockEditable(p.x,p.y,p.z) && box.contains(p))
            return NULL;
    }
    return best;
}

void LameMob::SetAge(float newAge)
{
    const bool wasBaby = IsBaby();
    age = newAge;
    if(wasBaby != IsBaby())
    {
        const float k = IsBaby() ? 0.5f : 2.0f;
        legs = Vector3(legs.x * k, legs.y * k, legs.z * k);
        SetCollideBox(baseColliderD, baseColliderU);
        UpdateBBox();
    }
}

void LameMob::SpawnHearts(int count)
{
    if(world == NULL)
        return;
    const float w = profile.fireWidth * ModelScale();
    const float h = profile.fireHeight * ModelScale();
    const float feetY = position.y - colliderD.y;
    for(int i = 0; i < count; i++)
    {
        Vector3 p(position.x - w * 0.5f + (rand() % 100) * 0.01f * w,
                  feetY + h * 0.6f + (rand() % 100) * 0.004f * h,
                  position.z - w * 0.5f + (rand() % 100) * 0.01f * w);
        Aurora::Graphics::Particle::Spawn(world, "heart", p);
    }
}

bool LameMob::Feed(int itemId)
{
    if(killed == true || breedItemId < 0 || itemId != breedItemId)
        return false;

    if(IsBaby())
    {
        // Java ageUp: feeding a baby skips 10% of the time it still needs.
        SetAge(age - age * 0.1f);
        SpawnHearts(1);
        return true;
    }

    if(age > 0.0f || InLove())
        return false;
    // No point falling in love if the baby could never be spawned.
    if(world != NULL && world->CanSpawnPassive() == false)
        return false;

    loveTimer = 30.0f;
    breedTimer = 0.0f;
    heartTimer = 0.4f;
    SpawnHearts(7);
    return true;
}

bool LameMob::UpdatePassiveAI(float dt, int heldItemId, LameMob* mate, LameMob* parent, float* headAngleY)
{
    bool babyDue = false;

    if(IsBaby())
    {
        float a = age + dt;
        SetAge(a >= 0.0f ? 0.0f : a);
    }
    else if(age > 0.0f)
    {
        age -= dt;
        if(age < 0.0f)
            age = 0.0f;
    }

    if(loveTimer > 0.0f)
    {
        loveTimer -= dt;
        heartTimer -= dt;
        if(heartTimer <= 0.0f)
        {
            SpawnHearts(1);
            heartTimer = 0.6f + (rand() % 5) * 0.1f;
        }
        if(loveTimer <= 0.0f)
        {
            loveTimer = 0.0f;
            breedTimer = 0.0f;
        }
    }

    if(poseLock)
    {
        moving = false;
        if(headAngleY != NULL)
            *headAngleY = 0.0f;
        return false;
    }

    if(UpdatePanic(dt))
    {
        breedTimer = 0.0f;
        if(headAngleY != NULL)
            *headAngleY = 0.0f;
        return false;
    }

    if(killed == false && kicked == false && InLove() && mate != NULL)
    {
        moving = false;
        hauntingTimer = 0.0f;
        const float dx = mate->position.x - position.x;
        const float dz = mate->position.z - position.z;
        const float dist = sqrtf(dx*dx + dz*dz);
        const float mateAngle = ATAN2_fast(-dx, -dz) + PI;
        mainAngle = TurnToward(mainAngle, mateAngle, 0.1f);
        if(headAngleY != NULL)
            *headAngleY = 0.0f;

        // Java EntityAIMate: walk over, then 3 s side by side (distance < 3).
        if(dist > 1.2f)
            TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * dt);
        if(dist < 3.0f)
        {
            breedTimer += dt;
            if(breedTimer >= 3.0f)
            {
                babyDue = true;
                loveTimer = 0.0f;
                breedTimer = 0.0f;
                age = 300.0f;
                mate->loveTimer = 0.0f;
                mate->breedTimer = 0.0f;
                mate->age = 300.0f;
            }
        }
        return babyDue;
    }
    breedTimer = 0.0f;

    if(UpdateTemptFollow(dt, heldItemId, breedItemId, headAngleY))
        return false;

    // Java EntityAIFollowParent: stay within 3 blocks of the nearest adult.
    if(killed == false && kicked == false && IsBaby() && parent != NULL)
    {
        const float dx = parent->position.x - position.x;
        const float dz = parent->position.z - position.z;
        const float dist = sqrtf(dx*dx + dz*dz);
        if(dist > 3.0f)
        {
            moving = false;
            hauntingTimer = 0.0f;
            const float parentAngle = ATAN2_fast(-dx, -dz) + PI;
            mainAngle = TurnToward(mainAngle, parentAngle, 0.1f);
            moveSpeedMul = 1.1f;
            TryToMove(Vector3(sinf(mainAngle), 0.0f, cosf(mainAngle)), SPD * 1.1f * dt);
            return false;
        }
    }

    UpdateWander(dt);
    return false;
}

void ApplyMeleeHit(CraftWorld* world, SoundManager* sounds, int barSlot, LameMob* mob, float power, float dt, bool wearWeapon)
{
    if(world == NULL || mob == NULL)
        return;

    float damage = 1.0f;
    bool weapon = false;
    damage = world->FetchDamagePoints(world->invId[27+barSlot], weapon);

    if(wearWeapon == true && weapon == true)
    {
        world->invAm[27+barSlot] -= 1+rand()%2;

        if(world->invAm[27+barSlot] <= 0)
        {
            world->invId[27+barSlot] = -1;
            world->invAm[27+barSlot] = -1;
            world->invSt[27+barSlot] = false;
            if(sounds != NULL)
                sounds->PlayBreakSound();
        }
    }

    mob->TakeDamage(damage, power, dt);
}

void WearBowOnShot(CraftWorld* world, SoundManager* sounds, int barSlot)
{
    if(world == NULL)
        return;

    // Creative does not consume the thing it is holding. ApplyMeleeHit expresses this as its
    // wearWeapon argument; folding the same rule in here keeps it out of both call sites, so a
    // future third FireBowArrow cannot forget it.
    if(world->gameModeWorld == 1)
        return;

    const int slot = 27 + barSlot;
    if(IsBowItem(world->invId[slot]) == false)
        return;

    // The bow's remaining uses live in invAm[slot] like every other tool's. DurabilityPointsItem is
    // the maximum (384 for a bow), not the current value -- see the durability note in changes.md,
    // which is also where the reason this was missing at all is written down.
    world->invAm[slot] -= 1;

    if(world->invAm[slot] <= 0)
    {
        // The same ending the mining path gives a worn-out pickaxe: the bow is destroyed where it
        // is and the break sound plays. No particle burst -- the mining path spawns one because it
        // already has the camera in hand for the block-face particles, and the melee path, which is
        // the closer precedent for a held weapon wearing out, has none either.
        world->invId[slot] = -1;
        world->invAm[slot] = -1;
        world->invSt[slot] = false;
        if(sounds != NULL)
            sounds->PlayBreakSound();
    }
}

void LameMob::SetOnFire(float seconds)
{
    if(killed == true)
        return;
    if(seconds > fireTime)
        fireTime = seconds;
}

void LameMob::UpdateFire(float dt)
{
    if(killed == true)
    {
        fireTime = 0.0f;
        fireDamageTimer = 0.0f;
        burning = false;
        return;
    }

    // Contact (Java): lava 15 s, fire block 8 s.
    if(footInLava || headInLava)
    {
        SetOnFire(15.0f);
    }
    else if(world != NULL)
    {
        Vector3 feet = position + foot;
        int fireId = Fire::getID();
        if(world->_GetBlock((int)floorf(feet.x), (int)floorf(feet.y + 0.05f), (int)floorf(feet.z)) == fireId ||
           world->_GetBlock((int)floorf(position.x), (int)floorf(position.y), (int)floorf(position.z)) == fireId)
        {
            SetOnFire(8.0f);
        }
    }

    // Water puts it out.
    if(footInWater || headInWater)
        fireTime = 0.0f;

    if(fireTime > 0.0f)
    {
        fireTime -= dt;
        fireDamageTimer += dt;
        if(fireDamageTimer >= 1.0f)
        {
            fireDamageTimer -= 1.0f;
            HP -= 1.0f;
            damaged = true;
            damagedTimer = 0.55f;
            playHurtSound = true;
            lastDamageCause = CauseFire;
        }
    }
    else
    {
        fireTime = 0.0f;
        fireDamageTimer = 0.0f;
    }
    burning = fireTime > 0.0f;
}

void LameMob::RenderFire(float javaWidth, float javaHeight)
{
    // Java 1.7.3 RenderLiving.renderEntityOnFire. A camera-facing stack of quads from
    // the feet up, each 1.4 tall, stepping up 0.45, shrinking 10%, alternating
    // fire_layer_0 / fire_layer_1 and mirroring every other pair. Drawn unlit and
    // 0.3 behind the body centre so the model occludes it.
    if(burning == false || killed == true || world == NULL)
        return;
    if(world->textureFireAnimationId < 0 || world->textureFireAnimation2Id < 0)
        return;

    TextureManager* tm = TextureManager::Instance();
    const int tex0 = world->textureFireAnimationId;
    const int tex1 = world->textureFireAnimation2Id;
    int frames0 = (int)(tm->getHeight(tex0) / tm->getWidth(tex0));
    int frames1 = (int)(tm->getHeight(tex1) / tm->getWidth(tex1));
    if(frames0 < 1) frames0 = 1;
    if(frames1 < 1) frames1 = 1;
    const int frame0 = ((int)world->animationFireFrame) % frames0;
    // 1.7.3's fire_layer_0.mcmeta starts at frame 16: the layers run half a cycle apart.
    const int frame1 = (frame0 + frames1 / 2) % frames1;

    const float s = javaWidth * 1.4f;

    sceGumPushMatrix();
    ScePspFVector3 fireLoc = {position.x, position.y - colliderD.y, position.z}; // feet
    sceGumTranslate(&fireLoc);
    sceGumRotateY(atan2f(relPlayerPos.x - position.x, relPlayerPos.z - position.z)); // local +Z faces the player
    ScePspFVector3 fireScale = {s, s, s};
    sceGumScale(&fireScale);

    // GU_CULL_FACE is on globally (DrawSetCulling only flips the winding), and a
    // billboard has one winding, so culling must be off here.
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuDepthMask(GU_TRUE);
    sceGuColor(0xFFFFFFFF);

    const float layers = javaHeight / s;
    const float zStart = -0.3f + (float)((int)layers) * 0.02f;

    for(int pass = 0; pass < 2; pass++)
    {
        const int frames = (pass == 0) ? frames0 : frames1;
        const int frame = (pass == 0) ? frame0 : frame1;
        const float v0 = (float)frame / (float)frames;
        const float v1 = (float)(frame + 1) / (float)frames;

        TexturesPSPVertex* fv = (TexturesPSPVertex*)sceGuGetMemory(4 * 6 * sizeof(TexturesPSPVertex));
        int fi = 0;
        float hw = 0.5f, y = 0.0f, z = zStart, rem = layers;
        for(int i = 0; rem > 0.0f && fi <= 18; i++)
        {
            if((i & 1) == pass)
            {
                float u0 = 0.0f, u1 = 1.0f;
                if(((i / 2) & 1) == 0) { u0 = 1.0f; u1 = 0.0f; }
                const float yT = y + 1.4f;
                fv[fi].u = u1; fv[fi].v = v1; fv[fi].x =  hw; fv[fi].y = y;  fv[fi].z = z; fi++;
                fv[fi].u = u0; fv[fi].v = v1; fv[fi].x = -hw; fv[fi].y = y;  fv[fi].z = z; fi++;
                fv[fi].u = u0; fv[fi].v = v0; fv[fi].x = -hw; fv[fi].y = yT; fv[fi].z = z; fi++;
                fv[fi].u = u1; fv[fi].v = v1; fv[fi].x =  hw; fv[fi].y = y;  fv[fi].z = z; fi++;
                fv[fi].u = u0; fv[fi].v = v0; fv[fi].x = -hw; fv[fi].y = yT; fv[fi].z = z; fi++;
                fv[fi].u = u1; fv[fi].v = v0; fv[fi].x =  hw; fv[fi].y = yT; fv[fi].z = z; fi++;
            }
            rem -= 0.45f;
            y += 0.45f;
            hw *= 0.9f;
            z += 0.03f;
        }
        if(fi > 0)
        {
            tm->SetTextureModeulate(pass == 0 ? tex0 : tex1);
            sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, fi, 0, fv);
        }
    }

    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_CULL_FACE);
    sceGuColor(0xFFFFFFFF);
    sceGumPopMatrix();
}
