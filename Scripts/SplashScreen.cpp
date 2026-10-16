#include "SplashScreen.h"
#include "InitTrace.h"

SplashScreen::SplashScreen(const char* filename,int startW,int startH,int endW,int endH,float splashTime)
{
	// load up the images
	sprite = new Sprite(filename,startW,startH,endW,endH);
	sprite->SetPosition(240,136);

	finishTime = splashTime;
	timer = 0.0f;
	dt = mTimer.GetDeltaTime();
}

SplashScreen::SplashScreen(int textureNumber,int startW,int startH,int endW,int endH,float splashTime)
{
	// The texture number is recorded rather than just "the splash was constructed", because -1 here
	// is the interesting case: it means TextureHelper never loaded the splash image and this Sprite is
	// about to be built on a texture that does not exist.
	MC_TraceInt("splash: ctor with texture", textureNumber);

	sprite = new Sprite(textureNumber,startW,startH,endW,endH);
	MC_TracePtr("splash: sprite allocated", sprite);
	sprite->SetPosition(240,136);

	finishTime = splashTime;
	timer = 0.0f;
	dt = mTimer.GetDeltaTime();
	MC_Trace("splash: ctor complete");
}

SplashScreen::~SplashScreen()
{
	sprite->RemoveImage();
	delete sprite;
}


void SplashScreen::ShowSplash()
{
	MC_Trace("splash: ShowSplash enter");
	dt = mTimer.GetDeltaTime();
	int frames = 0;
	while(timer < finishTime)
	{
		dt = mTimer.GetDeltaTime();

		RenderManager::InstancePtr()->StartFrame(1,1,1);

		sceGuDisable(GU_DEPTH_TEST);
		sceGuEnable(GU_BLEND);
		sceGuColor(GU_COLOR(1,1,1,1.0f));

		sprite->Draw();

		sceGuDisable(GU_BLEND);
		sceGuEnable(GU_DEPTH_TEST);

		RenderManager::InstancePtr()->EndFrame();

		// Once, not per frame: this loop runs for three seconds, and a line per frame would be a
		// hundred flushes of the same information. The first frame is the one that matters -- it is
		// the first time this game drives the display itself.
		if(frames == 0)
			MC_Trace("splash: first frame drawn and presented");

		frames++;
		timer+=dt;
	}

	MC_TraceInt("splash: ShowSplash done, frames drawn", frames);
	return;
}

