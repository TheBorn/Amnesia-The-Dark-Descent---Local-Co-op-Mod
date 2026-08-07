/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "graphics/PostEffect_ImageTrail.h"

#include "graphics/Graphics.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"

#include "system/PreprocessParser.h"

namespace hpl {
	
	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARS
	//////////////////////////////////////////////////////////////////////////

	#define kVar_afAlpha	0

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT BASE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cPostEffectType_ImageTrail::cPostEffectType_ImageTrail(cGraphics *apGraphics, cResources *apResources) : iPostEffectType("ImageTrail",apGraphics,apResources)
	{
		cParserVarContainer vars;
		vars.Add("UseUv");

		mpProgram = mpGraphics->CreateGpuProgramFromShaders("ImageTrail","deferred_base_vtx.glsl", "posteffect_image_trail_frag.glsl", &vars);
		if(mpProgram)
		{
			mpProgram->GetVariableAsId("afAlpha",kVar_afAlpha);
		}
	}
	
	//-----------------------------------------------------------------------

	cPostEffectType_ImageTrail::~cPostEffectType_ImageTrail()
	{

	}

	//-----------------------------------------------------------------------

	iPostEffect * cPostEffectType_ImageTrail::CreatePostEffect(iPostEffectParams *apParams)
	{
		cPostEffect_ImageTrail *pEffect = hplNew(cPostEffect_ImageTrail, (mpGraphics,mpResources,this));
		cPostEffectParams_ImageTrail *pImageTrailParams = static_cast<cPostEffectParams_ImageTrail*>(apParams);

		return pEffect;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPostEffect_ImageTrail::cPostEffect_ImageTrail(cGraphics *apGraphics, cResources *apResources, iPostEffectType *apType) : iPostEffect(apGraphics,apResources,apType)
	{
		cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
		
		mpAccumTexture = mpGraphics->CreateTexture("ImageTrailTexture", eTextureType_Rect, eTextureUsage_RenderTarget);	
		mpAccumTexture->CreateFromRawData(cVector3l(vSize.x, vSize.y,1), ePixelFormat_RGB, NULL);

		mpAccumBuffer = mpGraphics->CreateFrameBuffer("ImageTrailBuffer");
		mpAccumBuffer->SetTexture2D(0, mpAccumTexture);
		if(mpAccumBuffer->CompileAndValidate()==false)
		{
			Error("Could not compile and validate image trail frame buffer!\n");
		}

		mpImageTrailType = static_cast<cPostEffectType_ImageTrail*>(mpType);

		mbClearFrameBuffer = true;
	}

	//-----------------------------------------------------------------------

	cPostEffect_ImageTrail::~cPostEffect_ImageTrail()
	{

	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::Reset()
	{
		mbClearFrameBuffer = true;
	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::OnSetParams()
	{
		
	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::OnSetActive(bool abX)
	{
		if(abX == false)
		{
			Reset();
		}
	}

	//-----------------------------------------------------------------------


	iTexture* cPostEffect_ImageTrail::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
	{
		/////////////////////////
		// Init render states
		mpCurrentComposite->SetFlatProjection();
		mpCurrentComposite->SetBlendMode(eMaterialBlendMode_Alpha);
		mpCurrentComposite->SetChannelMode(eMaterialChannelMode_RGBA);
		mpCurrentComposite->SetTextureRange(NULL,0);

		/////////////////////////
		// Render to accumulation buffer
		mpCurrentComposite->SetFrameBuffer(mpAccumBuffer);
		
		mpCurrentComposite->SetProgram(mpImageTrailType->mpProgram);
		if(mbClearFrameBuffer)
		{
			mpCurrentComposite->ClearFrameBuffer(eClearFrameBufferFlag_Color, true);
			mbClearFrameBuffer = false;
			if(mpImageTrailType->mpProgram)
				mpImageTrailType->mpProgram->SetFloat(kVar_afAlpha, 1.0f);
		}
		else
		{
			// Get the amount of blur depending frame time.
			//*30 is just so that good amount values are still between 0 - 1
			//
			// THE WHOLE-SCREEN BLACK FLICKER.
			//
			// This is the only thing in the frame that carries state ACROSS frames: an
			// accumulation buffer that each frame is blended into by fAmount, and which
			// is what actually reaches the screen. Whatever is in that buffer IS the
			// picture, so anything that stops it updating shows the buffer's old
			// contents instead of this frame.
			//
			// fFrameTime is divided by, and it is not always sane. cLuxHelpFuncs renders
			// the menu background capture with 0.0001, which sends fPow to 10000*amount
			// and exp(-150*amount) to a hard zero; an exactly-zero frame time gives
			// 1.0/0 = inf and the same zero, and 0 * inf gives a NaN that poisons the
			// blend outright. At alpha zero the buffer keeps exactly what it had. If the
			// last thing to touch it was the clear in the branch above, that is solid
			// black -- and the entire screen goes black for as long as it lasts, with
			// the scene itself rendering perfectly the whole time. That is why every
			// probe before the post-effect chain came back reporting a clean frame.
			//
			// Clamp the frame time to something a frame can actually take, and put a
			// floor under the blend so the buffer always takes at least a little of the
			// current frame. It can go stale for a few frames now; it can never freeze,
			// and it can never sit on black.
			float fFrameTime = mpCurrentComposite->GetCurrentFrameTime();
			if(fFrameTime < 0.001f)	fFrameTime = 0.001f;
			if(fFrameTime > 0.25f)	fFrameTime = 0.25f;

			float fPow = (1.0f / fFrameTime) * mParams.mfAmount; //The higher this is, the more blur!
			float fAmount = exp(-fPow * 0.015f); 

			//NaN fails every comparison including against itself, so this is the test.
			if(fAmount != fAmount)	fAmount = 1.0f;
			if(fAmount < 0.02f)		fAmount = 0.02f;
			if(fAmount > 1.0f)		fAmount = 1.0f;

			if(mpImageTrailType->mpProgram)
				mpImageTrailType->mpProgram->SetFloat(kVar_afAlpha, fAmount);
		}
		
		mpCurrentComposite->SetTexture(0, apInputTexture);
				
		DrawQuad(0, 1, apInputTexture, true);

		mpCurrentComposite->SetProgram(NULL);
		mpCurrentComposite->SetBlendMode(eMaterialBlendMode_None);
		
		return mpAccumTexture;
	}

	//-----------------------------------------------------------------------

}
