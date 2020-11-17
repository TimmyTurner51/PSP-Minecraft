#include "LoadingScreen.h"
#include "TextureHelper.h"
#include <pspthreadman.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>

#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Graphics/Sprite.h>

// font vars
#define default_size 0.5
#define default_big_size 0.687

#define ENGLISH 1
#define RUSSIAN 2

using namespace Aurora::Graphics;

Sprite* backSprite;
Sprite* loadSprite;
Sprite* subLoadSprite;

int LoadingScreen::readiness = 0;
int LoadingScreen::stateName = 0;

LoadingScreen::LoadingScreen()
{
	// the loadingscreen is loaded as a thread
	thid_ = sceKernelCreateThread("LoadingThread", RunLoadingScreen, 0x18, 0x10000, THREAD_ATTR_VFPU|THREAD_ATTR_USER, NULL);
	// start the thread
	sceKernelStartThread(thid_, 0, 0);
}


void LoadingScreen::KillLoadingScreen()
{
	// shut down the loading screen again.
	sceKernelTerminateDeleteThread(thid_);
	// free the mem space of the images
	delete backSprite;
	delete loadSprite;
	delete subLoadSprite;
}


int LoadingScreen::RunLoadingScreen(SceSize args, void *argp)
{
	// load up the images
	backSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Dirt),0,0,32,32);
	backSprite->Scale(2,2);

    loadSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Load),0,0,16,8);
	loadSprite->Scale(1,0.5f);

    subLoadSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Load),0,8,16,8);
	subLoadSprite->Scale(1,0.5f);


    int tip = rand() % 6;
	float loadingProcess = 0.0f;

	// start the render loop
	while(1)
	{
		RenderManager::InstancePtr()->StartFrame(0.466,0.72,1);

		sceGuDisable(GU_DEPTH_TEST);
		sceGuEnable(GU_BLEND);
		sceGuColor(GU_COLOR(1,1,1,1.0f));

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 5; y++)
            {
                backSprite->SetPosition(x*64,y*64);
                backSprite->Draw();

                sceGuDisable(GU_BLEND);
                sceGuEnable(GU_DEPTH_TEST);
            }
        }

        if(readiness == 0)
        {
            if(loadingProcess < 160)
            {
                loadingProcess += 1.1f;
            }
        }
        else
        {
            if(loadingProcess <= readiness/100.0f*160)
            {
                loadingProcess += 0.65f;
            }
        }

        subLoadSprite->NewScale(160);
        subLoadSprite->SetPosition(160,150);

        loadSprite->NewScale(loadingProcess);
        loadSprite->SetPosition(160,150);

        if(loadingProcess < 159)
        {
            subLoadSprite->Draw();
            loadSprite->Draw();
        }

		sceGuDisable(GU_BLEND);
		sceGuEnable(GU_DEPTH_TEST);

		//draw subtitles on buttons
        RenderManager::InstancePtr()->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000200|0x00000000);

        if(RenderManager::InstancePtr()->GetFontLanguage() == ENGLISH)
        {
            if(stateName == 0)
            {
                RenderManager::InstancePtr()->DebugPrint(240,103,"Loading spawn area");
            }
            else
            {
                RenderManager::InstancePtr()->DebugPrint(240,103,"Initializing server");
                switch(stateName)
                {
                    case 1:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Finding Seed for the World Generator",(readiness*2)-1);//%i %%
                    break;
                    case 2:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                    case 3:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                    case 4:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                    case 5:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                    case 6:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                    case 7:
                    RenderManager::InstancePtr()->DebugPrint(240,136,"Loading spawn area");
                    break;
                }
            }

            RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.45f,0.45f,0.45f,1.0f),0,0x00000200|0x00000000);
            switch(tip)
            {
                case 0:
                RenderManager::InstancePtr()->DebugPrint(240,240,"Music by C418!");
                break;
                case 1:
                RenderManager::InstancePtr()->DebugPrint(240,240,"Use a hoe to prepare areas of ground for planting.");
                break;
                case 2:
				RenderManager::InstancePtr()->DebugPrint(240, 240, "Obsidian can only be mined with a diamond pickaxe.");
                break;
                case 3:
				RenderManager::InstancePtr()->DebugPrint(240, 240, "Minecon 2012 was in Paris, France!");
                break;
                case 4:
                RenderManager::InstancePtr()->DebugPrint(240,240,"Use the right tool for the job!");
                break;
                case 5:
                RenderManager::InstancePtr()->DebugPrint(240,240,"If you have an empty bucket, you can fill it with");
                RenderManager::InstancePtr()->DebugPrint(240,255,"milk from a cow, or water, or lava!");
                break;
            }
        }
            RenderManager::InstancePtr()->SetFontStyle(default_size,GU_COLOR(0.45f,0.45f,0.45f,1.0f),0,0x00000200|0x00000000);
            switch(tip)
            
        RenderManager::InstancePtr()->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000200|0x00000000);
		RenderManager::InstancePtr()->EndFrame();
	}
	return 0;
}