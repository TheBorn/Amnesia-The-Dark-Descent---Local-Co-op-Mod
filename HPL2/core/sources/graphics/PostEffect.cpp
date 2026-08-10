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

#include "graphics/PostEffect.h"

#include "graphics/Graphics.h"

#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT BASE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iPostEffectType::iPostEffectType(const tString& asName, cGraphics *apGraphics, cResources *apResources)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;

		msName = asName;
	}
	
	//-----------------------------------------------------------------------

	iPostEffectType::~iPostEffectType()
	{

	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPostEffect::iPostEffect(cGraphics *apGraphics,cResources *apResources, iPostEffectType *apType)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpType = apType;

		mbFinalFrameBufferUsed = false;

		mpLowLevelGraphics = mpGraphics->GetLowLevel();

		mbActive = true;
		mbDisabled = false;
	}

	//-----------------------------------------------------------------------

	iPostEffect::~iPostEffect()
	{
	}

	//-----------------------------------------------------------------------

	iTexture* iPostEffect::Render(cPostEffectComposite *apComposite, iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer, bool abLastEffect)
	{
		///////////////////////////
		// Set up variables and data
		mpCurrentComposite = apComposite;
		mbIsLastEffect = abLastEffect;

		mbFinalFrameBufferUsed = false;

		///////////////////////////
		// Render
		iTexture *pOutputTex = RenderEffect(apInputTexture, apFinalTempBuffer);
		
		//////////////////////////
		// If last effect and final frame buffer has not been called, copy to rendertarget
		//
		// cPostEffectComposite::CopyToFrameBuffer, not a second copy written out
		// here. This used to be its own five lines, and they were not the same five:
		// no depth state, no alpha or channel mode, no flat projection, and -- the
		// one that showed -- no SetTextureRange(NULL, 1), so whatever the effect had
		// left on texture units 1 and up was still bound while this drew with no
		// program at all. Bloom leaves unit 1 holding its input; the sanity wave
		// clears it. So the picture depended on which effect happened to be last,
		// and the chain changes length whenever an effect comes or goes -- alternate
		// frames, alternate result, a flicker on whichever screen takes this path.
		//
		// Nobody noticed for fifteen years because nothing in the game reached here:
		// every effect claimed the final buffer itself. Routing an off-centre
		// viewport's last effect through a temp buffer (see SetFinalFrameBuffer) is
		// what started using it every frame.
		if(mbIsLastEffect && mbFinalFrameBufferUsed == false)
		{
			mpCurrentComposite->CopyToFrameBuffer(pOutputTex);
		}

		return pOutputTex;
	}
	
	//-----------------------------------------------------------------------

	void iPostEffect::SetActive(bool abX)
	{
		if(mbActive == abX) return;

		mbActive = abX;

		OnSetActive(abX);
	}
	
	//-----------------------------------------------------------------------

	void iPostEffect::SetParams(iPostEffectParams *apSrcParams)
	{
		if(mpType==NULL) return;

		//Make sure the type is correct!
		if(apSrcParams->GetName() != mpType->GetName()) return;

        GetTypeSpecificParams()->LoadFrom(apSrcParams);
		OnSetParams();
	}
	
	void iPostEffect::GetParams(iPostEffectParams *apDestParams)
	{
		if(mpType==NULL) return;

		//Make sure the type is correct!
		if(apDestParams->GetName() != mpType->GetName()) return;

		GetTypeSpecificParams()->CopyTo(apDestParams);
	}

	//-----------------------------------------------------------------------

	bool iPostEffect::TargetIsWholeScreenBuffer()
	{
		cRenderTarget *pRenderTarget = mpCurrentComposite ?
									  mpCurrentComposite->GetCurrentRenderTarget() : NULL;
		if(pRenderTarget==NULL) return true;

		if(pRenderTarget->mvPos.x != 0 || pRenderTarget->mvPos.y != 0) return false;

		//A negative size means "all of it", which is the single-player viewport.
		const cVector2l vScreen = mpLowLevelGraphics->GetScreenSizeInt();

		if(pRenderTarget->mvSize.x >= 0 && pRenderTarget->mvSize.x != vScreen.x) return false;
		if(pRenderTarget->mvSize.y >= 0 && pRenderTarget->mvSize.y != vScreen.y) return false;

		return true;
	}

	//-----------------------------------------------------------------------

	void iPostEffect::SetFinalFrameBuffer(iFrameBuffer *apOutputBuffer)
	{
		///////////////////////
		// Set the frame buffer
		if(mbIsLastEffect && TargetIsWholeScreenBuffer())
		{
			mbFinalFrameBufferUsed = true;

			cRenderTarget *pRenderTarget = mpCurrentComposite->GetCurrentRenderTarget();
			mpCurrentComposite->SetFrameBuffer(pRenderTarget->mpFrameBuffer, true);
		}
		else if(mbIsLastEffect)
		{
			//////////////////////////////////////////////////////////////////////////
			// AN EFFECT SHADER NEVER DRAWS STRAIGHT TO AN OFF-CENTRE VIEWPORT.
			//
			// This is the co-op insanity smear, and the reason it was the sanity wave
			// and nothing else: the LAST effect in the chain is the one that blits to
			// the screen, and with the wave switched off that was bloom, whose shader
			// only ever touches gl_TexCoord. The wave's does screen-space maths -- an
			// amplitude map and a zoom map looked up by where the pixel is on the
			// screen -- and a fragment shader gets those coordinates from the WINDOW,
			// not from its viewport.
			//
			// iLowLevelGraphics::mvScreenSize is fixed at the size the window was
			// CREATED at and never updated (see cLuxMapHandler::GetSplitViewportRects,
			// which says so). In dual-monitor the window is twice that wide and Player
			// 2's viewport starts at x = one whole screen -- so every one of their
			// pixels asked those maps about a position past the right-hand edge of
			// everything, got the clamped edge back, and the offset that came out of it
			// pushed the source sample clean off the texture. Clamp-to-edge on a rect
			// texture then repeats one row down the screen: the horizontal smear.
			//
			// Player 1 never saw it in the same mode -- their viewport IS at the origin
			// at exactly screen size -- and single player cannot see it at all, which is
			// why the effect has been correct for fifteen years.
			//
			// So: render the pass into the temp buffer like any intermediate, full
			// buffer at the origin, where the window and the viewport agree -- and
			// leave mbFinalFrameBufferUsed false, which makes Render() finish the job
			// with its plain program-less copy into the viewport's rect. One extra
			// full-screen blit, and no effect shader ever has to know where on the
			// desktop it landed.
			//
			// Deliberately general, not a special case for the wave: radial blur pulls
			// toward a screen centre and sepia is a lookup, so the next shader anybody
			// adds gets this for free rather than as a bug report.
			SetFrameBuffer(apOutputBuffer);
		}
		else
		{
			//////////////////////////////////////////////////////////////////////////
			// false, NOT true -- the other half of the split-screen post effect bug.
			//
			// apOutputBuffer is an INTERMEDIATE, one of cPostEffectComposite's two
			// ping-pong temp buffers, and the next effect in the chain reads it back
			// whole, from the origin -- GetScaledUVRange spans the entire texture.
			// So this pass has to FILL it from the origin.
			//
			// true bound the viewport's ON-SCREEN rect instead. Single player never
			// noticed, because there that rect IS the whole buffer and the two are the
			// same thing. In co-op they are not: Player 2's rect starts at half the
			// window (a whole monitor's width in dual-monitor), so every non-final
			// effect rasterised its output into the far side of a buffer that the
			// following effect then sampled from zero. What came back was the part of
			// that temp buffer nobody had written this pass -- whatever the renderer
			// last left there. That buffer is not idle either: it is index 0 at screen
			// size, the very same object cRendererDeferred takes for
			// mpRefractionTexture and mpEdgeSmooth_TempAccum. So Player 2's screen was
			// a stale copy of the scene, stretched across the half that was written
			// and smeared over the half that was not -- present only while post
			// processing was on, and changing character with whatever the renderer had
			// dropped in that buffer that frame, which is what made it look occasional.
			//
			// iPostEffect::SetFrameBuffer above already passes false for exactly this
			// reason. This was the same call it was fixed for, one function down, and
			// it is the one every effect's main output pass actually goes through.
			mbFinalFrameBufferUsed = true;
			mpCurrentComposite->SetFrameBuffer(apOutputBuffer, false);
		}
	}

	//-----------------------------------------------------------------------

	void iPostEffect::GetTextureUvPosAndSize(const cVector2f& avTexSize, cVector2f& avUvPos,  cVector2f& avUvSize)
	{
		//////////////////////////////////////////////////////////////////////////
		// THE SPLIT-SCREEN CO-OP BUG.
		//
		// This used to locate the viewport's slice of the source as
		//
		//     viewportPos / screenSize * texSize
		//
		// which was right when every viewport rasterised into its own sub-rect of a
		// shared screen-sized buffer. That is no longer how this renderer works.
		// cRendererDeferred::RenderObjects calls SetupInternalBufferRenderTarget and
		// rasterises EVERY viewport full-buffer at the origin, from that viewport's own
		// camera; CopyToFrameBuffer is what shrinks the finished image into the
		// viewport's on-screen rect, at the very end and nowhere else.
		//
		// So the source already holds THIS viewport's complete view, at the origin. The
		// old maths took a slice out of it anyway, positioned by where the viewport sits
		// on the monitor -- so Player 2, at half the screen width, got the right-hand
		// half of their OWN full view stretched across their half of the screen, and
		// Player 1 got the left-hand half of theirs. That is the off-centre picture: each
		// player's crosshair sits where it belongs, but the world behind it is shifted
		// and stretched by half a screen. In dual-monitor the division is exactly 1.0 and
		// the sampling runs clean off the right edge of the texture -- solid black.
		//
		// And it only bites when a post effect is active, because the effect-free path
		// goes straight through cRendererDeferred::CopyToFrameBuffer, which had already
		// been converted. Bloom, sepia and radial blur switching on and off is what makes
		// it come and go. Single player never sees it: the viewport is at the origin and
		// covers the screen, so the old maths reduced to the identity.
		//
		// The whole (render-scaled) source, at the origin -- the same rect the renderer
		// itself samples in cRendererDeferred::CopyToFrameBuffer.
		//
		// GetScaledSizeF, NOT avTexSize * scale. The renderer sizes its viewport
		// with the integer-rounded scale, so a raw float multiply here lands a
		// fraction of a texel away from the rect that was actually rasterised --
		// and a fraction of a texel outside it is unwritten black. They have to
		// be the same function or they are eventually the same bug.
		avUvPos = cVector2f(0,0);
		avUvSize = mpCurrentComposite->GetScaledSizeF(avTexSize);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::SetFrameBuffer(iFrameBuffer *apFrameBuffer)
	{
		iTexture *pTex = apFrameBuffer->GetColorBuffer(0)->ToTexture();

		/////////////////////
		//Check if texture is same size as screen, if so no need do any extra calcs.
		//A scaled render never qualifies: the buffer is still screen sized but only
		//its top-left sub-rect is in play, so the rect has to be computed.
		if(pTex->GetSizeInt2D() == mpLowLevelGraphics->GetScreenSizeInt() &&
			mpCurrentComposite->IsRenderScaled()==false)
		{
			//false, not true. true rasterises this pass into the viewport's ON-SCREEN
			//rect; the source it is filtering was rasterised full-buffer at the origin,
			//so an intermediate has to match the source. Identity in single player,
			//where those two rects are the same thing.
			mpCurrentComposite->SetFrameBuffer(apFrameBuffer,false);
		}
		/////////////////////
		//Texture does not have same size as screen, need to do extra calculations
		else
		{
			cVector2f vTexSize = pTex->GetSizeFloat2D();
			cVector2f vUvPos, vUvSize;
			GetTextureUvPosAndSize(vTexSize,vUvPos, vUvSize);

			cVector2l vTargetPos((int)(vUvPos.x+0.5f), (int)(vUvPos.y+0.5f));		
			cVector2l vTargetSize((int)(vUvSize.x+0.5f), (int)(vUvSize.y+0.5f));

			mpLowLevelGraphics->SetCurrentFrameBuffer(apFrameBuffer, vTargetPos, vTargetSize);
		}
	}

	//-----------------------------------------------------------------------

	void iPostEffect::DrawQuad(const cVector3f& avPos,  const cVector2f& avSize, iTexture *apTexture, bool abFlipY)
	{
		//The renderer's own rect helper rather than the same arithmetic written
		//out again -- including its half-texel inset, which is what keeps the
		//blur taps off the unwritten part of the buffer.
		cVector2f vTexSize = apTexture->GetSizeFloat2D();
		cVector2f vMinUV, vMaxUV;
		mpCurrentComposite->GetScaledUVRange(vTexSize, vMinUV, vMaxUV);

		mpCurrentComposite->DrawQuad(avPos,avSize, vMinUV, vMaxUV, abFlipY);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::DrawQuad(	const cVector3f& avPos,  const cVector2f& avSize, iTexture *apTexture0, iTexture *apTexture1, 
								bool abFlipY0,bool abFlipY1)
	{
		cVector2f vTexSize[2] = {apTexture0->GetSizeFloat2D(), apTexture1->GetSizeFloat2D()};
		cVector2f vTexMin[2], vTexMax[2];
		
		for(int i=0; i<2; ++i)
			mpCurrentComposite->GetScaledUVRange(vTexSize[i], vTexMin[i], vTexMax[i]);

		mpCurrentComposite->DrawQuad(cVector2f(0,0),1,vTexMin[0],vTexMax[0],vTexMin[1],vTexMax[1],abFlipY0,abFlipY1);
	}

	//-----------------------------------------------------------------------

}
