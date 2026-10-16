package net.minecraft.client.model;

// ModelQuadruped + ModelPig, flattened into one file for the extractor.
//
// ModelPig is a 20 line class: it calls super(6, scale) and then adds the snout. Rather than make
// the extractor resolve `super` through a second file (and a class hierarchy that also needs
// ModelBase), the quadruped body is written out here with its two constructor arguments inlined:
// par1 (legHeight) = 6 and par2 (scale) = 0.0F. Every addBox and setRotationPoint is verbatim.
//
// This file is NOT compiled. It exists only as input to tools/mcjava_to_json.py.

import net.minecraft.entity.Entity;

public class ModelPig extends ModelBase
{
    public ModelRenderer head;
    public ModelRenderer body;
    public ModelRenderer leg1;
    public ModelRenderer leg2;
    public ModelRenderer leg3;
    public ModelRenderer leg4;

    public ModelPig()
    {
        int par1 = 6;
        float par2 = 0.0F;

        this.head = new ModelRenderer(this, 0, 0);
        this.head.addBox(-4.0F, -4.0F, -8.0F, 8, 8, 8, par2);
        this.head.setRotationPoint(0.0F, (float)(18 - par1), -6.0F);

        this.body = new ModelRenderer(this, 28, 8);
        this.body.addBox(-5.0F, -10.0F, -7.0F, 10, 16, 8, par2);
        this.body.setRotationPoint(0.0F, (float)(17 - par1), 2.0F);

        this.leg1 = new ModelRenderer(this, 0, 16);
        this.leg1.addBox(-2.0F, 0.0F, -2.0F, 4, par1, 4, par2);
        this.leg1.setRotationPoint(-3.0F, (float)(24 - par1), 7.0F);

        this.leg2 = new ModelRenderer(this, 0, 16);
        this.leg2.addBox(-2.0F, 0.0F, -2.0F, 4, par1, 4, par2);
        this.leg2.setRotationPoint(3.0F, (float)(24 - par1), 7.0F);

        this.leg3 = new ModelRenderer(this, 0, 16);
        this.leg3.addBox(-2.0F, 0.0F, -2.0F, 4, par1, 4, par2);
        this.leg3.setRotationPoint(-3.0F, (float)(24 - par1), -5.0F);

        this.leg4 = new ModelRenderer(this, 0, 16);
        this.leg4.addBox(-2.0F, 0.0F, -2.0F, 4, par1, 4, par2);
        this.leg4.setRotationPoint(3.0F, (float)(24 - par1), -5.0F);

        // ModelPig: the snout is a second cube sharing the head bone, on its own texture offset
        this.head.setTextureOffset(16, 16).addBox(-2.0F, 0.0F, -9.0F, 4, 3, 1, par2);
    }

    // The render-time rotation the extractor reads. ModelQuadruped tips the body over; without it
    // the pig's fore-aft length reads as its vanilla height (16) instead of its depth (8).
    public void setRotationAngles(float p_78087_1_, float p_78087_2_, float p_78087_3_, float p_78087_4_, float p_78087_5_, float p_78087_6_, Entity p_78087_7_)
    {
        this.head.rotateAngleX = p_78087_5_ / (180F / (float)Math.PI);
        this.head.rotateAngleY = p_78087_4_ / (180F / (float)Math.PI);
        this.body.rotateAngleX = ((float)Math.PI / 2F);
        this.leg1.rotateAngleX = MathHelper.cos(p_78087_1_ * 0.6662F) * 1.4F * p_78087_2_;
        this.leg2.rotateAngleX = MathHelper.cos(p_78087_1_ * 0.6662F + (float)Math.PI) * 1.4F * p_78087_2_;
        this.leg3.rotateAngleX = MathHelper.cos(p_78087_1_ * 0.6662F + (float)Math.PI) * 1.4F * p_78087_2_;
        this.leg4.rotateAngleX = MathHelper.cos(p_78087_1_ * 0.6662F) * 1.4F * p_78087_2_;
    }
}
