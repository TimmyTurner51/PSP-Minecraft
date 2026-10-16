#ifndef TEXTUREHELPER_H_
#define TEXTUREHELPER_H_

#include <string>
#include <Aurora/Graphics/TextureManager.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

class TextureHelper
{
private:

	std::string defaulPatch;
	std::string fileName;

public:

	enum Textures
	{
		 ESRB,
         Genesis,
		 YJ,
		 Logo,
		 Utils,
		 Xpbar,
		 Dirt,
		 Terrain1,
		 Terrain2,
		 WaterAnimation,
		 LavaAnimation,
		 FireAnimation,
		 FireAnimation2,
		 // The nether portal's 32-frame swirl. Animated the same way water and lava are: a separate
		 // strip whose current frame is copied into its terrain-atlas tile each animation tick.
		 PortalAnimation,
		 // A single purple square, stretched over the screen while the player is being pulled through a
		 // portal. Its own file rather than a tint on Black because Sprite has no working colour tint
		 // (Sprite::RGB is still an empty stub) and a black texture modulated by anything stays black.
		 PortalFade,
         Items1,
		 Blue,
		 Sun,
         Moon,
         Stars,
         SnowBall3,
         zombieTexture,
         Steve,
		 lameCraft,
		 Buttons,
		 Rectangles,
		 Load,
		 Red,
		 Black,
		 PumpkinMask,
         clouds1,
         inv,
         invCreative,
         crt,
         cht,
         fur,
         cursor,
         invSlider,
         furFire,
         furArrow,
         invCell,
         leatherArmor1,
         leatherArmor2,
         chainmailArmor1,
         chainmailArmor2,
         ironArmor1,
         ironArmor2,
         diamondArmor1,
         diamondArmor2,
         goldenArmor1,
         goldenArmor2,
         cowTexture,
         smokeTexture,
         flameTexture,
         noteTexture,
         bubbleTexture,
         explosionTexture,
         vegetationColorMap,
         clockTexture,
         compassTexture,
         skyColorTexture,
         rainTexture,
         snowTexture,
         creeperTexture,
		 pigTexture,
         sheepTexture,
         sheepFurTexture,
         chickenTexture,
         spiderTexture,
         spiderEyesTexture,
         // The skeleton's own skin. 64x32 like the zombie's, because ModelSkeleton inherits the biped
         // head and body from ModelBiped and only overrides the four thin limb boxes.
         skeletonTexture,
         background,
         villagerFarmerTexture,
         villagerLibrarianTexture,
         villagerPriestTexture,
         villagerSmithTexture,
         villagerButcherTexture,
         heartTexture,
         // The arrow projectile. Vanilla's own entity/arrow.png (32x32), the same art the mob
         // importer stages for the other entities, so the shot in flight matches the item icon's art.
         arrowProjectileTexture,
         // The d-pad hint drawn in the News & Converter changelog box: one atlas, two 16x16 cells,
         // up then down. Its own file rather than a cell in menu_elements.png because that atlas is
         // full and packed by hand, and a new cell there would have to be carved out of the button
         // art in every texture pack.
         dpadTexture,
         // The launcher's button prompt sheet, shared with it: six 16x16 cells, triangle, circle,
         // cross, square, L, R. Written into every pack by tools/make-button-icons.py.
         buttonIconsTexture,
	};
    std::string texturePatch;
    std::string defaultFolder;

	static TextureHelper m_TextureHelper;
	static TextureHelper* Instance();


	TextureHelper();
	~TextureHelper();

	int GetTexture(Textures texture);
    void Init();
    void FastRemoveTexture(std::string name);
    void FastLoadTexture(std::string name);

    // The title-screen art (and the splash) is the only part of the boot texture set that no frame
    // inside a world samples. ReleaseMenuTextures frees it when a world is entered and
    // EnsureMenuTextures reloads it when the menu comes back, which is worth roughly 1.4 MB on a
    // device where the nether was running out of the heap. See the notes in the .cpp.
    void ReleaseMenuTextures();
    void EnsureMenuTextures();
	void SetTextureFolderName(std::string name);
	void SetTexturePack(std::string name);

    void RemoveConstTextures();

    void LoadTexturePackPath();
    void SaveTexturePackPath();
    inline bool fileExists(const std::string& name);

    // The title menu's Skins list writes Config/skin.txt. "Steve" is the built-in sheet.
    // Anything else is a file name inside Skins/, loaded into the same slot the player model
    // already samples, so the hand, the inventory and third person all follow it.
    void ApplySavedSkin();
    bool SetPlayerSkin(const char* name);
    const char* PlayerSkinName() const;
    // A skin sheet the caller owns. Swizzle 1 is for the GE; 0 leaves the pixels in rows so a
    // preview can read the face. NULL if the file is missing or not a 64x32 / 64x64 skin.
    Aurora::Graphics::Image* LoadPlayerSkinImage(const char* name, int swizzle);

private:
    // The switch itself. GetTexture wraps it so a miss can be reported (see the .cpp).
    int GetTextureRaw(Textures texture);
    bool InstallPlayerSkin(const char* name);
    std::string playerSkinFile;
};

#endif
