#ifndef LAMEMOB_H_
#define LAMEMOB_H_

#include <math.h>
#include <psptypes.h>
#include <string>
#include <vector>

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>
#include <Aurora/Utils/Timer.h>
#include <Aurora/System/SystemManager.h>
#include "TextureHelper.h"
#include "MobProfile.h"

class CraftWorld;

class LameMob
{
public:
	LameMob(Vector3 mobPosition, int mobNumber);
    virtual ~LameMob();

    float DistanceToPlayer();
    unsigned int FastDistanceToPlayer();
    float PreciseDistanceToPlayer();
    float DistanceToHauntingPos();
    void TryToJump();
    void TryToMove(Vector3 movePosition1, float speed);
    void TryToMoveWithoutJump(Vector3 movePosition1, float speed);
    void SetPosition(Vector3 newPosition);
    void SetCollideBox(Vector3 collider1, Vector3 collider2);
    void UpdateBBox();
    // The box a projectile should test: the living size (profile.fireWidth/Height,
    // same numbers the fire overlay uses) unioned with the physics collider, then
    // grown a little. A new mob that calls SetCollideBox + ApplyProfile gets this
    // for free -- no per-type arrow table.
    BoundingBox HitBox() const;

    virtual void BoxCollision();
    virtual void TakeDamage(float damage, float power, float dt);
    virtual void TakeDamageFromPointExplosion(float damage, float power, float dt, Vector3 point);
    virtual void Update(CraftWorld *myWorld, Vector3 playerPos, float dt);
    virtual void Render(Frustum &camFrustum, float dt);

    // --- Shared mob layer (see MobProfile.h) ---
    // Apply once in the subclass ctor. New mobs: fill a MobProfile, call this,
    // then implement Render + any unique AI (fuse, shear, etc.).
    void ApplyProfile(const MobProfile& p);

    // Fire (Java semantics): SetOnFire keeps the longer of current/new duration.
    // UpdateFire handles lava/fire-block contact, water extinguish and 1 HP/s.
    // RenderFire is 1.7.3 RenderLiving.renderEntityOnFire with the Java entity size.
    void SetOnFire(float seconds);
    void UpdateFire(float dt);
    void RenderFire(float javaWidth, float javaHeight);
    void RenderFire(); // uses profile.fireWidth/Height

    // Shared damage / knockback kick (armor already applied by caller if any).
    void ApplyKickDamage(float damage, float power, float dt, int kickType, float* animLegAngleT);

    // Shared Update pieces. Call order for a typical mob:
    //   BeginMobUpdate → (early outs) → UpdateMobTimers → UpdateWander or UpdateHostileAI
    //   → UpdateKnockbackMove → UpdateLegAnim → UpdateMobPhysics → EndMobUpdate freeze
    // Returns false if the column is unloaded (caller should return immediately).
    bool BeginMobUpdate(CraftWorld* myWorld, Vector3 playerPos, float dt);
    void UpdateDeathPose(float dt);
    // Returns true if just-killed this frame (caller may resize collide box and return).
    bool CheckJustKilled();
    void UpdateMobTimers(float dt, const BoundingBox& playerHitBox);
    void UpdateWander(float dt);
    // Tempted by a held item (wheat for cow/pig/sheep). Returns true if following
    // (caller should skip UpdateWander). Optionally writes head yaw into headAngleY.
    bool UpdateTemptFollow(float dt, int heldItemId, int temptItemId, float* headAngleY);
    // Passive flee after a hit / while burning (profile.panicSpeedMul > 0).
    // Returns true while panicking (caller should skip tempt + wander).
    bool UpdatePanic(float dt);
    // Hostile: aggro/chase when profile.hostile && survival. callAttack when in melee range.
    void UpdateHostileAI(float dt, bool* callAttack);
    // Hostile ranged (the skeleton's bow). Chases until within profile.rangedRange, then holds still
    // and looses an arrow. Writes true into *callShoot on the frame a shot is due and reloads
    // rangedShotTimer from the profile; the caller owns the Arrow2, because only the world knows
    // where the mobs and the player are. No-op unless profile.rangedRange > 0.
    void UpdateHostileRangedAI(float dt, bool* callShoot);
    // Turns the body onto the player at the shared ranged turn rate. Shared by the bow AI and by a
    // draw that a debug stick started -- in creative a mob never aggros, so the AI's own facing never
    // runs, and a shot fired from the debug menu would leave the bow while the skeleton still faced
    // whichever way it happened to be wandering. One implementation so the two cannot disagree.
    void AimAtPlayer(float dt);
    void UpdateKnockbackMove(float dt);
    void UpdateLegAnim(float dt, float* animLegAngleT, float* animLegAngle);
    void UpdateMobPhysics(float dt);
    // Returns true if this mob should be destroyed (out of range too long).
    bool UpdateOutOfRangeFreeze(float dt);

    // --- Debug stick -----------------------------------------------------------
    // What a debug stick offers when it is used on this mob, and what each choice does. The default
    // set is generic and works on anything alive; a mob overrides both to add its own actions (the
    // skeleton adds its bow) by appending after the base call and delegating any index it did not
    // claim back down.
    //
    // Filling `options` and handling `index` have to agree, so they live side by side and the generic
    // choices share one enum rather than a magic offset that could drift.
    enum DebugGeneric
    {
        Debug_SetFire = 0,
        Debug_Extinguish,
        Debug_Damage5,
        Debug_Kill,
        Debug_Heal,
        Debug_GenericCount
    };
    // Appends this mob's options to `options` and returns the running total, so the caller gets the
    // count for free and a subclass cannot forget to account for the base's entries.
    virtual int DebugOptions(std::vector<std::string>& options);
    // Runs one choice. Indices below Debug_GenericCount are the generic set.
    virtual void DebugAction(int index);

    // The debug menu's title for this mob. Set in each subclass ctor beside ApplyProfile, because it
    // is player-facing text and there is nothing already on the mob to read it from.
    const char* debugName;

    // --- Breeding / age (Java EntityAnimal + EntityAgeable) ---
    // age < 0: baby, counts up to 0 then grows up. age > 0: breeding cooldown.
    bool IsBaby() const { return age < 0.0f; }
    bool InLove() const { return loveTimer > 0.0f; }
    float ModelScale() const { return age < 0.0f ? 0.5f : 1.0f; }
    // Switches collider/legs between baby and adult size when crossing 0.
    void SetAge(float newAge);
    // Right-click with food. Returns true if the item should be consumed.
    bool Feed(int itemId);
    void SpawnHearts(int count);
    // Passive animal brain: panic > mate > tempt > follow parent > wander.
    // Returns true on the frame this mob finishes mating; caller spawns the baby
    // (species-specific) next to it. Both parents are already reset.
    bool UpdatePassiveAI(float dt, int heldItemId, LameMob* mate, LameMob* parent, float* headAngleY);

    // Test / inspect: no walking, no panic run, no park, no knockback slide. Physics still
    // sits it on the ground. Set after a test spawn so the model can be looked at.
    bool poseLock;

    // Nearest same-species mob for breeding (both in love) or for a baby to follow
    // (nearest adult). T is the concrete class stored in CraftWorld's vector.
    template<class T>
    T* FindKin(std::vector<T*>& list, bool wantMate, float range)
    {
        T* best = NULL;
        float bestD = range * range;
        for(unsigned int i = 0; i < list.size(); i++)
        {
            T* o = list[i];
            if(o == NULL || (LameMob*)o == this || o->killed || o->toDestroy)
                continue;
            if(wantMate ? (!o->InLove() || o->IsBaby()) : o->IsBaby())
                continue;
            float dx = o->position.x - position.x;
            float dz = o->position.z - position.z;
            float d = dx*dx + dz*dz;
            if(d < bestD && fabsf(o->position.y - position.y) < 3.0f)
            {
                bestD = d;
                best = o;
            }
        }
        return best;
    }

    float age;
    // Seconds this mob has actually existed, counted every frame it is updated. `age` above is the
    // breeding timer and is 0 for every adult, so it could never answer "how long since it spawned" --
    // which is the one question the death log has to answer to tell a real spawn death from an
    // ordinary fall. This is that number.
    float lifeTime;
    float loveTimer;       // > 0 in love (Java 600 ticks)
    float breedTimer;      // time spent next to the mate (Java 60 ticks)
    float heartTimer;
    int breedItemId;       // food that tempts/breeds this species (-1 = none)
    Vector3 baseColliderD; // adult collider; SetCollideBox scales by ModelScale()
    Vector3 baseColliderU;

    MobProfile profile;

    float fallDistance;    // blocks fallen since last on ground (Java fall damage)
    float lastFallDistance; // the value above at the moment fall damage was applied, for the death log
    float spawnGrace;      // seconds after spawn with no fall damage
    // False until this mob has touched the ground for the first time. The drop that brings a newly
    // spawned mob down onto the terrain is not a fall it should be able to die of, and tying the fall
    // damage to "has landed at least once" makes that true whatever the grace timer does.
    bool spawnLanded;
    int lastDamageCause;   // DamageCause, logged when the mob dies
    enum DamageCause { CauseNone = 0, CauseHit, CauseExplosion, CauseFall, CauseLava, CauseFire, CauseCactus };

    float panicTimer;      // > 0 while fleeing
    float panicRepick;     // seconds until a new flee heading
    float panicAngle;
    float moveSpeedMul;    // this frame's speed vs walk (1 = walk); scales leg swing

    bool burning;
    float fireTime;
    float fireDamageTimer;

    float mainAngle;

    Vector3 position;
    Vector3 relPlayerPos;
    BoundingBox bBox;

    bool walkingOnGround;
    // Set by a mob that climbs walls instead of jumping (the spider). While it is true
    // UpdateMobPhysics leaves the height alone: a climbing mob's feet are still buried in the
    // block it pushed off from, and the ground snap would otherwise undo every climb step.
    bool climbing;
    bool moving;
    bool angred;
    // Set by UpdateHostileRangedAI while a bow mob is in range and holding its ground. Separate from
    // `angred` because 1.7.3's EntityAIArrowAttack clears that when it stops to shoot: in range the mob
    // is neither angry nor moving, but it is very much aiming. The skeleton reads this to keep its head
    // on the player while it draws.
    bool rangedEngaged;
    bool toDestroy;
    bool killed;
    bool kicked;
    bool nearToPlayer;
    bool damaged;

    int lastKickType;

    float HP;
    float SPD;

    float damagedTimer;   // red hurt flash; reset by every damage source
    float knockbackTimer; // knockback strength/duration; only set by TakeDamage*
    float kickTimer;
    float deathTimer;
    float sayTimer;

    // Seconds until the next ranged shot, reloaded by UpdateHostileRangedAI from the profile.
    // Unused (left at 0) by every mob that has no bow.
    float rangedShotTimer;

    float deathAngle;

    int number;

    bool playHurtSound;
    bool playDieSound;
    bool playSaySound;
    // Set by a mob that has footfall clips of its own (the spider) on a distance walked cadence;
    // the play-state loops read and clear it like the three above.
    bool playStepSound;

    CraftWorld *world;

    float fastDistanceToPlayer;
    float distanceToPlayer;

    Vector3 hauntingPos;

    Vector3 velocity;
    Vector3 colliderD;
    Vector3 colliderU;

    Vector3 legs, foot, body, head;

    Vector3 explosionPos;

    bool jumping;
    bool freezed;
    bool footInWater;
    bool footInLava;
    bool headInWater;
    bool headInLava;
    bool jumpReady;
    bool haunting;

    float jumpTimer;
    float freezedTimer;
    float freezedTimerMax;
    float hauntingTimer;
    float hauntingTimerMax;
    float checkCollisionTimer;
    float updateLightTimer;

    float tickCactusDamage;
    bool touchingCactus;

    float hauntingAngle;

    float myLight;

    int mainTexture;

    bool friendly;

private:

};

class SoundManager;
LameMob* FindMobAlongRay(CraftWorld* world, const Vector3& origin, const Vector3& dir, float reach, float* hitT, float* knockPower);
// Nearest living mob whose HitBox() the segment a→b crosses. Used by arrows so a
// shot cannot pass through a body between two physics steps.
LameMob* FindMobAlongSegment(CraftWorld* world, const Vector3& a, const Vector3& b, float* hitT);
// Nearest mob along the look ray that is not behind a block (right-click use).
LameMob* FindMobForUse(CraftWorld* world, const Vector3& origin, const Vector3& dir, float reach);
void ApplyMeleeHit(CraftWorld* world, SoundManager* sounds, int barSlot, LameMob* mob, float power, float dt, bool wearWeapon);

// One shot fired wears the bow down. Shared by both play states for the same reason ApplyMeleeHit
// is: each state has its own FireBowArrow, and a wear step written out twice is a wear step that
// drifts. The mode policy lives inside, not at the call sites -- a shot taken in a creative mode
// world does not spend the bow, exactly as a creative swing does not spend the sword.
void WearBowOnShot(CraftWorld* world, SoundManager* sounds, int barSlot);

#endif

