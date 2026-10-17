#include "CraftWorld2.h"
#include "SoundManager.h"
#include <Aurora/Graphics/Effects/Arrow2.h>
#include <Aurora/Graphics/Effects/Particle.h>
#include <math.h>

void CraftWorld::SpawnZombie(float xx, float yy, float zz)
{
    zombieNum += 1;

    Zombie *NewZombie = new Zombie(Vector3(xx,yy,zz), zombieNum);
    NewZombie->world = this;
    mZombies.push_back(NewZombie);
}

void CraftWorld::DestroyZombie(int id)
{
    if(id < mZombies.size() && id >= 0)
    {
        delete mZombies[id];
        mZombies.erase(mZombies.begin()+id);
    }
}

void CraftWorld::DestroyAllZombies()
{
    for(int ii = 0; ii < mZombies.size(); ii++)
    {
        delete mZombies[0];
        mZombies.erase(mZombies.begin());
    }
    mZombies.clear();
}

void CraftWorld::SpawnCreeper(float xx, float yy, float zz)
{
    zombieNum += 1;

    Creeper *NewCreeper = new Creeper(Vector3(xx,yy,zz), zombieNum);
    NewCreeper->world = this;
    mCreepers.push_back(NewCreeper);
}

void CraftWorld::SpawnCreeper(float xx, float yy, float zz, float freezedTimerMax)
{
    zombieNum += 1;

    Creeper *NewCreeper = new Creeper(Vector3(xx,yy,zz), zombieNum);
    NewCreeper->world = this;
    NewCreeper->freezedTimerMax = freezedTimerMax;
    mCreepers.push_back(NewCreeper);
}


void CraftWorld::DestroyCreeper(int id)
{
    if(id < mCreepers.size() && id >= 0)
    {
        delete mCreepers[id];
        mCreepers.erase(mCreepers.begin()+id);
    }
}

void CraftWorld::DestroyAllCreepers()
{
    for(int ii = 0; ii < mCreepers.size(); ii++)
    {
        delete mCreepers[0];
        mCreepers.erase(mCreepers.begin());
    }
    mCreepers.clear();
}

void CraftWorld::SpawnPig(float xx, float yy, float zz)
{
    zombieNum += 1;

    Pig *NewPig = new Pig(Vector3(xx,yy,zz), zombieNum);
    NewPig->world = this;
    mPigs.push_back(NewPig);
}

void CraftWorld::SpawnPig(float xx, float yy, float zz, float freezedTimerMax)
{
    zombieNum += 1;

    Pig *NewPig = new Pig(Vector3(xx,yy,zz), zombieNum);
    NewPig->world = this;
    NewPig->freezedTimerMax = freezedTimerMax;
    mPigs.push_back(NewPig);
}

void CraftWorld::DestroyPig(int id)
{
    if(id < mPigs.size() && id >= 0)
    {
        delete mPigs[id];
        mPigs.erase(mPigs.begin()+id);
    }
}

void CraftWorld::DestroyAllPigs()
{
    for(int ii = 0; ii < (int)mPigs.size(); ii++)
    {
        delete mPigs[0];
        mPigs.erase(mPigs.begin());
    }
    mPigs.clear();
}

void CraftWorld::SpawnChicken(float xx, float yy, float zz)
{
    zombieNum += 1;

    Chicken *NewChicken = new Chicken(Vector3(xx,yy,zz), zombieNum);
    NewChicken->world = this;
    mChickens.push_back(NewChicken);
}

void CraftWorld::SpawnChicken(float xx, float yy, float zz, float freezedTimerMax)
{
    zombieNum += 1;

    Chicken *NewChicken = new Chicken(Vector3(xx,yy,zz), zombieNum);
    NewChicken->world = this;
    NewChicken->freezedTimerMax = freezedTimerMax;
    mChickens.push_back(NewChicken);
}

void CraftWorld::DestroyChicken(int id)
{
    if(id < (int)mChickens.size() && id >= 0)
    {
        delete mChickens[id];
        mChickens.erase(mChickens.begin()+id);
    }
}

void CraftWorld::DestroyAllChickens()
{
    for(int ii = 0; ii < (int)mChickens.size(); ii++)
    {
        delete mChickens[0];
        mChickens.erase(mChickens.begin());
    }
    mChickens.clear();
}

void CraftWorld::SpawnSpider(float xx, float yy, float zz)
{
    zombieNum += 1;

    Spider *NewSpider = new Spider(Vector3(xx,yy,zz), zombieNum);
    NewSpider->world = this;
    mSpiders.push_back(NewSpider);
}

void CraftWorld::SpawnSpider(float xx, float yy, float zz, float freezedTimerMax)
{
    zombieNum += 1;

    Spider *NewSpider = new Spider(Vector3(xx,yy,zz), zombieNum);
    NewSpider->world = this;
    NewSpider->freezedTimerMax = freezedTimerMax;
    mSpiders.push_back(NewSpider);
}

void CraftWorld::DestroySpider(int id)
{
    if(id < (int)mSpiders.size() && id >= 0)
    {
        delete mSpiders[id];
        mSpiders.erase(mSpiders.begin()+id);
    }
}

void CraftWorld::DestroyAllSpiders()
{
    for(int ii = 0; ii < (int)mSpiders.size(); ii++)
    {
        delete mSpiders[0];
        mSpiders.erase(mSpiders.begin());
    }
    mSpiders.clear();
}

void CraftWorld::SpawnSkeleton(float xx, float yy, float zz)
{
    zombieNum += 1;

    Skeleton *NewSkeleton = new Skeleton(Vector3(xx,yy,zz), zombieNum);
    NewSkeleton->world = this;
    mSkeletons.push_back(NewSkeleton);
}

void CraftWorld::SpawnSkeleton(float xx, float yy, float zz, float freezedTimerMax)
{
    zombieNum += 1;

    Skeleton *NewSkeleton = new Skeleton(Vector3(xx,yy,zz), zombieNum);
    NewSkeleton->world = this;
    NewSkeleton->freezedTimerMax = freezedTimerMax;
    mSkeletons.push_back(NewSkeleton);
}

void CraftWorld::DestroySkeleton(int id)
{
    if(id < (int)mSkeletons.size() && id >= 0)
    {
        delete mSkeletons[id];
        mSkeletons.erase(mSkeletons.begin()+id);
    }
}

void CraftWorld::DestroyAllSkeletons()
{
    for(int ii = 0; ii < (int)mSkeletons.size(); ii++)
    {
        delete mSkeletons[0];
        mSkeletons.erase(mSkeletons.begin());
    }
    mSkeletons.clear();
}

void CraftWorld::DestroyArrow(int id)
{
    if(id < (int)mArrows.size() && id >= 0)
    {
        delete mArrows[id];
        mArrows.erase(mArrows.begin()+id);
    }
}

void CraftWorld::UpdateArrows(float dt)
{
    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        Arrow2* UseArrow = mArrows[i];

        if(UseArrow->stuck == true)
        {
            // Landed: no motion, no hit test, and no chance of hitting the same wall again. Only the
            // collection delay still runs, and only so an arrow cannot be swept up on the frame it
            // comes to rest under the player's feet.
            UseArrow->TickPickupDelay(dt);

            if(UseArrow->Collectable() == true && HP > 0.0f && InventoryIsFull(Arrow::getID(),1,true) == false)
            {
                // The same box Drop uses to decide you walked over something: 0.8 across and 1.8
                // tall, so a landed arrow is collected by standing on it, not by passing near it.
                const float dx = UseArrow->position.x - playerPos.x;
                const float dy = UseArrow->position.y - playerPos.y;
                const float dz = UseArrow->position.z - playerPos.z;
                if(dx > -0.8f && dx < 0.8f && dy > -1.8f && dy < 1.8f && dz > -0.8f && dz < 0.8f)
                {
                    // A full inventory leaves the arrow where it is rather than swallowing it: the
                    // stack is worth more than the animation, and the player can come back for it.
                    PutInInventory(Arrow::getID(),1,true);
                    DestroyArrow(i);
                    i --;
                }
            }
            continue;
        }

        UseArrow->Update(this, dt);

        bool removeArrow = false;
        // Set by every branch that stopped the arrow in a body rather than in a block, so the impact
        // sound below can tell the two apart without repeating itself in five hit tests.
        bool hitBody = false;
        Vector3 arrowStop = UseArrow->position;

        if(UseArrow->Expired() == true)
        {
            // Flew for a minute without hitting anything. See Arrow2::Expired -- this is the case
            // where there is no block to land in, so nothing else would ever end it.
            removeArrow = true;
            arrowStop = UseArrow->position;
        }
        else
        {
            // Bodies before blocks. A shot that reaches a mob standing against a wall used to
            // stick in the wall and skip the hit test entirely, which is the "arrow went through
            // them" report: the mob is air as far as CheckCollision is concerned, and the wall
            // behind them is not. The step is swept, because a full-charge shot covers close to
            // a block per frame on 30 Hz.
            Vector3 seg(UseArrow->position.x - UseArrow->prevPosition.x,
                        UseArrow->position.y - UseArrow->prevPosition.y,
                        UseArrow->position.z - UseArrow->prevPosition.z);
            float segLen2 = seg.x*seg.x + seg.y*seg.y + seg.z*seg.z;

            if(UseArrow->fromMob == true)
            {
                // A mob shot can only hit the player. The chest sits ~0.9 above the feet.
                Vector3 p(playerPos.x, playerPos.y - 0.9f, playerPos.z);
                Vector3 toP(p.x - UseArrow->prevPosition.x, p.y - UseArrow->prevPosition.y, p.z - UseArrow->prevPosition.z);
                float t = (segLen2 > 0.0001f) ? ((toP.x*seg.x + toP.y*seg.y + toP.z*seg.z) / segLen2) : 0.0f;
                if(t < 0.0f) t = 0.0f;
                if(t > 1.0f) t = 1.0f;
                Vector3 closest(UseArrow->prevPosition.x + seg.x*t, UseArrow->prevPosition.y + seg.y*t, UseArrow->prevPosition.z + seg.z*t);
                float dx = closest.x - p.x, dy = closest.y - p.y, dz = closest.z - p.z;
                if(dx*dx + dy*dy + dz*dz < 0.36f) // 0.6 blocks
                {
                    removeArrow = true;
                    arrowStop = closest;
                    hitBody = true;
                    if(gameModeWorld != 1)
                    {
                        // Reuses the melee knockback channel (kickedBy 1 = hurt by a mob).
                        kickedStart = true;
                        kickedAngle = ATAN2_fast(closest.x - UseArrow->prevPosition.x, closest.z - UseArrow->prevPosition.z);
                        kickedBy = 1;
                        kickedDamage = (int)UseArrow->damage;
                    }
                }
            }
            else
            {
                // One walk, one box: HitBox() is the living size plus a little slack,
                // so a zombie and a spider do not need their own numbers here.
                float tHit = 0.0f;
                LameMob* hit = FindMobAlongSegment(this, UseArrow->prevPosition, UseArrow->position, &tHit);
                if(hit != 0)
                {
                    removeArrow = true;
                    hitBody = true;
                    if(segLen2 > 0.0001f)
                    {
                        float inv = 1.0f / sqrtf(segLen2);
                        arrowStop = Vector3(UseArrow->prevPosition.x + seg.x * inv * tHit,
                                            UseArrow->prevPosition.y + seg.y * inv * tHit,
                                            UseArrow->prevPosition.z + seg.z * inv * tHit);
                    }
                    else
                    {
                        arrowStop = UseArrow->position;
                    }
                    hit->TakeDamage(UseArrow->damage, 1.0f, dt);
                }
            }

            if(removeArrow == false && UseArrow->CheckCollision(this) == true)
            {
                // A block stops an arrow, it does not delete one. It lands against the face it hit
                // and stays in the world, so a shot can be walked over and picked back up -- see Stick().
                UseArrow->Stick(this);
                arrowStop = UseArrow->position;

                if(Particle::ActiveCount() <= 52)
                {
                    for(int k = 0; k <= 3; k++) // 4 particles
                    {
                        Particle* ArrowPart = Particle::Spawn(this,"item crack",Vector3(arrowStop.x,arrowStop.y+0.1f,arrowStop.z));
                        ArrowPart->var1 = Arrow::getID()-250;
                        ArrowPart->SetScale(0.06f, 0.12f);
                    }
                }
            }
        }

        if(removeArrow == true)
        {
            // One impact sound per arrow, chosen by what stopped it: a body is a different sample
            // from a wall, which is why the two are separate calls (SoundManager decides which of the
            // four block clips to use, so a wall of arrows does not tick like a metronome). A shot
            // that merely ran out of air -- Expired, above -- hit nothing and so stays silent, which
            // is also why this sits after the branch rather than inside it.
            if(UseArrow->Expired() == false)
            {
                const float sdx = arrowStop.x - playerPos.x;
                const float sdy = arrowStop.y - playerPos.y;
                const float sdz = arrowStop.z - playerPos.z;
                const float hitDistance = sqrtf(sdx*sdx + sdy*sdy + sdz*sdz);
                if(hitBody == true)
                    SoundManager::Instance()->PlayArrowFleshSound(hitDistance);
                else
                    SoundManager::Instance()->PlayArrowHitSound(hitDistance);
            }

            if(Particle::ActiveCount() <= 52)
            {
                for(int k = 0; k <= 3; k++) // 4 particles
                {
                    Particle* ArrowPart = Particle::Spawn(this,"item crack",Vector3(arrowStop.x,arrowStop.y+0.1f,arrowStop.z));
                    ArrowPart->var1 = Arrow::getID()-250;
                    ArrowPart->SetScale(0.06f, 0.12f);
                }
            }

            DestroyArrow(i);
            i --;
        }
    }

    // Evict the oldest landed shots past the cap. Done as a pass of its own rather than inside the
    // loop above, so that removing an entry cannot shift the index the loop is holding. The list is
    // in creation order, so the first landed arrow in it is the oldest one.
    int landed = 0;
    for(unsigned int i = 0; i < mArrows.size(); i++)
    {
        if(mArrows[i]->stuck == true)
            landed ++;
    }
    while(landed > kMaxLandedArrows)
    {
        for(unsigned int i = 0; i < mArrows.size(); i++)
        {
            if(mArrows[i]->stuck == true)
            {
                DestroyArrow(i);
                landed --;
                break;
            }
        }
    }
}

void CraftWorld::SpawnCow(float xx, float yy, float zz)
{
    zombieNum += 1;

    Cow *NewCow = new Cow(Vector3(xx,yy,zz), zombieNum);
    NewCow->world = this;
    mCows.push_back(NewCow);
}

void CraftWorld::DestroyCow(int id)
{
    if(id < mCows.size() && id >= 0)
    {
        delete mCows[id];
        mCows.erase(mCows.begin()+id);
    }
}

void CraftWorld::SpawnSheep(float xx, float yy, float zz)
{
    zombieNum += 1;

    Sheep *NewSheep = new Sheep(Vector3(xx,yy,zz), zombieNum);
    NewSheep->world = this;
    mSheeps.push_back(NewSheep);
}

// Every mob costs ~8-10 draw calls plus its AI each frame; the 64 MB models also
// run the bigger stream radius, so they get roughly double the population.
int CraftWorld::PassiveMobCap()     { return bigMemory ? 40 : 20; }
int CraftWorld::HostileMobCap()     { return bigMemory ? 16 : 8; }
int CraftWorld::NaturalPassiveCap() { return bigMemory ? 16 : 10; }
int CraftWorld::PassiveMobCount()   { return (int)(mCows.size() + mSheeps.size() + mPigs.size() + mChickens.size()); }
int CraftWorld::HostileMobCount()   { return (int)(mZombies.size() + mCreepers.size() + mSpiders.size() + mSkeletons.size()); }
bool CraftWorld::CanSpawnPassive()  { return PassiveMobCount() < PassiveMobCap(); }
bool CraftWorld::CanSpawnHostile()  { return HostileMobCount() < HostileMobCap(); }

bool CraftWorld::IsSpawnEgg(int itemId)
{
    // Listed rather than ranged: the eggs are not contiguous any more (the chicken egg is id 410,
    // after the redstone torch and repeater), and a range check would have to be kept in step with
    // every future item anyway.
    return itemId == CowSpawnEgg::getID()     || itemId == SheepSpawnEgg::getID() ||
           itemId == ZombieSpawnEgg::getID()  || itemId == CreeperSpawnEgg::getID() ||
           itemId == PigSpawnEgg::getID()     || itemId == ChickenSpawnEgg::getID() ||
           itemId == SpiderSpawnEgg::getID()  || itemId == SkeletonSpawnEgg::getID();
}

bool CraftWorld::SpawnFromEgg(int itemId, float x, float y, float z)
{
    if(itemId == CowSpawnEgg::getID())
    {
        if(!CanSpawnPassive())
            return false;
        SpawnCow(x, y + 1.0f, z);
        return true;
    }
    if(itemId == SheepSpawnEgg::getID())
    {
        if(!CanSpawnPassive())
            return false;
        SpawnSheep(x, y + 1.0f, z);
        return true;
    }
    if(itemId == ZombieSpawnEgg::getID())
    {
        if(!CanSpawnHostile())
            return false;
        SpawnZombie(x, y + 0.8f, z);
        return true;
    }
    if(itemId == CreeperSpawnEgg::getID())
    {
        if(!CanSpawnHostile())
            return false;
        SpawnCreeper(x, y + 0.8f, z);
        return true;
    }
    if(itemId == PigSpawnEgg::getID())
    {
        if(!CanSpawnPassive())
            return false;
        SpawnPig(x, y + 1.0f, z);
        return true;
    }
    if(itemId == ChickenSpawnEgg::getID())
    {
        if(!CanSpawnPassive())
            return false;
        SpawnChicken(x, y + 1.0f, z);
        return true;
    }
    if(itemId == SpiderSpawnEgg::getID())
    {
        if(!CanSpawnHostile())
            return false;
        SpawnSpider(x, y + 0.8f, z);
        return true;
    }
    if(itemId == SkeletonSpawnEgg::getID())
    {
        if(!CanSpawnHostile())
            return false;
        SpawnSkeleton(x, y + 0.8f, z);
        return true;
    }
    return false;
}

static bool MobNameIs(const char* a, const char* b)
{
    if(a == NULL || b == NULL)
        return false;
    while(*a != 0 && *b != 0)
    {
        char ca = *a++;
        char cb = *b++;
        if(ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + 32);
        if(cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + 32);
        if(ca != cb)
            return false;
    }
    return *a == 0 && *b == 0;
}

LameMob* CraftWorld::SpawnMobByName(const char* name, float x, float y, float z)
{
    if(name == NULL || name[0] == 0)
        return NULL;
    if(MobNameIs(name, "cow"))
    {
        SpawnCow(x, y, z);
        return mCows.empty() ? NULL : mCows.back();
    }
    if(MobNameIs(name, "sheep"))
    {
        SpawnSheep(x, y, z);
        return mSheeps.empty() ? NULL : mSheeps.back();
    }
    if(MobNameIs(name, "pig"))
    {
        SpawnPig(x, y, z);
        return mPigs.empty() ? NULL : mPigs.back();
    }
    if(MobNameIs(name, "chicken"))
    {
        SpawnChicken(x, y, z);
        return mChickens.empty() ? NULL : mChickens.back();
    }
    if(MobNameIs(name, "zombie"))
    {
        SpawnZombie(x, y, z);
        return mZombies.empty() ? NULL : mZombies.back();
    }
    if(MobNameIs(name, "creeper"))
    {
        SpawnCreeper(x, y, z);
        return mCreepers.empty() ? NULL : mCreepers.back();
    }
    if(MobNameIs(name, "spider"))
    {
        SpawnSpider(x, y, z);
        return mSpiders.empty() ? NULL : mSpiders.back();
    }
    if(MobNameIs(name, "skeleton"))
    {
        SpawnSkeleton(x, y, z);
        return mSkeletons.empty() ? NULL : mSkeletons.back();
    }
    return NULL;
}

void CraftWorld::DestroySheep(int id)
{
    if(id < mSheeps.size() && id >= 0)
    {
        delete mSheeps[id];
        mSheeps.erase(mSheeps.begin()+id);
    }
}

void CraftWorld::StoreParkedPassive(int kind, LameMob* mob)
{
    if(!mob)
        return;
    int slot = parkedPassiveCount;
    if(slot >= kParkedPassiveMax)
    {
        int worst = 0;
        float worstD = -1.0f;
        for(int i = 0; i < parkedPassiveCount; i++)
        {
            float dx = parkedPassive[i].x - playerPos.x;
            float dz = parkedPassive[i].z - playerPos.z;
            float d = dx * dx + dz * dz;
            if(d > worstD)
            {
                worstD = d;
                worst = i;
            }
        }
        slot = worst;
    }
    else
        parkedPassiveCount++;

    ParkedPassive& p = parkedPassive[slot];
    p.kind = (unsigned char)kind;
    p.sheared = 0;
    p.color = 0;
    p.pad = 0;
    if(kind == 1)
    {
        Sheep* sheep = (Sheep*)mob;
        p.sheared = sheep->sheared ? 1 : 0;
        p.color = (unsigned char)sheep->GetFurColor();
    }
    p.x = mob->position.x;
    p.y = mob->position.y;
    p.z = mob->position.z;
    p.angle = mob->mainAngle;
    p.hp = mob->HP;
    p.age = mob->age;
}

static void RestoreParkedPassive(CraftWorld* world, const CraftWorld::ParkedPassive& p)
{
    if(p.kind == 0)
        world->SpawnCow(p.x, p.y, p.z);
    else if(p.kind == 1)
        world->SpawnSheep(p.x, p.y, p.z);
    else if(p.kind == 2)
        world->SpawnPig(p.x, p.y, p.z);
    else if(p.kind == 3)
        world->SpawnChicken(p.x, p.y, p.z);
    else
        return;

    LameMob* mob = 0;
    if(p.kind == 0 && world->mCows.empty() == false)
        mob = world->mCows.back();
    else if(p.kind == 1 && world->mSheeps.empty() == false)
        mob = world->mSheeps.back();
    else if(p.kind == 2 && world->mPigs.empty() == false)
        mob = world->mPigs.back();
    else if(p.kind == 3 && world->mChickens.empty() == false)
        mob = world->mChickens.back();
    if(!mob)
        return;
    mob->mainAngle = p.angle;
    mob->HP = p.hp;
    mob->SetAge(p.age);
    if(p.kind == 1)
    {
        Sheep* sheep = (Sheep*)mob;
        sheep->sheared = p.sheared != 0;
        sheep->SetFurColor(p.color);
    }
}

template<typename T>
static void ParkFarPassives(std::vector<T*>& mobs, int kind, float parkDist2, CraftWorld* world)
{
    for(int i = (int)mobs.size() - 1; i >= 0; --i)
    {
        T* mob = mobs[i];
        if(!mob || mob->killed || mob->toDestroy || mob->poseLock)
            continue;
        float dx = mob->position.x - world->playerPos.x;
        float dz = mob->position.z - world->playerPos.z;
        if(dx * dx + dz * dz < parkDist2)
            continue;
        world->StoreParkedPassive(kind, mob);
        delete mob;
        mobs.erase(mobs.begin() + i);
    }
}

void CraftWorld::MaintainPassiveHerd(float dt)
{
    passiveHerdTimer += dt;
    if(passiveHerdTimer < 1.0f)
        return;
    passiveHerdTimer = 0.0f;

    int view = (int)mainOptions.horizontalViewDistance;
    if(view < 1)
        view = 1;
    // Just outside the distance at which the animal stops being updated. Inside that, it plays.
    // Back inside a slightly smaller distance, a stored one is a mob again. The gap stops a herd
    // on the boundary from being stored and restored every second.
    float parkDist = (float)view * 14.0f + 8.0f;
    float wakeDist = (float)view * 14.0f + 2.0f;
    float parkDist2 = parkDist * parkDist;
    float wakeDist2 = wakeDist * wakeDist;

    ParkFarPassives(mCows, 0, parkDist2, this);
    ParkFarPassives(mSheeps, 1, parkDist2, this);
    ParkFarPassives(mPigs, 2, parkDist2, this);
    ParkFarPassives(mChickens, 3, parkDist2, this);

    for(int i = parkedPassiveCount - 1; i >= 0; --i)
    {
        if(PassiveMobCount() >= PassiveMobCap())
            break;
        float dx = parkedPassive[i].x - playerPos.x;
        float dz = parkedPassive[i].z - playerPos.z;
        if(dx * dx + dz * dz > wakeDist2)
            continue;
        int x = (int)parkedPassive[i].x;
        int z = (int)parkedPassive[i].z;
        if(CHUNK_SIZE <= 0)
            continue;
        ChunkColumn* col = FindColumn(ChunkFloorDiv(x, CHUNK_SIZE), ChunkFloorDiv(z, CHUNK_SIZE));
        if(!col || !col->generated)
            continue;
        RestoreParkedPassive(this, parkedPassive[i]);
        parkedPassive[i] = parkedPassive[parkedPassiveCount - 1];
        parkedPassiveCount--;
    }
}

void CraftWorld::TryAnimalPack(int cx, int cz)
{
    if(dimension != 0)
        return;
    if(PassiveMobCount() >= NaturalPassiveCap())
        return;
    // About one column in ten, which is the plains rate a new chunk grows a creature pack.
    if((rand() % 10) != 0)
        return;
    int kind = rand() % 4;
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;
    if(CHUNK_SIZE <= 0)
        return;
    int placed = 0;
    for(int t = 0; t < 8 && placed < 2; t++)
    {
        if(PassiveMobCount() >= NaturalPassiveCap())
            return;
        int x = x0 + (rand() % CHUNK_SIZE);
        int z = z0 + (rand() % CHUNK_SIZE);
        int y = groundHeightWater(x, z);
        if(y <= 0 || !MobSpawnSpotOk(x, y, z, false))
            continue;
        float sx = x + 0.5f;
        float sy = y + 2.0f;
        float sz = z + 0.5f;
        if(kind == 0)
            SpawnCow(sx, sy, sz);
        else if(kind == 1)
            SpawnSheep(sx, sy, sz);
        else if(kind == 2)
            SpawnPig(sx, sy, sz);
        else
            SpawnChicken(sx, sy, sz);
        placed++;
    }
}
