#ifndef SIMPLEMESHCHUNK_H
#define SIMPLEMESHCHUNK_H

#include <string>
//framework
#include <Aurora/Math/BoundingBox.h>
#include <Aurora/Math/Vector2.h>
#include <Aurora/Graphics/Vertex.h>
#include <cstdlib>

using namespace Aurora::Math;
using namespace Aurora::Graphics;

/*
 * The scratch the mesh builders accumulate geometry in before it is copied into the GPU buffers.
 *
 * WHY THIS IS NOT A std::vector. It was one, and it killed the game. A vector grows by doubling
 * through operator new, and this build's operator new is the heap guard's -- which cannot return
 * null and so calls abort(). The mesher's vertex scratch doubles through 49 KB, 98 KB and finally
 * 196,608 bytes (8192 vertices), and on a 32 MB-partition PSP the heap is down to a few hundred KB
 * of *scattered* free space by the time a large section is rebuilt. The doubling request could not
 * be satisfied by any single free block, so the game aborted mid-session with
 * "HEAP GUARD: OUT OF MEMORY allocating 196608 bytes" -- a hard crash over one section's scratch.
 *
 * This buffer grows with realloc instead, reports failure by RETURNING FALSE, and never aborts. A
 * section whose scratch does not fit is left empty and is rebuilt when it next changes, which is a
 * far better outcome than losing the session. realloc also extends in place when it can, so the
 * churn that fragmented the heap in the first place is gone.
 *
 * kMaxBytes is a sanity bound: without it a corrupt section could ask for megabytes it has no use
 * for. Real sections are nowhere near it -- the largest observed needs 256 KB.
 */
template <typename T>
class MeshScratch
{
public:
	MeshScratch() : mData(0), mSize(0), mCapacity(0), mFailed(false) {}
	~MeshScratch() { free(mData); }

	unsigned int size() const { return mSize; }
	bool failed() const { return mFailed; }
	bool empty() const { return mSize == 0; }

	T* data() { return mData; }
	const T* data() const { return mData; }
	T& operator[](unsigned int index) { return mData[index]; }
	const T& operator[](unsigned int index) const { return mData[index]; }
	T& back() { return mData[mSize - 1]; }

	// Kept between rebuilds only within one build; the callers release() it when they are done, so
	// the steady-state cost is unchanged from when this was a vector.
	void clear() { mSize = 0; mFailed = false; }

	void release()
	{
		free(mData);
		mData = 0;
		mSize = 0;
		mCapacity = 0;
		mFailed = false;
	}

	// Best-effort: a reserve that cannot be met is not an error, because push_back can still grow
	// the buffer later if memory frees up.
	void reserve(unsigned int items)
	{
		if(items > mCapacity)
			GrowTo(items);
	}

	bool push_back(const T& value)
	{
		if(mSize >= mCapacity && !GrowTo(mCapacity == 0 ? kInitial : mCapacity * 2))
		{
			// Out of memory, not out of room: the callback cannot be completed, and the caller
			// finds out from failed() rather than from an abort several frames later.
			mFailed = true;
			return false;
		}

		mData[mSize++] = value;
		return true;
	}

private:
	enum { kInitial = 256, kMaxBytes = 256u * 1024u };

	bool GrowTo(unsigned int items)
	{
		// Cap by bytes rather than items so both instantiations are bounded the same way.
		const unsigned int maxItems = (unsigned int)(kMaxBytes / sizeof(T));
		if(items > maxItems)
			items = maxItems;
		if(items <= mCapacity)
			return false;

		T* grown = (T*)realloc(mData, (size_t)items * sizeof(T));
		if(grown == 0)
			return false;

		mData = grown;
		mCapacity = items;
		return true;
	}

	T* mData;
	unsigned int mSize;
	unsigned int mCapacity;
	bool mFailed;
};

class SimpleMeshChunk
{
public:

	//unsigned short id;
    bool inFrustum;

	int chunkStartZ;
	int chunkStartY;
	int chunkStartX;

	bool periodicallyUpadted;
	bool haveTransparentVerts;

	BoundingBox bBox;

	SimpleMeshChunk();
	SimpleMeshChunk(int _chunkSize);
	~SimpleMeshChunk();

	int trienglesCount;   // number of indices (or expanded vertices) drawn
	bool created;
	bool needUpdate;
	bool outOfMemory;    // last end() could not allocate the mesh; the previous one stays on screen
	bool rejectedEmpty; // end() kept the previous mesh because the new build had no faces
	// The ground passes through this section (its column's surface band). Such a section is in the
	// player's zone whenever its column is, whatever height the player is at -- see
	// CraftWorld::MeshInPlayerZone.
	bool surfaceBand;
	// The walk that built this mesh stopped once the column was sealed stone. The ground, water
	// and cliffs are here; a cave under that seal is not, until a later walk with this flag clear.
	bool skinOnly;

	void position(float x,float y,float z);
	void textureCoord(float u,float v);
	void triangle(int x,int y,int z);
	void colour(float x,float y,float z);

	void info(float x,float y,float z,float u,float v,float r,float g,float b);
	void vert(float x,float y,float z,float u,float v,float r,float g,float b);

	int currentVertexCount();
	void multiplyColourFrom(int start, float rMul, float gMul, float bMul);

	void start();
	// allowEmpty: a rebuild that produced no triangles may replace the mesh on screen.
	// Pass false when the section still has faces. An empty result then keeps the old
	// buffer, which is what stops a drawn chunk from turning into air.
	void end(bool allowEmpty = true);

	void drawChunk();
	void reset();
	void beginRebuild();

	CraftPSPVertex *meshVertices;
	unsigned short *meshIndices;  // NULL when the expanded (non-indexed) fallback is used

	// Which of the two shared build scratches this chunk accumulates into: 0 = opaque, 1 = transparent.
	//
	// The scratch used to be two members of this class, i.e. one pair per section, grown with realloc
	// and RELEASED at the end of every build (see MeshScratch). That is a full allocate/grow/free cycle
	// of up to 256 KB for every section rebuild, and on this device there are rebuilds constantly --
	// which is a fragmentation engine. The heap ends up holding a megabyte of scattered holes and no
	// single block big enough for the next scratch, so the mesher is left failing on sections it has
	// room for. The card's own log is that: `mesh OOM at 272,64,256` with `heapFreeKB=1264`, the same
	// coordinate, forever.
	//
	// Two scratches shared by every section and never released turn that around. Only one rebuild job
	// runs at a time (RequestMeshRebuild refuses a second, and `meshBuildsThisFrame` refuses an inline
	// one while a job holds the caches), so a shared buffer is never two sections' worth -- the live
	// scratch memory is the same as it always was, it simply stops being freed and re-found. The two
	// slots exist because a section's opaque and transparent meshes are walked together in one pass
	// and both must accumulate at once.
	//
	// The cost is that the scratches keep their high-water mark for the session, which is one opaque
	// section's worth of vertices plus one transparent's -- the same buffers that were resident while
	// the last job ran.
	void SetScratchSlot(int slot) { mScratchSlot = (slot != 0) ? 1 : 0; }

private:
	int mScratchSlot;
};

#endif

