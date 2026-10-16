#ifndef LAMEFUNCTIONS_H
#define LAMEFUNCTIONS_H

#define PI 3.1415926535897f

#include <stdio.h>
#include <string>
#include <math.h>

#include <psptypes.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <Aurora/Math/BoundingBox.h>

inline float ATAN2(float dx, float dz)
{
    if(dx == 0 && dz == 0)
    {
        return 0.0f;
    }
    return atan2f(dx,dz);
}

#define PI_FLOAT     3.14159265f
#define PIBY2_FLOAT  1.5707963f
inline float ATAN2_fast(float y, float x)
{
	if ( x == 0.0f )
	{
		if ( y > 0.0f ) return PIBY2_FLOAT;
		if ( y == 0.0f ) return 0.0f;
		return -PIBY2_FLOAT;
	}
	float atan;
	float z = y/x;
	if ( fabs( z ) < 1.0f )
	{
		atan = z/(1.0f + 0.28f*z*z);
		if ( x < 0.0f )
		{
			if ( y < 0.0f ) return atan - PI_FLOAT;
			return atan + PI_FLOAT;
		}
	}
	else
	{
		atan = PIBY2_FLOAT - z/(z*z + 0.28f);
		if ( y < 0.0f ) return atan - PI_FLOAT;
	}
	return atan;
}

inline void MatrixAngle(Vector3 angle)
{
    sceGumRotateX(angle.x/180*PI);
    sceGumRotateY(angle.y/180*PI);
    sceGumRotateZ(angle.z/180*PI);
}

inline void MatrixRadianAngle(Vector3 angle)
{
    sceGumRotateX(angle.x);
    sceGumRotateY(angle.y);
    sceGumRotateZ(angle.z);
}

inline void MatrixScale(Vector3 scale)
{
    ScePspFVector3 sca = {scale.x,scale.y,scale.z};
    sceGumScale(&sca);
}

inline void MatrixTranslation(Vector3 pos)
{
    ScePspFVector3 loc = {pos.x,pos.y,pos.z};
    sceGumTranslate(&loc);
}

inline void MatrixPush()
{
    sceGumPushMatrix();
}

inline void MatrixPop()
{
    sceGumPopMatrix();
}

inline void MatrixColor(int color)
{
    sceGuColor(color);
}

inline void DrawSetInverseColors()
{
    sceGuBlendFunc(GU_ABS, GU_ONE_MINUS_SRC_ALPHA, GU_SRC_ALPHA, 0, 0);
}

inline void DrawSetDefaultColors()
{
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

inline void DrawSetCulling(bool enable)
{
    if(enable)
    {
        sceGuFrontFace(GU_CW);
    }
    else
    {
        sceGuFrontFace(GU_CCW);
    }
}

inline void DrawSetBlend(bool enable)
{
    if(enable)
    {
        sceGuEnable(GU_BLEND);
    }
    else
    {
        sceGuDisable(GU_BLEND);
    }
}

inline void DrawSetDepthTest(bool enable)
{
    if(enable)
    {
        sceGuEnable(GU_DEPTH_TEST);
    }
    else
    {
        sceGuDisable(GU_DEPTH_TEST);
    }
}

inline void DrawSetDepthMask(bool enable)
{
    sceGuDepthMask(enable);
}

inline void DrawSetAlphaTest(bool enable)
{
    if(enable)
    {
        sceGuEnable(GU_ALPHA_TEST);
    }
    else
    {
        sceGuDisable(GU_ALPHA_TEST);
    }
}

inline void DrawPlaceTexture(bool enable)
{
    if(enable)
    {
        sceGuEnable(GU_TEXTURE_2D);
    }
    else
    {
        sceGuDisable(GU_TEXTURE_2D);
    }
}

inline float GuiTexX(float texW, float scale, float texX)
{
    return 240.0f - texW * scale * 0.5f + texX * scale;
}

inline float GuiTexY(float texH, float scale, float texY)
{
    return 136.0f - texH * scale * 0.5f + texY * scale;
}

// Sprite::SetPosition is the sprite center, not its top-left.
inline float GuiSpriteX(float texW, float scale, float texLeft, float spriteW)
{
    return GuiTexX(texW, scale, texLeft + spriteW * 0.5f);
}

inline float GuiSpriteY(float texH, float scale, float texTop, float spriteH)
{
    return GuiTexY(texH, scale, texTop + spriteH * 0.5f);
}

inline float GuiSlotX(float texW, float scale, float slotLeft)
{
    return GuiTexX(texW, scale, slotLeft + 9.0f);
}

inline float GuiSlotY(float texH, float scale, float slotTop)
{
    return GuiTexY(texH, scale, slotTop + 9.0f);
}

inline float GuiPlayerInvSlotY(float texH, float scale, int row)
{
    float slotTop = (row < 3) ? (60.0f + row * 18.0f) : 116.0f;
    return GuiSlotY(texH, scale, slotTop);
}

// Hotbar icons use the exact slot center, same as the catalog/inventory rows.
inline float GuiPlayerInvItemY(float texH, float scale, int row)
{
    return GuiPlayerInvSlotY(texH, scale, row);
}

inline float GuiCreativeHotbarItemY(float texH, float scale)
{
    return GuiSlotY(texH, scale, 111.0f);
}

// Java 4-row thin-header creative sheet (inventory_creative.png 195x136 blit).
static const float kCreativeGuiW = 195.0f;
static const float kCreativeGuiH = 136.0f;
static const float kCreativeGuiScale = 2.0f;
static const int kCreativeTabCount = 5;
static const int kCreativeCatRows = 4;
static const int kCreativeHotbarRow = 4;
static const float kCreativeSlotX0 = 8.0f;
static const float kCreativeCatY0 = 35.0f;
static const float kCreativeHotbarY = 111.0f;
static const float kCreativeTabW = 26.0f;
static const float kCreativeTabH = 32.0f;
static const float kCreativeSliderX = 175.0f;
static const float kCreativeSliderY = 36.0f;
static const float kCreativeSliderH = 70.0f;
static const float kCreativeSliderNubW = 12.0f;
static const float kCreativeSliderNubH = 15.0f;

inline float GuiCreativeSlotX(int col)
{
    return GuiSlotX(kCreativeGuiW, kCreativeGuiScale, kCreativeSlotX0 + col * 18.0f);
}

inline float GuiCreativeSlotY(int row)
{
    float slotTop = (row < kCreativeCatRows) ? (kCreativeCatY0 + row * 18.0f) : kCreativeHotbarY;
    return GuiSlotY(kCreativeGuiH, kCreativeGuiScale, slotTop);
}

inline float GuiCreativeItemY(int row)
{
    return GuiCreativeSlotY(row);
}

inline float GuiAmtX(float slotX) { return slotX + 10.0f; }
inline float GuiAmtY(float slotY) { return slotY + 12.0f; }
inline float GuiDurX(float slotX) { return slotX - 12.0f; }
inline float GuiDurY(float slotY) { return slotY + 10.0f; }

// Placement of the item-name hover label the inventory panels draw under the cursor. Unlike the
// other Gui* helpers there is no panel geometry to work from -- the caller passes the slot centre it
// already has and the label's measured width. Centring the label on the slot and then pulling it
// inside the display keeps a long name ("Spawn Zombie", "Cobblestone") from running off the edge.
//
// The width is measured rather than assumed (RenderManager::GetTextWidth) because the estimate is
// easy to get wrong and the failure is silent. For reference, _tmp_atlas/preview_invgui.py prints
// where each slot actually lands: the survival inventory's leftmost column centres on x 97 and its
// rightmost on 383, so with a ~65 px label nothing clamps at all -- the guard is there for the long
// ones ("Spawn Skeleton", "Wooden Pressure Plate", localised names) and for the creative catalog,
// whose rightmost column sits further out.
//
// There is no free margin to put the label in: every panel is a 176x138 sheet at scale 350/176, so it
// fills 350x274 of the 480x272 display. The label is therefore an overlay on the panel and leans on
// its drop shadow, and the only real decision is which way to go when the slot is near the bottom.
inline float GuiHoverLabelX(float slotX, float textWidth)
{
    const float half = textWidth * 0.5f + 4.0f;   // 4 px so the drop shadow is not clipped either
    if(slotX < half) return half;
    if(slotX > 480.0f - half) return 480.0f - half;
    return slotX;
}

// Below the slot by default. The bottom inventory row sits at y 247 (GuiPlayerInvSlotY row 3), where
// "below" would put the label off the 272-px edge, so it flips above instead -- over the row above,
// which the shadow handles.
inline float GuiHoverLabelY(float slotY)
{
    return (slotY > 240.0f) ? (slotY - 30.0f) : (slotY + 14.0f);
}

inline BoundingBox MobAttackBox(BoundingBox box)
{
    box.min.x -= 0.40f;
    box.min.y -= 0.20f;
    box.min.z -= 0.40f;
    box.max.x += 0.40f;
    box.max.y += 0.45f;
    box.max.z += 0.40f;
    return box;
}

inline bool RayIntersectAABB(const Vector3& origin, const Vector3& dir, const BoundingBox& box, float maxDist, float* hitT)
{
    float tmin = 0.0f;
    float tmax = maxDist;

    for(int axis = 0; axis < 3; axis++)
    {
        float o, d, bmin, bmax;
        if(axis == 0) { o = origin.x; d = dir.x; bmin = box.min.x; bmax = box.max.x; }
        else if(axis == 1) { o = origin.y; d = dir.y; bmin = box.min.y; bmax = box.max.y; }
        else { o = origin.z; d = dir.z; bmin = box.min.z; bmax = box.max.z; }

        if(fabsf(d) < 0.000001f)
        {
            if(o < bmin || o > bmax)
                return false;
        }
        else
        {
            float inv = 1.0f / d;
            float t1 = (bmin - o) * inv;
            float t2 = (bmax - o) * inv;
            if(t1 > t2)
            {
                float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if(t1 > tmin) tmin = t1;
            if(t2 < tmax) tmax = t2;
            if(tmin > tmax)
                return false;
        }
    }

    if(tmax < 0.0f)
        return false;
    if(hitT != NULL)
        *hitT = tmin;
    return true;
}

#endif
