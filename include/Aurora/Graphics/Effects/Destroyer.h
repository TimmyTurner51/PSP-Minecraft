#ifndef DESTROYER_H_
#define DESTROYER_H_

#include <Aurora/Graphics/Vertex.h>
#include <Aurora/Graphics/RenderManager.h>
#include <pspmath.h>
#include <Aurora/Math/Frustum.h>
#include <Scripts/CraftWorld2.h>
#include <Aurora/Math/BoundingBox.h>

namespace Aurora
{
	namespace Graphics
	{
		class Destroyer
		{
		public:
			Destroyer();
			~Destroyer();

			// Cell coordinates, so the shape can be asked of the world: a button's box depends on the
			// facing and pressed state stored in the cell, not just on the block id.
			void Update(float stad, CraftWorld* crft, int blockId, int cellX, int cellY, int cellZ);
			void Render(float stad);

		private:

			TexturesPSPVertex *boxVertices;
			TexturesPSPVertex *boxVertices2;
			TexturesPSPVertex *boxVertices3;
			TexturesPSPVertex *boxVertices4;
			TexturesPSPVertex *boxVertices5;
			TexturesPSPVertex *boxVertices6;

			SimplePSPVertex2 *boxVertices11;
			SimplePSPVertex2 *boxVertices12;
			SimplePSPVertex2 *boxVertices13;
			SimplePSPVertex2 *boxVertices14;
			SimplePSPVertex2 *boxVertices15;
			SimplePSPVertex2 *boxVertices16;

			float size;
		};
	}
}

#endif

