#ifndef MOBPROFILE_H_
#define MOBPROFILE_H_

// Tunable constants for a mob type. Construct once, ApplyProfile() in the
// subclass ctor. Shared Update* helpers on LameMob read these so a new mob
// does not copy-paste wander / chase / physics / knockback / anim.

struct MobProfile
{
    float hp;
    float spd;

    // 1.7.3 RenderLiving fire size (width, height of the living entity).
    float fireWidth;
    float fireHeight;

    // Hostile AI. Passive aggroRange <= 0 the mob never chases (passive animal).
    float aggroRange;      // fastDistanceToPlayer to start chase (zombie ~11)
    float stopChaseRange;  // distanceToPlayer: stop walking when this close (~1.3)
    float attackRange;     // distanceToPlayer to melee (~2.42)
    float attackDamage;
    float attackCooldown;    // seconds between melee kicks (~1.0)

    float playerBumpRange; // set nearToPlayer / soft push (~1.5 animals, ~2.4 zombie)
    float knockbackEndTimer; // clear kicked when grounded and knockbackTimer <= this
                             // (animals 0.17, hostiles 0.0)

    bool sunBurns;         // SetOnFire(8) while in sunlight (zombie)
    bool checkCactus;      // cactus collision + damage (zombie/creeper)
    bool hostile;          // may aggro when gameModeWorld != creative

    float wanderRadius;    // half-extent of random wander target (~6)
    float freezedDestroy;  // out-of-range freeze timer before toDestroy

    // Leg animation
    float legWalkRate;     // rad/s multiplier on PI*dt (walk ~1.6)
    float legWalkAmp;
    float legKickRate;     // while kicked (~6)
    float legKickAmp;

    // Running (Java 1.7 EntityAIPanic / EntityAITempt speed multipliers).
    // Hostile profiles leave these 0: no panic, no lure.
    float panicSpeedMul;   // flee speed after a hit or while burning (0 = never panics)
    float panicTime;       // seconds of panic after a hit (Java revenge timer 5 s)
    float temptSpeedMul;   // lure speed when farther than temptRunRange
    float temptRunRange;   // run toward the lure beyond this distance, walk inside it
    float temptStopRange;  // stop in front of the player at this distance

    // Ranged attack (the skeleton's bow). rangedRange <= 0 means the mob has no ranged attack, and
    // LameMob::UpdateHostileRangedAI is never called for it. These are appended at the end of the
    // struct on purpose: every profile above uses positional aggregate initialisation, so a field
    // added mid-struct would silently shift the values of every existing mob.
    float rangedRange;       // approach until this close, then hold and shoot (~15 blocks)
    float rangedCooldownMin; // seconds between shots, lower bound (Java 20 ticks = 1.0 s)
    float rangedCooldownMax; // upper bound (Java 60 ticks = 3.0 s)
};

namespace MobProfiles
{
    // Values match the pre-overhaul hardcoded behaviour.
    static const MobProfile Cow = {
        /*hp*/10, /*spd*/1.0f,
        /*fire*/0.9f, 1.3f,
        /*aggro*/0, 0, 0, 0, 0,
        /*bump*/1.5f, /*kbEnd*/0.17f,
        /*sun*/false, /*cactus*/false, /*hostile*/false,
        /*wander*/6.0f, /*freeze*/500.0f,
        /*leg*/1.6f, 0.8f, 6.0f, 1.1f,
        /*panic*/2.0f, 5.0f, /*tempt*/1.25f, 4.0f, 2.3f
    };

    static const MobProfile Pig = {
        /*hp*/10, /*spd*/1.0f,
        /*fire*/0.9f, 0.9f,
        /*aggro*/0, 0, 0, 0, 0,
        /*bump*/1.5f, /*kbEnd*/0.17f,
        /*sun*/false, /*cactus*/false, /*hostile*/false,
        /*wander*/6.0f, /*freeze*/500.0f,
        /*leg*/1.6f, 0.8f, 6.0f, 1.1f,
        /*panic*/1.25f, 5.0f, /*tempt*/1.2f, 4.0f, 2.3f
    };

    static const MobProfile Sheep = {
        /*hp*/8, /*spd*/1.0f,
        /*fire*/0.9f, 1.3f,
        /*aggro*/0, 0, 0, 0, 0,
        /*bump*/1.5f, /*kbEnd*/0.17f,
        /*sun*/false, /*cactus*/false, /*hostile*/false,
        /*wander*/6.0f, /*freeze*/500.0f,
        /*leg*/1.5f, 0.655f, 6.0f, 0.95f,
        /*panic*/1.25f, 5.0f, /*tempt*/1.1f, 4.0f, 2.3f
    };

    static const MobProfile Chicken = {
        /*hp*/4, /*spd*/1.0f,
        /*fire*/0.4f, 0.7f,
        /*aggro*/0, 0, 0, 0, 0,
        /*bump*/1.5f, /*kbEnd*/0.17f,
        /*sun*/false, /*cactus*/false, /*hostile*/false,
        /*wander*/6.0f, /*freeze*/500.0f,
        /*leg*/2.0f, 0.55f, 6.0f, 0.8f,
        /*panic*/2.0f, 5.0f, /*tempt*/1.2f, 4.0f, 1.8f
    };

    static const MobProfile Zombie = {
        /*hp*/18, /*spd*/1.1f,
        /*fire*/0.6f, 1.8f,
        /*aggro*/11.0f, /*stop*/1.3f, /*atk*/2.42f, /*dmg*/3.0f, /*cd*/1.0f,
        /*bump*/2.42f, /*kbEnd*/0.0f,
        /*sun*/true, /*cactus*/true, /*hostile*/true,
        /*wander*/6.0f, /*freeze*/60.0f,
        /*leg*/1.6f, 0.7f, 6.0f, 1.0f
    };

    // Vanilla EntitySpider: setSize(1.4, 0.9), maxHealth 16, movementSpeed 0.3,
    // attackDamage 2.0, and it does not burn in daylight. The engine's SPD is blocks per second
    // and the zombie's 0.23 attribute is 1.1 here, so the same scale puts the spider's 0.3 at 1.35.
    // The melee ranges are wider than the zombie's because the hitbox is 1.4 blocks across rather
    // than 0.6: the bodies touch 0.4 further out, so attackRange and the bump range move with it.
    static const MobProfile Spider = {
        /*hp*/16, /*spd*/1.35f,
        /*fire*/1.4f, 0.9f,
        /*aggro*/11.0f, /*stop*/1.3f, /*atk*/2.9f, /*dmg*/2.0f, /*cd*/1.0f,
        /*bump*/2.8f, /*kbEnd*/0.0f,
        /*sun*/false, /*cactus*/true, /*hostile*/true,
        /*wander*/6.0f, /*freeze*/60.0f,
        /*leg*/1.9f, 0.55f, 6.0f, 1.0f
    };

    static const MobProfile Creeper = {
        /*hp*/18, /*spd*/1.18f,
        /*fire*/0.6f, 1.8f,
        /*aggro*/11.0f, /*stop*/1.3f, /*atk*/0, /*dmg*/0, /*cd*/0, // fuse is creeper-specific
        /*bump*/2.18f, /*kbEnd*/0.0f,
        /*sun*/false, /*cactus*/true, /*hostile*/true,
        /*wander*/6.0f, /*freeze*/60.0f,
        /*leg*/1.8f, 0.7f, 6.0f, 1.0f
    };

    // Vanilla EntitySkeleton: maxHealth 20, movementSpeed 0.25 and a hurt of 2.0, but its weapon is
    // the bow rather than its fists, so attackRange/attackDamage/attackCooldown are 0 here and the
    // damage lives in rangedDamage below. It is 0.6 wide and 1.99 tall, and like the zombie it is a
    // sun-sensitive monster -- 1.7.3 sets skeleton entity fire on daylight just as it does for the
    // zombie. The engine's SPD is blocks per second and the zombie's 0.23 attribute is 1.1 here, so
    // the skeleton's 0.25 scales to 1.2.
    //
    // EntityAIArrowAttack(this, 1.0, 20, 60, 15.0F): the skeleton closes to 15 blocks, then stops and
    // looses every 20-60 ticks. Because the aggro range is 16 it is essentially always shooting by the
    // time it is close enough, which is why aggro sits just above the firing range.
    static const MobProfile Skeleton = {
        /*hp*/20, /*spd*/1.2f,
        /*fire*/0.6f, 1.99f,
        /*aggro*/16.0f, /*stop*/1.3f, /*atk*/0, /*dmg*/0, /*cd*/0,
        /*bump*/2.42f, /*kbEnd*/0.0f,
        /*sun*/true, /*cactus*/true, /*hostile*/true,
        /*wander*/6.0f, /*freeze*/60.0f,
        /*leg*/1.6f, 0.7f, 6.0f, 1.0f,
        /*panic*/0, 0, /*tempt*/0, 0, 0,
        /*ranged*/15.0f, 1.0f, 3.0f
    };

}

#endif
