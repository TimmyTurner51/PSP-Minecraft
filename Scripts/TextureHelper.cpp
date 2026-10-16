#include "TextureHelper.h"
#include "DebugConfig.h"
#include "BootTrace.h"
#include "InitTrace.h"
#include <Aurora/Graphics/RenderManager.h>
#include <Aurora/Utils/Logger.h>

TextureHelper::TextureHelper()
{
	// Names this constructor in the trace. Global constructors run before main() and before Log.txt
	// exists, and the constructor phase is the one part of startup no in-game log can describe -- so if
	// a console dies in here, the trace file is the only place the fact can be recorded. See
	// Scripts/InitTrace.h.
	MC_Trace("ctor: TextureHelper entering");

	// One line per statement, because this constructor is where the game dies on hardware and its
	// three assignments are not equally innocent. The first one is the interesting one: "Assets/Textures/"
	// is 16 characters, one past libstdc++'s small-string buffer, so `defaulPatch = "..."` is the first
	// dynamic allocation the program ever performs. The other two stay inside the small-string buffer
	// for their inputs, and the third only allocates for the concatenated result.
	//
	// Splitting them turns "died in TextureHelper's constructor" into "died in the first allocation it
	// makes", which is a different bug with a different fix.
	MC_Trace("ctor: TextureHelper assigning defaulPatch (first heap allocation in the program)");
	defaulPatch = "Assets/Textures/";
	MC_Trace("ctor: TextureHelper defaulPatch done");

	MC_Trace("ctor: TextureHelper assigning defaultFolder");
	defaultFolder = "Minecraft/";
	MC_Trace("ctor: TextureHelper defaultFolder done");

	MC_Trace("ctor: TextureHelper assigning texturePatch");
	texturePatch = defaulPatch+defaultFolder;
	MC_Trace("ctor: TextureHelper texturePatch done");

	MC_Trace("ctor: TextureHelper done");
}

TextureHelper::~TextureHelper()
{

}

void TextureHelper::Init()
{
    LoadTexturePackPath();
    texturePatch = defaulPatch+defaultFolder;

	//FastLoadTexture("title/esrb.png");
	FastLoadTexture("title/mojang_splash_screen.png");
    //FastLoadTexture("title/YJ.png");
    FastLoadTexture("environment/moon.png");
    FastLoadTexture("hud/utils.png");
	FastLoadTexture("hud/xpbar.png");
    FastLoadTexture("title/dirt.png");
    FastLoadTexture("terrain.png");
    FastLoadTexture("terrain_mips.png");
    FastLoadTexture("water_still.png");
    FastLoadTexture("lava_still.png");
	FastLoadTexture("fire_0.png");
	FastLoadTexture("fire_1.png");
    // The nether portal strip. It has to be in BOTH texture lists -- Init's and SetTexturePack's --
    // because GetTextureNumber resolves a name through whatever list was last loaded and returns -1
    // otherwise. Without this the portal renders as a missing texture however present the file is,
    // which is exactly what the log reported, and it would break again on every pack switch.
    FastLoadTexture("portal.png");
    FastLoadTexture("items.png");
    FastLoadTexture("hud/blue.png");
    FastLoadTexture("environment/sun.png");
    FastLoadTexture("environment/moon.png");
    FastLoadTexture("environment/stars.png");
    FastLoadTexture("entity/snowball.png");
    FastLoadTexture("entity/arrow.png");
    FastLoadTexture("entity/zombie/zombie.png");
    FastLoadTexture("entity/skeleton/skeleton.png");
    FastLoadTexture("entity/player_skins/steve.png");
    FastLoadTexture("title/logo.png");
    FastLoadTexture("title/choose_rectangles.png");
    FastLoadTexture("title/menu_elements.png");
    FastLoadTexture("title/load.png");
    // The News & Converter changelog's d-pad hints. GetTextureRaw can only return a texture that has
    // been registered here -- TextureManager::GetTextureNumber looks the name up in this list and
    // returns -1 otherwise -- and a -1 reaches Sprite's constructor as Images[-1], which faults.
    FastLoadTexture("title/dpad.png");
    FastLoadTexture("title/buttons.png");
    FastLoadTexture("hud/red.png");
    FastLoadTexture("hud/black.png");
    FastLoadTexture("hud/pumpkinblur.png");
    FastLoadTexture("hud/portalfade.png");
    FastLoadTexture("environment/clouds.png");
    FastLoadTexture("gui/inventory1.png");
    FastLoadTexture("gui/inventory_creative.png");
    FastLoadTexture("gui/crafting_table.png");
    FastLoadTexture("gui/chest.png");
    FastLoadTexture("gui/furnace.png");
    FastLoadTexture("gui/cursor.png");
    FastLoadTexture("gui/slider.png");
    FastLoadTexture("gui/fire.png");
	FastLoadTexture("gui/arrow.png");
	FastLoadTexture("gui/cell.png");
	FastLoadTexture("armor/cloth_1.png");
	FastLoadTexture("armor/cloth_2.png");
	FastLoadTexture("armor/chain_1.png");
	FastLoadTexture("armor/chain_2.png");
	FastLoadTexture("armor/iron_1.png");
	FastLoadTexture("armor/iron_2.png");
	FastLoadTexture("armor/diamond_1.png");
	FastLoadTexture("armor/diamond_2.png");
	FastLoadTexture("armor/gold_1.png");
	FastLoadTexture("armor/gold_2.png");
	FastLoadTexture("entity/cow/cow.png");
    FastLoadTexture("particle/smoke.png");
	FastLoadTexture("particle/flame.png");
	FastLoadTexture("particle/note.png");
	FastLoadTexture("particle/bubble.png");
	FastLoadTexture("particle/explosion.png");
	FastLoadTexture("colormap/grass.png");
	FastLoadTexture("item/clock.png");
	FastLoadTexture("item/compass.png");
	FastLoadTexture("environment/sky_color.png");
	FastLoadTexture("environment/rain.png");
	FastLoadTexture("environment/snow.png");
	FastLoadTexture("entity/creeper/creeper.png");
	FastLoadTexture("entity/pig/pig.png");
	FastLoadTexture("entity/sheep/sheep.png");
	FastLoadTexture("entity/sheep/sheep_fur.png");
	FastLoadTexture("entity/chicken/chicken.png");
	FastLoadTexture("entity/spider/spider.png");
	FastLoadTexture("entity/spider/spider_eyes.png");
	FastLoadTexture("title/background.png");
	FastLoadTexture("particle/heart.png");

    ApplySavedSkin();
    TextureManager::Instance()->LogTextureUsage("after boot");
}

int TextureHelper::GetTexture(Textures texture)
{
	// Wrapper that reports a texture the game asks for but no pack provides. GetTextureRaw returns
	// -1 when the file is not in a FastLoadTexture list (or simply is not in the chosen pack), and
	// that -1 used to reach sceGuTexImage through SetTextureModeulate, which indexed Images[-1] --
	// a stale allocator word dereferenced as an Image*. TextureManager now refuses an out-of-range
	// number instead of reading it, so the failure is survivable, but it is also *invisible*: the
	// mob or block just renders with whatever texture was bound last. This line is what makes it
	// visible. Once per texture, so a per-frame caller cannot flood Log.txt.
	int number = GetTextureRaw(texture);

	if(number < 0 && texture >= 0 && texture < 256)
	{
		static bool reported[256] = {false};
		if(reported[texture] == false)
		{
			reported[texture] = true;
			MC_DEBUG_LOG("texture missing: %s (TextureHelper case %d) -- drawing with it is skipped",
			             fileName.c_str(), (int)texture);
		}
	}

	return number;
}

int TextureHelper::GetTextureRaw(Textures texture)
{
	switch(texture)
	{
		/*case ESRB:
		{
			fileName = texturePatch + "title/esrb.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;*/
		case Genesis:
		{
			fileName = texturePatch + "title/mojang_splash_screen.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		/*case YJ:
		{
			fileName = texturePatch + "title/YJ.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;*/
		case Logo:
		{
			fileName = texturePatch + "environment/moon.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Utils:
		{
			fileName = texturePatch + "hud/utils.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Xpbar:
		{
			fileName = texturePatch + "hud/xpbar.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Dirt:
		{
			fileName = texturePatch + "title/dirt.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Terrain1:
		{
			fileName = texturePatch + "terrain.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Terrain2:
		{
			fileName = texturePatch + "terrain_mips.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case WaterAnimation:
		{
			fileName = texturePatch + "water_still.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case LavaAnimation:
		{
			fileName = texturePatch + "lava_still.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case FireAnimation:
		{
			fileName = texturePatch + "fire_0.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case FireAnimation2:
		{
			fileName = texturePatch + "fire_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case PortalAnimation:
		{
			// 16x512, thirty-two 16px frames. Copied into the terrain atlas by UpdateWorldAnimation.
			fileName = texturePatch + "portal.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Items1:
		{
			fileName = texturePatch + "items.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Blue:
		{
			fileName = texturePatch + "hud/blue.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Sun:
		{
			fileName = texturePatch + "environment/sun.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Moon:
		{
			fileName = texturePatch + "environment/moon.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Stars:
		{
			fileName = texturePatch + "environment/stars.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case SnowBall3:
		{
			fileName = texturePatch + "entity/snowball.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case arrowProjectileTexture:
		{
			fileName = texturePatch + "entity/arrow.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case dpadTexture:
		{
			// 32x16: the up cell at (0,0) and the down cell at (16,0). Missing from a texture pack that
			// predates it, in which case the hints are simply not drawn -- the box still scrolls, and
			// the key presses are the same, so this is decoration and not a dependency.
			fileName = texturePatch + "title/dpad.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case buttonIconsTexture:
		{
			// -1 for a pack without the sheet; callers name the button in words instead.
			fileName = texturePatch + "title/buttons.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case zombieTexture:
		{
			fileName = texturePatch + "entity/zombie/zombie.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case skeletonTexture:
		{
			fileName = texturePatch + "entity/skeleton/skeleton.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Steve:
		{
			fileName = texturePatch + "entity/player_skins/steve.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case lameCraft:
		{
			fileName = texturePatch + "title/logo.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Buttons:
		{
			fileName = texturePatch + "title/menu_elements.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case Rectangles:
		{
			fileName = texturePatch + "title/choose_rectangles.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Load:
		{
			fileName = texturePatch + "title/load.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Red:
		{
			fileName = texturePatch + "hud/red.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case Black:
		{
			fileName = texturePatch + "hud/black.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case PumpkinMask:
		{
			fileName = texturePatch + "hud/pumpkinblur.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case PortalFade:
		{
			fileName = texturePatch + "hud/portalfade.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case clouds1:
		{
			fileName = texturePatch + "environment/clouds.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case inv:
		{
			fileName = texturePatch + "gui/inventory1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case invCreative:
		{
			fileName = texturePatch + "gui/inventory_creative.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case crt:
		{
			fileName = texturePatch + "gui/crafting_table.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case cht:
		{
			fileName = texturePatch + "gui/chest.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case fur:
		{
			fileName = texturePatch + "gui/furnace.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case cursor:
		{
			fileName = texturePatch + "gui/cursor.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case invSlider:
		{
			fileName = texturePatch + "gui/slider.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case furFire:
		{
			fileName = texturePatch + "gui/fire.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case furArrow:
		{
			fileName = texturePatch + "gui/arrow.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case invCell:
		{
			fileName = texturePatch + "gui/cell.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case leatherArmor1:
		{
			fileName = texturePatch + "armor/cloth_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case leatherArmor2:
		{
			fileName = texturePatch + "armor/cloth_2.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case chainmailArmor1:
		{
			fileName = texturePatch + "armor/chain_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case chainmailArmor2:
		{
			fileName = texturePatch + "armor/chain_2.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case ironArmor1:
		{
			fileName = texturePatch + "armor/iron_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case ironArmor2:
		{
			fileName = texturePatch + "armor/iron_2.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case diamondArmor1:
		{
			fileName = texturePatch + "armor/diamond_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case diamondArmor2:
		{
			fileName = texturePatch + "armor/diamond_2.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case goldenArmor1:
		{
			fileName = texturePatch + "armor/gold_1.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case goldenArmor2:
		{
			fileName = texturePatch + "armor/gold_2.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case cowTexture:
		{
			fileName = texturePatch + "entity/cow/cow.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case smokeTexture:
		{
			fileName = texturePatch + "particle/smoke.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case flameTexture:
		{
			fileName = texturePatch + "particle/flame.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case noteTexture:
		{
			fileName = texturePatch + "particle/note.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case bubbleTexture:
		{
			fileName = texturePatch + "particle/flame.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case explosionTexture:
		{
			fileName = texturePatch + "particle/explosion.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
        case vegetationColorMap:
		{
			fileName = texturePatch + "colormap/grass.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case clockTexture:
		{
			fileName = texturePatch + "item/clock.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case compassTexture:
		{
			fileName = texturePatch + "item/compass.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case skyColorTexture:
		{
			fileName = texturePatch + "environment/sky_color.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case rainTexture:
		{
			fileName = texturePatch + "environment/rain.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case snowTexture:
		{
			fileName = texturePatch + "environment/snow.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case creeperTexture:
		{
			fileName = texturePatch + "entity/creeper/creeper.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case pigTexture:
		{
			fileName = texturePatch + "entity/pig/pig.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case sheepTexture:
		{
			fileName = texturePatch + "entity/sheep/sheep.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case sheepFurTexture:
		{
			fileName = texturePatch + "entity/sheep/sheep_fur.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case chickenTexture:
		{
			fileName = texturePatch + "entity/chicken/chicken.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case spiderTexture:
		{
			fileName = texturePatch + "entity/spider/spider.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case spiderEyesTexture:
		{
			fileName = texturePatch + "entity/spider/spider_eyes.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case background:
		{
			fileName = texturePatch + "title/background.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		case heartTexture:
		{
			fileName = texturePatch + "particle/heart.png";
			return TextureManager::Instance()->GetTextureNumber(fileName);
		}
		break;
		// ESRB and YJ are still in the Textures enum but their art (title/esrb.png, title/YJ.png) is in
		// no pack, which is why their cases above are commented out. They used to fall through to the
		// "return 0" below the switch, and that was worse than it looks: 0 is a valid texture number, so
		// a request for one of these was indistinguishable from a real texture and drew whatever happened
		// to be bound last. Answering -1 here is what GetTexture's missing-texture report keys on, so the
		// request becomes visible (once) instead of silent.
		default:
			return -1;
	}
	return 0;
}

void TextureHelper::SetTexturePack(std::string name)
{
    TextureManager::Instance()->RemoveTextures();

	SetTextureFolderName(name+"/");

    FastLoadTexture("environment/moon.png");
    FastLoadTexture("hud/utils.png");
	FastLoadTexture("hud/xpbar.png");
    FastLoadTexture("title/dirt.png");
    FastLoadTexture("terrain.png");
    FastLoadTexture("terrain_mips.png");
    FastLoadTexture("water_still.png");
    FastLoadTexture("lava_still.png"); 
	FastLoadTexture("fire_0.png");
	FastLoadTexture("fire_1.png");
    // The nether portal strip. It has to be in BOTH texture lists -- Init's and SetTexturePack's --
    // because GetTextureNumber resolves a name through whatever list was last loaded and returns -1
    // otherwise. Without this the portal renders as a missing texture however present the file is,
    // which is exactly what the log reported, and it would break again on every pack switch.
    FastLoadTexture("portal.png");
    FastLoadTexture("items.png");
    FastLoadTexture("hud/blue.png");
    FastLoadTexture("environment/sun.png");
    FastLoadTexture("environment/moon.png");
    FastLoadTexture("environment/stars.png");
    FastLoadTexture("entity/snowball.png");
    FastLoadTexture("entity/arrow.png");
    FastLoadTexture("entity/zombie/zombie.png");
    FastLoadTexture("entity/skeleton/skeleton.png");
    FastLoadTexture("entity/player_skins/steve.png");
    FastLoadTexture("title/logo.png");
    FastLoadTexture("title/choose_rectangles.png");
    FastLoadTexture("title/menu_elements.png");
    FastLoadTexture("title/load.png");
    // The News & Converter changelog's d-pad hints. GetTextureRaw can only return a texture that has
    // been registered here -- TextureManager::GetTextureNumber looks the name up in this list and
    // returns -1 otherwise -- and a -1 reaches Sprite's constructor as Images[-1], which faults.
    FastLoadTexture("title/dpad.png");
    FastLoadTexture("title/buttons.png");
    FastLoadTexture("hud/red.png");
    FastLoadTexture("hud/black.png");
    FastLoadTexture("hud/pumpkinblur.png");
    FastLoadTexture("hud/portalfade.png");
    FastLoadTexture("environment/clouds.png");
    FastLoadTexture("gui/inventory1.png");
    FastLoadTexture("gui/inventory_creative.png");
    FastLoadTexture("gui/crafting_table.png");
    FastLoadTexture("gui/chest.png");
    FastLoadTexture("gui/furnace.png");
    FastLoadTexture("gui/cursor.png");
    FastLoadTexture("gui/slider.png");
    FastLoadTexture("gui/fire.png");
	FastLoadTexture("gui/arrow.png");
	FastLoadTexture("gui/cell.png");
	FastLoadTexture("armor/cloth_1.png");
	FastLoadTexture("armor/cloth_2.png");
	FastLoadTexture("armor/chain_1.png");
	FastLoadTexture("armor/chain_2.png");
	FastLoadTexture("armor/iron_1.png");
	FastLoadTexture("armor/iron_2.png");
	FastLoadTexture("armor/diamond_1.png");
	FastLoadTexture("armor/diamond_2.png");
	FastLoadTexture("armor/gold_1.png");
	FastLoadTexture("armor/gold_2.png");
	FastLoadTexture("entity/cow/cow.png");
	FastLoadTexture("particle/smoke.png");
	FastLoadTexture("particle/flame.png");
	FastLoadTexture("particle/note.png");
	FastLoadTexture("particle/bubble.png");
	FastLoadTexture("particle/explosion.png");
	FastLoadTexture("colormap/grass.png");
	FastLoadTexture("item/clock.png");
	FastLoadTexture("item/compass.png");
    FastLoadTexture("environment/sky_color.png");
    FastLoadTexture("environment/rain.png");
    FastLoadTexture("environment/snow.png");
    FastLoadTexture("entity/creeper/creeper.png");
	FastLoadTexture("entity/pig/pig.png");
    FastLoadTexture("entity/sheep/sheep.png");
    FastLoadTexture("entity/sheep/sheep_fur.png");
    FastLoadTexture("entity/chicken/chicken.png");
    FastLoadTexture("entity/spider/spider.png");
    FastLoadTexture("entity/spider/spider_eyes.png");
    FastLoadTexture("title/background.png");
    FastLoadTexture("particle/heart.png");
    ApplySavedSkin();
}

void TextureHelper::FastRemoveTexture(std::string name)
{

}

void TextureHelper::RemoveConstTextures()
{
    TextureManager::Instance()->RemoveConstTextures();
}

// HOW MANY BYTES A TEXTURE IS ALLOWED TO TAKE
//
// Every texture the game loads is kept in the heap as swizzled pixels, and the loader's default is
// GU_PSM_8888 -- four bytes per texel. Two bytes is enough for most of this game's art, and the
// difference is not small: the ~80 textures read at boot occupy about 7.5 MB at four bytes per
// texel, which is roughly 40% of the partition the game is given. That partition is exactly what
// world entry runs out of.
//
// So the big, fully opaque UI and menu art is stored at two bytes per texel instead. The list below
// is deliberately short, and a texture is on it only if BOTH of these hold:
//
//   1. Nothing reads or writes it pixel by pixel. TextureManager's getRed/getGreen/getBlue/
//      getAlpha/setColour/getPixelRaw/setPixelRaw index a texel as two 16-bit halves and are
//      written for 8888 alone. The animated tiles (terrain, terrain_mips, water, lava, fire,
//      portal), the clock/compass frame copies into items.png, the sky-colour sampling and the
//      alpha test the block and item meshes build from all go through them, so every one of those
//      textures stays 8888. Handing a two-byte image to one of those would write into the
//      neighbouring texel -- and one texel past the end of the buffer, for the last pixel.
//   2. Fully opaque, so 5551's single alpha bit costs nothing. Anything with a soft edge is left
//      alone: clouds, sun, moon, rain, snow, the particles, and the screen overlays.
//
// The saving is about 2.4 MB and it is almost all in menus (three 480x272-or-so rasters each
// rounding up to 512x512 and costing a full megabyte apiece), so a mistake here shows up as hard
// edges or banding on a menu rather than as something subtle. Nothing else about the texture
// changes: SetTexture passes image->ColorMode to sceGuTexMode, so the GE is told the format the
// loader actually used.
static bool NameEndsWith(const std::string& name, const char* suffix)
{
    const size_t n = strlen(suffix);
    if(name.size() < n)
        return false;
    return name.compare(name.size() - n, n, suffix) == 0;
}

static int TextureStorageFormat(const std::string& name)
{
    // These are the names FastLoadTexture is called with -- pack-relative paths with no leading
    // slash, so a suffix of "/title/background.png" would never match "title/background.png" (it is
    // one character too long and the slash is not there). An earlier version of this table was
    // written that way and matched nothing at all, which is worth remembering: the symptom was not
    // a broken texture, it was a change that silently did nothing.
    static const char* const kOpaque16[] =
    {
        "title/background.png",            // 512x289 -> 512x512: 1.0 MB -> 0.5 MB
        "title/mojang_splash_screen.png",  // 480x272 -> 512x512: 1.0 MB -> 0.5 MB
        "hud/xpbar.png",                   // 480x272 -> 512x512: 1.0 MB -> 0.5 MB
        "title/dirt.png",
        "title/choose_rectangles.png",
        "gui/inventory1.png",
        "gui/inventory_creative.png",
        "gui/furnace.png",
        "gui/crafting_table.png",
        "gui/chest.png",

        // Removed again, and this is what the rule above is for -- both were on the list and both
        // broke the menus on a real screen.
        //
        // "title/menu_elements.png" is the button sheet, and it holds the changelog panel cell at
        // (87,60), an 8x8 black square at alpha 105 that the menu scales to 60x22 as a translucent
        // backdrop. ONE alpha bit cannot express 41%: with the old conversion it came out solid
        // black, and once the conversion above is corrected it comes out invisible instead. There is
        // no threshold that is right, because the texture does not have hard alpha. Its button faces
        // also carry a smoothed bevel -- greys 127/129/124/123/131/127/129/128 down one column -- and
        // 5-bit colour flattened that to an alternating 123/132, which bands at the menu's 2x scale.
        // 32 KB.
        //
        // "title/logo.png" is 701 antialiased pixels spanning alpha 1..255, the MINECRAFT wordmark on
        // the title screen. A single bit turns a soft edge into a cut one; it is the largest artwork
        // the player looks at and it is 128 KB. Both together cost 160 KB of the ~2.4 MB this table
        // saves, and the rest of the list is unaffected: background/xpbar/splash are large and their
        // soft pixels are a one-pixel border or a near-opaque edge (see the notes on each), and the
        // gui/ sheets and choose_rectangles.png carry hard alpha only.
    };

    for(size_t i = 0; i < sizeof(kOpaque16) / sizeof(kOpaque16[0]); i++)
    {
        if(NameEndsWith(name, kOpaque16[i]))
            return GU_PSM_5551;
    }
    return GU_PSM_8888;
}

// ---------------------------------------------------------------------------
// The title-screen art is the only part of the boot texture set that no frame inside a world ever
// samples, so it is the cheapest memory in the game to give back.
//
// The list is SHORT and it is short for a reason: most of the menu art is not menu-only. `Buttons`
// (title/menu_elements.png) is rebuilt as the buttons of the in-world pause menu by StatePlay and
// StatePlayCreative, `Dirt` is the loading screen's backdrop and StateUpdate's, and `Load` is the
// loading bar in both. Freeing any of those when a world starts would leave the pause menu and the
// loading screen blank, which is a worse bug than the one this is fixing. Only these four are
// reached from StateMenu alone (verified by grepping every use of the enum, not the file name):
//
//   title/background.png        StateMenu's backgroundSprite, and nothing else      ~512 KB
//   title/logo.png              StateMenu's logoSprite, and nothing else            ~256 KB
//   title/choose_rectangles.png StateMenu's selection rectangles, and nothing else   ~64 KB
//   title/mojang_splash_screen.png  boot only; Enum `Genesis` is not referenced by any frame in
//                               Scripts at all, so this is also the one that is never reloaded  ~512 KB
//
// ~1.34 MB, against a nether that was running out of heap by about that much (1,519 KB free at world
// entry, versus the overworld's 4,621 KB for the same 36 columns).
//
// ReleaseTextureData frees the pixels and KEEPS the slot, which is what makes this safe to do under
// Sprite objects that already cached the number: a released texture resolves to its number but draws
// nothing (SafeImage treats a null ImageData as absent), and the number never moves. That matters
// because StateMenu builds its sprites once at boot and they live for the whole session -- a world is
// Pushed over the menu rather than replacing it, so the menu is paused, not destroyed. The reload
// therefore belongs in StateMenu::Resume (the way back in), not StateMenu::Init.
// ---------------------------------------------------------------------------
static const char* const kMenuOnlyTextures[] =
{
    "title/background.png",
    "title/logo.png",
    "title/choose_rectangles.png",
    "title/mojang_splash_screen.png",   // boot only: released and never reloaded
};
static const int kMenuOnlyTextureCount = (int)(sizeof(kMenuOnlyTextures) / sizeof(kMenuOnlyTextures[0]));

void TextureHelper::ReleaseMenuTextures()
{
    TextureManager* tm = TextureManager::Instance();
    for(int i = 0; i < kMenuOnlyTextureCount; i++)
    {
        int number = tm->GetTextureNumber(texturePatch + kMenuOnlyTextures[i]);
        if(number >= 0)
        {
            tm->ReleaseTextureData(number);
        }
        else
        {
            // Says so rather than silently doing nothing, which is the defect the storage-format
            // table's own history is a warning about: a name that matches nothing looks exactly
            // like a release that worked, until the memory is measured.
            MC_TraceStr("menu art not resident, release was a no-op", kMenuOnlyTextures[i]);
        }
    }
    tm->LogTextureUsage("while a world runs");
}

void TextureHelper::EnsureMenuTextures()
{
    TextureManager* tm = TextureManager::Instance();
    int reloaded = 0;
    int stillMissing = 0;
    for(int i = 0; i < kMenuOnlyTextureCount; i++)
    {
        if(strcmp(kMenuOnlyTextures[i], "title/mojang_splash_screen.png") == 0)
            continue;   // boot only
        int number = tm->GetTextureNumber(texturePatch + kMenuOnlyTextures[i]);
        if(number < 0)
        {
            continue;
        }
        if(tm->TextureDataLoaded(number))
        {
            reloaded++;             // never released in the first place, so nothing to do
            continue;
        }
        tm->ReloadTextureData(number, TextureStorageFormat(kMenuOnlyTextures[i]));
        if(tm->TextureDataLoaded(number))
        {
            reloaded++;
        }
        else
        {
            // The menu will draw a hole where this one belongs. Reported rather than passed over:
            // this is the only symptom when a reload cannot allocate, and the launcher log is the
            // only place it can be seen on hardware.
            stillMissing++;
            Aurora::Utils::Logger::Instance()->LogMessage("menu art could not be reloaded: %s\n",
                kMenuOnlyTextures[i]);
        }
    }
    tm->LogTextureUsage("back at the menu");
    if(stillMissing > 0)
    {
        Aurora::Utils::Logger::Instance()->LogMessage("menu art reload: %d of %d resident, %d missing\n",
            reloaded, kMenuOnlyTextureCount - 1, stillMissing);
    }
}

void TextureHelper::FastLoadTexture(std::string name)
{
    std::string endName = texturePatch+name;

    // Name the asset before reading it, and say so if it is not there at all.
    //
    // The order matters: recording it first makes the last line of Log.txt name the file that did not
    // finish loading, which is what a console that stops during startup has to tell us. This sits here
    // rather than in each caller so that one place covers every texture the game loads, including the
    // seventy-odd that TextureHelper::Init reads in a row.
    //
    // The existence check is the other half. The emulator reads the development folder, where a missing
    // asset is invisible; a real console reads whatever was actually copied to the card, where that same
    // asset is simply absent. If the two disagree about whether the game starts, this is one of the
    // first things that should show up in the log.
    BootTrace::Stage(endName.c_str());
    if(fileExists(endName) == false)
    {
        MC_DEBUG_LOG("texture MISSING: %s\n", endName.c_str());
        MC_TraceStr("tex: MISSING", endName.c_str());
    }

    TextureManager::Instance()->LoadTexture(endName, TextureStorageFormat(name));

    // The texture number it landed on, keyed by the path, so the trace says both which file was read
    // and what came back. A negative number here is a load that quietly produced nothing, which is
    // the failure that later shows up as a blank or wrongly-textured face rather than as a crash.
    MC_TraceInt(endName.c_str(), TextureManager::Instance()->GetTextureNumber(endName));
}

void TextureHelper::SetTextureFolderName(std::string name)
{
	defaultFolder = name;
	texturePatch = defaulPatch + defaultFolder;
	SaveTexturePackPath();
}

void TextureHelper::SaveTexturePackPath()
{
	unsigned int pathLength = defaultFolder.size();

	FILE * pFile;
	pFile = fopen("Config/texturepack.cf","wb");

	if(pFile == NULL)
		return;

    fwrite(&pathLength,sizeof(unsigned int),1,pFile);
	fwrite(defaultFolder.c_str(),1,pathLength,pFile);

	//close file
	fclose(pFile);
}

void TextureHelper::LoadTexturePackPath()
{
	unsigned int pathLength = 0;

	MC_Trace("tex: LoadTexturePackPath enter");

    FILE * pFile;
	pFile = fopen("Config/texturepack.cf","rb");

    if(pFile == NULL)
    {
        MC_Trace("tex: Config/texturepack.cf absent, using Minecraft/");
        defaultFolder = "Minecraft/";
        return;
    }

    MC_Trace("tex: Config/texturepack.cf opened");

    fread(&pathLength,sizeof(unsigned int),1,pFile);
    MC_TraceUInt("tex: stored pack path length", pathLength);

	// A length read straight off the card is not a length that can be trusted.
	//
	// This was new char[pathLength+1] with no check at all, so a truncated, empty, or foreign-format
	// file -- anything whose first four bytes are not the length some earlier build wrote -- asked the
	// allocator for up to about 4 GB. On a console that is a hard failure with nothing logged, and it
	// is invisible in the emulator for two independent reasons: the file is usually absent there, so
	// the early return above hides the whole path, and the emulator's allocator will not fault the
	// same way one that has to fit in 24 MB does.
	//
	// The bound is deliberately loose. Real pack folder names are a few dozen characters, so anything
	// past a few hundred is corruption rather than a pack, and tightening it further would break a
	// legitimate pack to guard against a case that cannot occur.
	const unsigned int kMaxTexturePackPath = 512;
	if(pathLength > kMaxTexturePackPath)
	{
		MC_Trace("tex: ERROR implausible path length, refusing it and using Minecraft/");
		fclose(pFile);
		defaultFolder = "Minecraft/";
		return;
	}

	char* c = new char[pathLength+1];
    size_t got = fread(c,1,pathLength,pFile);
	// Terminated at the bytes actually read rather than at pathLength: a short file would otherwise
	// leave uninitialised heap in the string.
    c[got] = '\0';
    MC_TraceStr("tex: pack folder from config", c);
    defaultFolder = c;
    delete[] c;

	//close file
	fclose(pFile);

	MC_TraceStr("tex: pack folder chosen", defaultFolder.c_str());

	if(fileExists(defaulPatch+defaultFolder+"terrain.png") == false) // if this texture pack doesn't exist at the moment
	{
		MC_Trace("tex: pack has no terrain.png, falling back to Minecraft/");
        defaultFolder = "Minecraft/"; // return to default one
	}

	MC_TraceStr("tex: final pack folder", defaultFolder.c_str());
}

static bool SkinNameSafe(const char* name)
{
    if (name == NULL || name[0] == 0)
        return false;
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL)
        return false;
    if (strstr(name, "..") != NULL)
        return false;
    return true;
}

Aurora::Graphics::Image* TextureHelper::LoadPlayerSkinImage(const char* name, int swizzle)
{
    bool steve = (name == NULL || name[0] == 0 || strcmp(name, "Steve") == 0);
    std::string path;
    if (steve)
        path = texturePatch + "entity/player_skins/steve.png";
    else
    {
        if (SkinNameSafe(name) == false)
            return NULL;
        path = std::string("Skins/") + name;
    }

    Aurora::Graphics::Image* fresh = TextureManager::Instance()->LoadPng(path.c_str(), GU_PSM_8888, swizzle, 0, 1);
    if (fresh == NULL || fresh->Width != 64 || fresh->Height != 32)
    {
        if (fresh != NULL)
        {
            if (fresh->ImageData != NULL && fresh->vRam == 0)
                free(fresh->ImageData);
            delete fresh;
        }
        return NULL;
    }
    return fresh;
}

bool TextureHelper::InstallPlayerSkin(const char* name)
{
    int slot = GetTexture(Steve);
    if (slot < 0)
        return false;

    bool steve = (name == NULL || name[0] == 0 || strcmp(name, "Steve") == 0);
    Aurora::Graphics::Image* fresh = LoadPlayerSkinImage(steve ? "Steve" : name, 1);
    if (fresh == NULL)
    {
        Aurora::Utils::Logger::Instance()->LogMessage("player skin: could not use %s\n", name != NULL ? name : "Steve");
        return false;
    }

    TextureManager::Instance()->ReplaceImage(slot, fresh);
    playerSkinFile = steve ? "Steve" : name;
    Aurora::Utils::Logger::Instance()->LogMessage("player skin: %s\n", playerSkinFile.c_str());
    return true;
}

void TextureHelper::ApplySavedSkin()
{
    char raw[80];
    raw[0] = 0;
    FILE* f = fopen("Config/skin.txt", "rb");
    if (f != NULL)
    {
        if (fgets(raw, (int)sizeof(raw), f) == NULL)
            raw[0] = 0;
        fclose(f);
    }
    for (int i = 0; raw[i] != 0; i++)
    {
        if (raw[i] == '\n' || raw[i] == '\r')
            raw[i] = 0;
    }
    if (raw[0] == 0)
    {
        playerSkinFile = "Steve";
        return;
    }
    if (InstallPlayerSkin(raw) == false)
        playerSkinFile = "Steve";
}

bool TextureHelper::SetPlayerSkin(const char* name)
{
    if (name == NULL)
        name = "Steve";
    if (InstallPlayerSkin(name) == false)
        return false;

    FILE* f = fopen("Config/skin.txt", "wb");
    if (f != NULL)
    {
        fputs(playerSkinFile.c_str(), f);
        fclose(f);
    }
    return true;
}

const char* TextureHelper::PlayerSkinName() const
{
    if (playerSkinFile.size() == 0)
        return "Steve";
    return playerSkinFile.c_str();
}

inline bool TextureHelper::fileExists (const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}


TextureHelper TextureHelper::m_TextureHelper;

TextureHelper* TextureHelper::Instance()
{
	return &m_TextureHelper;
}
