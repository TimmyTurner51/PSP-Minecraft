#include "SimpleMeshChunk2.h"
#include <Aurora/Graphics/RenderManager.h>
#include <pspgu.h>
#include <pspgum.h>
#include <cstdlib>
#include <cstring>


namespace {
// Mesh buffers the GE may still be reading. Since EndFrame only submits the list and the world
// update runs while the GE draws it, a buffer a section stops using (rebuild, reset, column unload)
// cannot be freed on the spot: it is parked here and released by the frame-retired hook, which
// RenderManager runs once the GE has finished the list. 512 slots cover a whole dimension change
// (every section's two buffers); if even that fills, the GE is waited for and the queue drained.
enum { kDeferredMeshSlots = 512 };
void* gDeferredMeshes[kDeferredMeshSlots];
int gDeferredCount = 0;
bool gDeferredHookInstalled = false;

void FlushDeferredMeshes()
{
	for(int i = 0; i < gDeferredCount; i++)
	{
		if(gDeferredMeshes[i] != NULL)
			free(gDeferredMeshes[i]);
		gDeferredMeshes[i] = NULL;
	}
	gDeferredCount = 0;
}

void DeferMeshFree(void* p)
{
	if(p == NULL)
		return;
	if(!gDeferredHookInstalled)
	{
		Aurora::Graphics::RenderManager::InstancePtr()->SetFrameRetiredHook(FlushDeferredMeshes);
		gDeferredHookInstalled = true;
	}
	if(gDeferredCount >= kDeferredMeshSlots)
	{
		Aurora::Graphics::RenderManager::InstancePtr()->WaitForGe();
		FlushDeferredMeshes();
	}
	gDeferredMeshes[gDeferredCount++] = p;
}

// The shared build scratches. Two slots: a section's opaque and transparent meshes are walked in the
// same pass, so both accumulate simultaneously. Never released between builds -- see SetScratchSlot
// for why the release was the problem. Defined here rather than as members so every section reuses
// the same buffers instead of each finding and freeing its own.
MeshScratch<CraftPSPVertex> sScratchVerts[2];
MeshScratch<Vector3> sScratchTris[2];
}

SimpleMeshChunk::SimpleMeshChunk()
{
	trienglesCount = 0;
	created = true;
	needUpdate = false;
	periodicallyUpadted = false;
	outOfMemory = false;
	rejectedEmpty = false;
	surfaceBand = false;
	skinOnly = false;

	// Must be initialised: vert/position subtract this origin from every coordinate they are given and
	// drawChunk adds it back, so an uninitialised value here would be applied twice and move the
	// section. It was previously never set in the constructor and only happened to be written by
	// CreateFullMeshChunk before the first vertex.
	chunkStartX = 0;
	chunkStartY = 0;
	chunkStartZ = 0;

	inFrustum = false;
	mScratchSlot = 0;

	meshVertices = NULL;
	meshIndices = NULL;
}

SimpleMeshChunk::SimpleMeshChunk(int _chunkSize)
{
	trienglesCount = 0;
	created = true;
	needUpdate = false;
	periodicallyUpadted = false;
	outOfMemory = false;
	rejectedEmpty = false;
	surfaceBand = false;
	skinOnly = false;

	chunkStartX = 0;
	chunkStartY = 0;
	chunkStartZ = 0;

	inFrustum = false;
	mScratchSlot = 0;

	meshVertices = NULL;
	meshIndices = NULL;
}

SimpleMeshChunk::~SimpleMeshChunk()
{
	// A section is deleted when its column unloads, in the update, while the last frame's list may
	// still be drawing it.
	DeferMeshFree(meshVertices);
	DeferMeshFree(meshIndices);
	meshVertices = NULL;
	meshIndices = NULL;
}

void SimpleMeshChunk::info(float x,float y,float z,float u,float v,float r,float g,float b)
{

}

void SimpleMeshChunk::vert(float x,float y,float z,float u,float v,float r,float g,float b)
{
    CraftPSPVertex vertex;

	vertex.u = u;
	vertex.v = v;
	vertex.color = GU_COLOR(r,g,b,1);
	// Stored relative to this section's own origin, not as absolute world coordinates. drawChunk puts
	// the origin back with a single translate, so the geometry lands in exactly the same place -- but
	// the numbers the GE transforms become small.
	//
	// The builders hand over absolute coordinates of ~2048, which forced the GE to multiply them by a
	// matrix whose translation was also ~2048 and cancel two large operands to reach a small result.
	// That cancellation is where the fractional bits went, and because the rotation part of the matrix
	// changes every frame, the error moved with the camera: the terrain swam and wobbled while turning,
	// on hardware only (PPSSPP does this transform in host floats and cannot show it).
	vertex.x = x - (float)chunkStartX;
	vertex.y = y - (float)chunkStartY;
	vertex.z = z - (float)chunkStartZ;

	sScratchVerts[mScratchSlot].push_back(vertex);
}

void SimpleMeshChunk::position(float x,float y,float z)
{
	CraftPSPVertex vertex;

	// Section-relative, for the same reason as vert above.
	vertex.x = x - (float)chunkStartX;
	vertex.y = y - (float)chunkStartY;
	vertex.z = z - (float)chunkStartZ;
	// The two texture values are the caller's to fill in with textureCoord; they are set here only so
	// the value is never indeterminate.
	vertex.u = 0;
	vertex.v = 0;

	sScratchVerts[mScratchSlot].push_back(vertex);
}

void SimpleMeshChunk::textureCoord(float u,float v)
{
	sScratchVerts[mScratchSlot][sScratchVerts[mScratchSlot].size()-1].u = u;
	sScratchVerts[mScratchSlot][sScratchVerts[mScratchSlot].size()-1].v = v;
}

void SimpleMeshChunk::colour(float x,float y,float z)
{
	sScratchVerts[mScratchSlot][sScratchVerts[mScratchSlot].size()-1].color = GU_COLOR(x,y,z,1);
}

void SimpleMeshChunk::triangle(int x,int y,int z)
{
    Vector3 triangle;
    triangle.x = x;
    triangle.y = y;
    triangle.z = z;

    sScratchTris[mScratchSlot].push_back(triangle);
}

int SimpleMeshChunk::currentVertexCount()
{
	return (int)sScratchVerts[mScratchSlot].size();
}

void SimpleMeshChunk::multiplyColourFrom(int start, float rMul, float gMul, float bMul)
{
	if(start < 0)
	{
		start = 0;
	}
	MeshScratch<CraftPSPVertex>& verts = sScratchVerts[mScratchSlot];
	for(int i = start; i < (int)verts.size(); i++)
	{
		unsigned int c = verts[i].color;
		float r = ((c) & 0xFF) / 255.0f * rMul;
		float g = ((c >> 8) & 0xFF) / 255.0f * gMul;
		float b = ((c >> 16) & 0xFF) / 255.0f * bMul;
		float a = ((c >> 24) & 0xFF) / 255.0f;
		if(r > 1.0f) r = 1.0f;
		if(g > 1.0f) g = 1.0f;
		if(b > 1.0f) b = 1.0f;
		if(r < 0.0f) r = 0.0f;
		if(g < 0.0f) g = 0.0f;
		if(b < 0.0f) b = 0.0f;
		verts[i].color = GU_COLOR(r,g,b,a);
	}
}

void SimpleMeshChunk::end(bool allowEmpty)
{
	rejectedEmpty = false;
	CraftPSPVertex* oldMesh = meshVertices;
	unsigned short* oldIndices = meshIndices;
	const int oldTris = trienglesCount;
	meshVertices = NULL;
	meshIndices = NULL;
	trienglesCount = 0;

	MeshScratch<CraftPSPVertex>& mVertices = sScratchVerts[mScratchSlot];
	MeshScratch<Vector3>& mTriangle = sScratchTris[mScratchSlot];

	unsigned int triCount = mTriangle.size();
	unsigned int vertCount = mVertices.size();

	// A scratch that ran out mid-section leaves the triangle indices pointing at vertices that were
	// never stored, so the geometry cannot be trusted and must not be drawn. Leaving the section
	// empty is the whole point of this change: it used to abort the game instead (see MeshScratch).
	outOfMemory = false;
	if(mVertices.failed() || mTriangle.failed())
	{
		outOfMemory = true;
		triCount = 0;
		vertCount = 0;
	}

	if(triCount > 0 && vertCount > 0)
	{
		// Indexed geometry: the builder already emits 4 unique vertices + 2 triangles per
		// quad, so keep those vertices as-is and draw through a 16-bit index list. That is
		// 4*24 + 6*2 = 108 bytes per quad instead of 6*24 = 144 for the expanded copy.
		// Sections with more than 65535 vertices cannot be indexed and fall back to the
		// expanded form. Allocation failure (PSP-1000 running dry) leaves the section
		// empty instead of writing through a null pointer.
		if(vertCount <= 65535)
		{
			meshVertices = (CraftPSPVertex*)memalign(16, vertCount * sizeof(CraftPSPVertex));
			meshIndices = (unsigned short*)memalign(16, triCount * 3 * sizeof(unsigned short));
			if(meshVertices != NULL && meshIndices != NULL)
			{
				memcpy(meshVertices, mVertices.data(), vertCount * sizeof(CraftPSPVertex));
				unsigned short* idx = meshIndices;
				for(unsigned int i = 0; i < triCount; i++)
				{
					*idx++ = (unsigned short)(int)mTriangle[i].x;
					*idx++ = (unsigned short)(int)mTriangle[i].y;
					*idx++ = (unsigned short)(int)mTriangle[i].z;
				}
				sceKernelDcacheWritebackInvalidateRange(meshVertices, vertCount * sizeof(CraftPSPVertex));
				sceKernelDcacheWritebackInvalidateRange(meshIndices, triCount * 3 * sizeof(unsigned short));
				trienglesCount = (int)(triCount * 3);
			}
		}
		else
		{
			meshVertices = (CraftPSPVertex*)memalign(16, (triCount * 3) * sizeof(CraftPSPVertex));
			if(meshVertices != NULL)
			{
				int vert = 0;
				for(unsigned int i = 0; i < triCount; i++)
				{
					meshVertices[vert++] = mVertices[(int)mTriangle[i].x];
					meshVertices[vert++] = mVertices[(int)mTriangle[i].y];
					meshVertices[vert++] = mVertices[(int)mTriangle[i].z];
				}
				sceKernelDcacheWritebackInvalidateRange(meshVertices, (triCount * 3) * sizeof(CraftPSPVertex));
				trienglesCount = (int)(triCount * 3);
			}
		}

		if(trienglesCount == 0)
		{
			// out of memory: drop whatever half succeeded
			if(meshVertices != NULL) free(meshVertices);
			if(meshIndices != NULL) free(meshIndices);
			meshVertices = NULL;
			meshIndices = NULL;
			outOfMemory = true;
		}
	}

	if(outOfMemory)
	{
		// The new buffer did not fit. Putting the previous mesh back is what keeps the ground
		// on screen; discarding it is how a failed cave build turned the whole view empty.
		// The scratch is released too: clear() would have kept the cave-sized allocation, and
		// that is the block that left the heap at a few hundred KB.
		if(oldMesh != NULL)
		{
			meshVertices = oldMesh;
			meshIndices = oldIndices;
			trienglesCount = oldTris;
			oldMesh = NULL;
			oldIndices = NULL;
		}
		mVertices.release();
		mTriangle.release();
	}
	else
	{
		// Reset the counters but KEEP the buffers. This is the difference between a scratch that is
		// re-found on every rebuild and one that is kept: see SetScratchSlot.
		mVertices.clear();
		mTriangle.clear();
	}

	// A rebuild that emitted nothing must not delete geometry that is already being drawn.
	// Walking up to a chunk used to start that rebuild (the column finishing, a neighbour
	// arriving) and the chunk turned into air. A section the world has actually emptied
	// still drops its mesh: the caller passes allowEmpty for that.
	if(!outOfMemory && trienglesCount == 0 && oldTris > 0 && oldMesh != NULL && !allowEmpty)
	{
		meshVertices = oldMesh;
		meshIndices = oldIndices;
		trienglesCount = oldTris;
		oldMesh = NULL;
		oldIndices = NULL;
		rejectedEmpty = true;
	}

	if(oldMesh != NULL && oldMesh != meshVertices)
		DeferMeshFree(oldMesh);
	if(oldIndices != NULL && oldIndices != meshIndices)
		DeferMeshFree(oldIndices);

	created = true;
	needUpdate = false;
}

void SimpleMeshChunk::drawChunk()
{
	if(trienglesCount > 0 && meshVertices != NULL)
    {
		// Put this section's origin back, since vert/position stored everything relative to it.
		//
		// Composed with the world model matrix (which rebases to worldOrigin) the effective translation
		// becomes (chunkStart - cameraPos) -- tens of units for anything on screen, where absolute
		// vertices forced ~2048. That is the whole point: it is the small operand that stops the GE
		// from losing the fractional bits it needs, and it is what removes the wobble during turning.
		// Push/pop rather than load, so the world model matrix survives for the next section. The stack
		// is never more than one deeper than it was on entry.
		sceGumMatrixMode(GU_MODEL);
		sceGumPushMatrix();
		ScePspFVector3 origin = {(float)chunkStartX, (float)chunkStartY, (float)chunkStartZ};
		sceGumTranslate(&origin);

		if(meshIndices != NULL)
			sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF |GU_COLOR_8888|GU_VERTEX_32BITF|GU_INDEX_16BIT| GU_TRANSFORM_3D, trienglesCount, meshIndices, meshVertices);
		else
			sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF |GU_COLOR_8888|GU_VERTEX_32BITF| GU_TRANSFORM_3D, trienglesCount, 0, meshVertices);

		sceGumPopMatrix();
    }
}

void SimpleMeshChunk::reset()
{
	DeferMeshFree(meshVertices);
	DeferMeshFree(meshIndices);
	meshVertices = NULL;
	meshIndices = NULL;
	trienglesCount = 0;
	created = false;
	// The scratch is shared by every section and is what the mesh job is filling right now.
	// Clearing it here used to wipe that job: a column unloading behind the player erased the
	// vertices of the chunk in front, and end() then published the empty scratch over the mesh
	// that was on screen. The next beginRebuild clears it, which is the only place that should.
}

void SimpleMeshChunk::beginRebuild()
{
	created = false;
	// clear() keeps the high-water capacity; release() would throw it away. The buffers are shared,
	// so the first build after a fresh world still grows them, and every build after that does not.
	sScratchVerts[mScratchSlot].clear();
	sScratchTris[mScratchSlot].clear();
	sScratchVerts[mScratchSlot].reserve(2048);
	sScratchTris[mScratchSlot].reserve(1024);
}
