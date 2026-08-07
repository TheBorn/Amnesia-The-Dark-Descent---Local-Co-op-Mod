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

#include "graphics/PostEffectComposite.h"

#include "impl/ImGuiDebugMenu.h"

#include "system/LowLevelSystem.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/Graphics.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/PostEffect.h"
#include "graphics/VertexBuffer.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPostEffectComposite::cPostEffectComposite(cGraphics *apGraphics)
	{
		mpGraphics = apGraphics;
		SetupRenderFunctions(mpGraphics->GetLowLevel());

		cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
		for(int i=0; i<2; ++i)
		{
			mpFinalTempBuffer[i] = mpGraphics->GetTempFrameBuffer(vSize,ePixelFormat_RGBA,i);
		}
	}

	//-----------------------------------------------------------------------

	cPostEffectComposite::~cPostEffectComposite()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cPostEffectComposite::Render(float afFrameTime, cFrustum *apFrustum, iTexture *apInputTexture, cRenderTarget *apRenderTarget)
	{
		////////////////////////////////
		//Set up stuff needed for rendering
		BeginRendering(afFrameTime, apFrustum, apInputTexture, apRenderTarget);

		////////////////////////////////
		//Iterate post effects and find the last one.
		iPostEffect *pLastEffect = NULL;
		tPostEffectMapIt it = m_mapPostEffects.begin();
        for(; it!= m_mapPostEffects.end(); ++it)
		{
			iPostEffect *pPostEffect = it->second;
			if(pPostEffect->IsActive()==false) continue;
			
			pLastEffect = pPostEffect;
		}

		////////////////////////////////
		//Nothing active after all -- present the input instead of nothing.
		//
		//THIS IS WHERE A BLACK FRAME COMES FROM. cScene::Render asks
		//HasActiveEffects() BEFORE it renders the world, and passes the answer to
		//iRenderer::Render as "put your output in a texture rather than on screen".
		//The only thing that then puts anything on screen is the LAST active effect,
		//via bLastEffect. So if the answer stops being true between that question and
		//this loop, the world is sitting in a texture, no effect claims the final
		//blit, and what the player sees is the black clear from the top of
		//cScene::Render. One frame, black, for as long as the disagreement lasts.
		//
		//The two are asked at different times and of different containers --
		//HasActiveEffects walks mvPostEffects, this walks m_mapPostEffects -- and
		//everything between them runs while the answer is assumed frozen: the whole
		//world render, the renderer callback list, OnPostWorldDraw and the 3D GUI.
		//Anything in there that ends an effect wins the race.
		//
		//Rather than try to freeze the answer, make the disagreement harmless: if
		//nothing is going to claim the blit, do the blit here. The engine already had
		//CopyToFrameBuffer for exactly this and it was commented out, so there was no
		//path at all from "asked to present a frame" to "presented one".
		if(pLastEffect == NULL)
		{
			CopyToFrameBuffer(apInputTexture);
			EndRendering();
			return;
		}

		////////////////////////////////
		//Iterate post effects and render them
		int lCurrentTempBuffer =0;
		iTexture *pInputTex = apInputTexture;
		it = m_mapPostEffects.begin();
		for(; it!= m_mapPostEffects.end(); ++it)
		{
			iPostEffect *pPostEffect =it->second;
			if(pPostEffect->IsActive()==false) continue;
		
			bool bLastEffect = pPostEffect == pLastEffect;

			pInputTex = pPostEffect->Render(this,pInputTex,mpFinalTempBuffer[lCurrentTempBuffer] ,bLastEffect);

			lCurrentTempBuffer = lCurrentTempBuffer==0 ? 1 : 0;
		}

		///////////////////////////////
		// Reset rendering stuff
		EndRendering();
	}

	//-----------------------------------------------------------------------

	void cPostEffectComposite::AddPostEffect(iPostEffect *apPostEffect, int alPrio)
	{
		if(apPostEffect==NULL) return;

		m_mapPostEffects.insert(tPostEffectMap::value_type(alPrio, apPostEffect));
		mvPostEffects.push_back(apPostEffect);
	}

	//-----------------------------------------------------------------------

	bool  cPostEffectComposite::HasActiveEffects()
	{
		if(mvPostEffects.empty()) return false;

		bool bActiveEffect = false;
		for(size_t i=0; i<mvPostEffects.size(); ++i)
		{
			if(mvPostEffects[i]->IsActive())
			{
				bActiveEffect = true;
				break;
			}
		}

		return bActiveEffect;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cPostEffectComposite::BeginRendering(float afFrameTime, cFrustum *apFrustum, iTexture *apInputTexture, cRenderTarget *apRenderTarget)
	{
		///////////////////////////////
		//Init the render functions
		mfCurrentFrameTime = afFrameTime;

		InitAndResetRenderFunctions(apFrustum, apRenderTarget, false);

		//Match the renderer. Both read the same value rather than being plumbed
		//together, so there is no way for the chain to disagree with its input.
		float fRenderScale = ::ImGuiDebugMenu::GetRenderScale();
		SetRenderScale(cVector2f(fRenderScale, fRenderScale));


		///////////////////////////////
		//Init the render states
		mpLowLevelGraphics->SetColorWriteActive(true, true, true, true);

		mpLowLevelGraphics->SetCullActive(true);
		mpLowLevelGraphics->SetCullMode(eCullMode_CounterClockwise);

		SetDepthTest(false);
		SetDepthWrite(false);
		mpLowLevelGraphics->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);

		mpLowLevelGraphics->SetColor(cColor(1,1,1,1));

		for(int i=0; i<kMaxTextureUnits; ++i)
			mpLowLevelGraphics->SetTexture(i, NULL);

	}
	
	//-----------------------------------------------------------------------

	void cPostEffectComposite::EndRendering()
	{
		/////////////////////////////////////////////
		// Reset all rendering states
		SetBlendMode(eMaterialBlendMode_None);
		SetChannelMode(eMaterialChannelMode_RGBA);

		/////////////////////////////////////////////
		// Unbind all rendering data
		for(int i=0; i<kMaxTextureUnits; ++i)
		{
			if(mvCurrentTexture[i]) mpLowLevelGraphics->SetTexture(i, NULL);
		}

		if(mpCurrentProgram)	mpCurrentProgram->UnBind();
		if(mpCurrentVtxBuffer)	mpCurrentVtxBuffer->UnBind();

		/////////////////////////////////////////////
		// Clean up render functions
		ExitAndCleanUpRenderFunctions();
	}

	//-----------------------------------------------------------------------

	/**
	 * Put a texture straight on the render target, no effect involved.
	 *
	 * Was commented out, which is why a composite with no active effect presented
	 * nothing at all instead of presenting its input. Declared in the header the
	 * whole time.
	 */
	void cPostEffectComposite::CopyToFrameBuffer(iTexture *apOutputTexture)
	{
		if(apOutputTexture == NULL) return;

		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetFrameBuffer(mpCurrentRenderTarget->mpFrameBuffer, true);

		SetFlatProjection();

		SetProgram(NULL);
		SetTexture(0, apOutputTexture);
		SetTextureRange(NULL, 1);

		////////////////////////////////////
		//Draw the input to the current frame buffer. The texture v coordinate is
		//reversed, hence the arithmetic.
		//
		//Render scale is in here, unlike in the version that was commented out: the
		//world is rasterised into the TOP-LEFT sub-rect of a screen-sized texture, so
		//the viewport's fraction has to be taken of that sub-rect and not of the whole
		//texture. Same maths as iPostEffect::GetTextureUvPosAndSize, so a pass-through
		//frame lines up with the frames either side of it instead of jumping.
		//Whole source at the origin -- see the note in iPostEffect::GetTextureUvPosAndSize.
		//This has to agree with that function exactly or a frame that happens to take the
		//pass-through path jumps against the frames either side of it.
		cVector2f vTexSize = apOutputTexture->GetSizeFloat2D();

		cVector2f vMinUV, vMaxUV;
		GetScaledUVRange(vTexSize, vMinUV, vMaxUV);

		DrawQuad(cVector3f(0,0,0), 1, vMinUV, vMaxUV, true);
	}

	//-----------------------------------------------------------------------

}
