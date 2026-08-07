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

#include "LuxEffectRenderer.h"

#include "LuxMapHandler.h"


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PROGRAM VARIABLES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#define kVar_afBlurSize			0
#define kVar_afColorMul			1

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffectRenderer::cLuxEffectRenderer() : iLuxUpdateable("LuxEffectRenderer")
{
	/////////////////////////////
	//Setup vars
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cVector2l vScreenSize = pGraphics->GetLowLevel()->GetScreenSizeInt();

	/////////////////////////////
	//Get deferred renderer stuff
	cRendererDeferred *pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));

	mpDeferredAccumBuffer = pRendererDeferred->GetAccumBuffer();

	/////////////////////////////
	//Create Outline color buffer
	//
	//PRIVATE, deliberately NOT GetTempFrameBuffer.
	//
	//That is a pool keyed on (size, format, index), and screen-size RGBA index 0
	//is already claimed three times over: cRendererDeferred's refraction texture
	//and its edge-smooth accumulator, and cPostEffectComposite's ping-pong buffer
	//-- which holds the COMPLETE finished frame. All four were the same texture.
	//
	//So this pass was reading, blurring and additively compositing over the
	//viewport whatever the post effects or a water refraction copy had last left
	//there: a snapshot of a scene, in the rectangular chunks the refraction copy
	//works in, stale for as long as nothing overwrote it. It showed up the moment
	//an outline was drawn -- i.e. the moment either player looked at a pickup.
	mpOutlineColorTexture = pGraphics->CreateTexture("LuxOutlineColor", eTextureType_Rect,
													 eTextureUsage_RenderTarget);
	mpOutlineColorTexture->CreateFromRawData(cVector3l(vScreenSize.x, vScreenSize.y, 0),
											 ePixelFormat_RGBA, NULL);
	mpOutlineColorTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

	mpFrameBufferColor = pGraphics->CreateFrameBuffer("OutlineColor");
	mpFrameBufferColor->SetDepthStencilBuffer(pRendererDeferred->GetDepthStencilBuffer());
	mpFrameBufferColor->SetTexture2D(0,mpOutlineColorTexture);

	mpFrameBufferColor->CompileAndValidate();	

	cParserVarContainer programVars;

	/////////////////////////////
	//Load Flash programs

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpFlashProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "game_object_flash_frag.glsl",&programVars);
	mpFlashProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();

	/////////////////////////////
	//Load Enemy Glow programs

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpEnemyGlowProgram = pGraphics->CreateGpuProgramFromShaders("EnemyGlow","deferred_base_vtx.glsl", "game_enemy_darkness_glow_frag.glsl",&programVars);
	mpEnemyGlowProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();
		
	/////////////////////////////
	//Load Outline programs

	//TODO: Could perhaps use deferred_base_vtx for color? Since the scale can be done to matrix.
	for(int i=0; i<2; ++i)
	{
		if(i==1) programVars.Add("UseAlpha");
		mpOutlineColorProgram[i] = pGraphics->CreateGpuProgramFromShaders("GameOutline","game_outline_vtx.glsl", "game_outline_frag.glsl",&programVars);
		programVars.Clear();
	}

	mpOutlineStencilProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "deferred_base_frag.glsl",&programVars);
	
	programVars.Add("UseUv");
	programVars.Add("UseDiffuse");
	mpOutlineStencilAlphaProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "deferred_base_frag.glsl",&programVars);
	programVars.Clear();

	

	///////////////////////////
	// Load Blur Programs
	mlBlurSizeDiv = 4;
	for(int i=0; i<2; ++i)
	{
		cParserVarContainer vars;
		if(i==1) vars.Add("BlurHorisontal");
		mpBlurProgram[i] = pGraphics->CreateGpuProgramFromShaders("BloomBlur","posteffect_bloom_blur_vtx.glsl", "posteffect_bloom_blur_frag.glsl", &vars);

		if(mpBlurProgram[i])
			mpBlurProgram[i]->GetVariableAsId("afBlurSize",kVar_afBlurSize);

		//PRIVATE, like the outline colour buffer above and for the same reason: the
		//temp pool is keyed on (size, format, index), so anything else asking for the
		//same triple gets the same buffer. The composite below ADDs the whole of
		//blur[1] across the whole accumulation buffer, so anything living in there
		//arrives on screen as a full-frame wash.
		cVector2l vBlurSize = vScreenSize / mlBlurSizeDiv;

		mpBlurTexture[i] = pGraphics->CreateTexture("LuxOutlineBlur" + cString::ToString(i),
												   eTextureType_Rect, eTextureUsage_RenderTarget);
		mpBlurTexture[i]->CreateFromRawData(cVector3l(vBlurSize.x, vBlurSize.y, 0),
											ePixelFormat_RGBA, NULL);
		mpBlurTexture[i]->SetWrapSTR(eTextureWrap_ClampToEdge);

		mpBlurBuffer[i] = pGraphics->CreateFrameBuffer("LuxOutlineBlurBuffer" + cString::ToString(i));
		mpBlurBuffer[i]->SetTexture2D(0, mpBlurTexture[i]);
		mpBlurBuffer[i]->CompileAndValidate();
	}


	///////////////////////////
	// Reset variables
	mFlashOscill.SetUp(0,1,0,1,1);

	///////////////////////////
	// Reset variables
	Reset();
}

//-----------------------------------------------------------------------

cLuxEffectRenderer::~cLuxEffectRenderer()
{
	//The blur buffers still come from the shared pool and belong to it. Only the
	//outline colour texture and the frame buffer wrapping it are ours.
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();

	if(mpFrameBufferColor)    pGraphics->DestroyFrameBuffer(mpFrameBufferColor);
	if(mpOutlineColorTexture) pGraphics->DestroyTexture(mpOutlineColorTexture);

	for(int i=0; i<2; ++i)
	{
		if(mpBlurBuffer[i])  pGraphics->DestroyFrameBuffer(mpBlurBuffer[i]);
		if(mpBlurTexture[i]) pGraphics->DestroyTexture(mpBlurTexture[i]);
		mpBlurBuffer[i] = NULL;
		mpBlurTexture[i] = NULL;
	}

	mpFrameBufferColor = NULL;
	mpOutlineColorTexture = NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Reset()
{
	mvOutlineObjects[0].clear();
	mvOutlineObjects[1].clear();
	ClearRenderLists();
}

void cLuxEffectRenderer::ClearRenderLists()
{
	mvFlashObjects.clear();
	mvEnemyGlowObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Update(float afTimeStep)
{
	mFlashOscill.Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderSolid(cRendererCallbackFunctions* apFunctions, int alPlayerIndex)
{
	if(apFunctions->GetSettings()->mbIsReflection) return;

}

void cLuxEffectRenderer::RenderTrans(cRendererCallbackFunctions* apFunctions, int alPlayerIndex)
{
	/////////////////////////////////////
	// Normal and Reflection
	RenderFlashObjects(apFunctions);
	RenderEnemyGlow(apFunctions);

	
	/////////////////////////////////////
	// Only Normal
	if(apFunctions->GetSettings()->mbIsReflection==false)
	{
		RenderOutline(apFunctions, alPlayerIndex);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddOutlineObject(iRenderable *apObject, int alPlayerIndex)
{
	mvOutlineObjects[alPlayerIndex == 1 ? 1 : 0].push_back(apObject);
}

void cLuxEffectRenderer::ClearOutlineObjects(int alPlayerIndex)
{
	mvOutlineObjects[alPlayerIndex == 1 ? 1 : 0].clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddFlashObject(iRenderable *apObject, float afAlpha)
{
	mvFlashObjects.push_back(cGlowObject(apObject, afAlpha) );
}

void cLuxEffectRenderer::AddEnemyGlow(iRenderable *apObject, float afAlpha)
{
	mvEnemyGlowObjects.push_back(cGlowObject(apObject, afAlpha) );
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderFlashObjects(cRendererCallbackFunctions* apFunctions)
{
	if(mvFlashObjects.empty()) return;
	if(mpFlashProgram==NULL) return;
	
	////////////////////////////////////
	// Setup functions
	apFunctions->SetDepthTestFunc(eDepthTestFunc_Equal);
	apFunctions->SetDepthTest(true);
	apFunctions->SetDepthWrite(false);
	apFunctions->SetBlendMode(eMaterialBlendMode_Add);
	apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);
	apFunctions->SetChannelMode(eMaterialChannelMode_RGBA);

	apFunctions->SetProgram(mpFlashProgram);
	
	float fGlobalAlpha = (0.5f+mFlashOscill.val*0.5f);

	////////////////////////////////////
	// Render objects
	cFrustum *pFrustum = apFunctions->GetFrustum();
	for(size_t i=0; i<mvFlashObjects.size(); ++i)
	{
		iRenderable *pObject = mvFlashObjects[i].mpObject;

		if(pObject->CollidesWithFrustum(pFrustum)==false) continue;

		if(mpFlashProgram)
			mpFlashProgram->SetFloat(kVar_afColorMul,mvFlashObjects[i].mfAlpha*fGlobalAlpha);
		
		apFunctions->SetTexture(0, pObject->GetMaterial()->GetTexture(eMaterialTexture_Diffuse));
	
		apFunctions->SetVertexBuffer(pObject->GetVertexBuffer());
		apFunctions->SetMatrix(pObject->GetModelMatrixPtr());
		
		for(int i=0; i<2; ++i)
			apFunctions->DrawCurrent();
	}

	apFunctions->SetTexture(0,NULL);
	apFunctions->SetProgram(NULL);
	apFunctions->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderEnemyGlow(cRendererCallbackFunctions* apFunctions)
{
	if(mvEnemyGlowObjects.empty()) return;
	if(mpEnemyGlowProgram==NULL) return;

	////////////////////////////////////
	// Setup functions
	apFunctions->SetDepthTestFunc(eDepthTestFunc_Equal);
	apFunctions->SetDepthTest(true);
	apFunctions->SetDepthWrite(false);
	apFunctions->SetBlendMode(eMaterialBlendMode_Add);
	apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);
	apFunctions->SetChannelMode(eMaterialChannelMode_RGBA);

	apFunctions->SetProgram(mpEnemyGlowProgram);

	////////////////////////////////////
	// Render objects
	cFrustum *pFrustum = apFunctions->GetFrustum();
	for(size_t i=0; i<mvEnemyGlowObjects.size(); ++i)
	{
		iRenderable *pObject = mvEnemyGlowObjects[i].mpObject;

		if(pObject->CollidesWithFrustum(pFrustum)==false) continue;

		mpEnemyGlowProgram->SetFloat(kVar_afColorMul,mvEnemyGlowObjects[i].mfAlpha);

		apFunctions->SetTexture(0, pObject->GetMaterial()->GetTexture(eMaterialTexture_Diffuse));

		apFunctions->SetVertexBuffer(pObject->GetVertexBuffer());
		apFunctions->SetMatrix(pObject->GetModelMatrixPtr());

		apFunctions->DrawCurrent();
	}

	apFunctions->SetTexture(0,NULL);
	apFunctions->SetProgram(NULL);
	apFunctions->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderOutline(cRendererCallbackFunctions* apFunctions, int alPlayerIndex)
{
	// Only this viewport's owner's outline.
	std::vector<iRenderable*> &vOutlineObjects = mvOutlineObjects[alPlayerIndex == 1 ? 1 : 0];

	if(vOutlineObjects.empty()) return;

	/////////////////////////////
	//Setup vars
	float fScaleAdd = 0.02f;
	
	////////////////////////////////////
	// Get entities to be rendered
	cFrustum *pFrustum = apFunctions->GetFrustum();
	tRenderableList lstObjects;
	for(size_t i=0; i<vOutlineObjects.size(); ++i)
	{
		iRenderable *pObject = vOutlineObjects[i];

		if(pObject->CollidesWithFrustum(pFrustum))
		{
			lstObjects.push_back(pObject);
		}
	}

	////////////////////////////////////
	// Build AABB from all objects
	cBoundingVolume totalBV;
	cVector3f vTotalMin = 100000.0f;
	cVector3f vTotalMax = -100000.0f;
	for(tRenderableListIt it = lstObjects.begin(); it != lstObjects.end(); ++it)
	{
		iRenderable *pObject = *it;
		cBoundingVolume *pBV = pObject->GetBoundingVolume();

		cMath::ExpandAABB(vTotalMin, vTotalMax, pBV->GetMin(), pBV->GetMax());        
	}
	
	//Need to scale so the outline is contained.
	cVector3f vTotalSize = vTotalMax - vTotalMin;
	cVector3f vTotalAdd = vTotalSize * (cVector3f(1.0f)/vTotalSize) * (fScaleAdd*2);
		
	totalBV.SetLocalMinMax(vTotalMin-vTotalAdd, vTotalMax+vTotalAdd);

	//The clip rect this used to build was in full-screen space but was handed to
	//SetScissorRect, which treats a rect as viewport-relative: it adds the
	//viewport's on-screen position and clamps to the viewport's size. In split
	//screen that moved the scissor a whole viewport away from the pixels it was
	//meant to keep, so P2's finished outline was clipped away to nothing.

	////////////////////////////////////
	// General setup
	apFunctions->SetFrameBuffer(mpFrameBufferColor);

	//Clear the WHOLE buffer, not the viewport's rect. This buffer is full screen
	//and the outline is rendered into it full screen, so a viewport-sized clear
	//leaves stale colour and stale stencil exactly where the outline lands --
	//and stale 0xFF stencil is rejected by the NotEqual test further down.
	//
	//KNOWN LIMITATION, do not 'fix' this by toggling the scissor. glClear obeys the
	//SCISSOR box, and split screen keeps a global scissor active for the whole pass
	//set to the viewport's ON-SCREEN rect -- P2's is entirely outside a buffer that
	//is only screen-wide, so P2's clear covers nothing. Bypassing that by calling
	//SetScissorActive(false) then (true) FROZE P2's viewport: the low level early
	//-outs when the state already matches, so if the test was already off the
	//restore switched it ON with a stale rect and clipped the rest of the frame.
	//The prior state cannot be read back from here, so any enable/disable pair is
	//a guess. Setting the scissor RECT to the whole buffer would be the safe way.
	apFunctions->ClearFrameBuffer(eClearFrameBufferFlag_Stencil | eClearFrameBufferFlag_Color,false);

	apFunctions->SetDepthTest(true);
	apFunctions->SetDepthWrite(false);
	apFunctions->SetStencilActive(true);

	////////////////////////////////////
	// Render stencil

	apFunctions->SetBlendMode(eMaterialBlendMode_None);
	apFunctions->SetChannelMode(eMaterialChannelMode_None);

	apFunctions->GetLowLevelGfx()->SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Replace);

	
	for(tRenderableListIt it = lstObjects.begin(); it != lstObjects.end(); ++it)
	{
		iRenderable *pObject = *it;

		/////////////////
		// Solid
		if(pObject->GetMaterial()->GetTexture(eMaterialTexture_Alpha)==NULL)
		{
			apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);
			apFunctions->SetProgram(mpOutlineStencilProgram);
			apFunctions->SetTexture(0, NULL);
		}
		/////////////////
		// Trans
		else
		{
			apFunctions->SetAlphaMode(eMaterialAlphaMode_Trans);
			apFunctions->SetProgram(mpOutlineStencilAlphaProgram);
			apFunctions->SetTexture(0, pObject->GetMaterial()->GetTexture(eMaterialTexture_Alpha));
		}

		apFunctions->SetVertexBuffer(pObject->GetVertexBuffer());
		apFunctions->SetMatrix(pObject->GetModelMatrixPtr());
		apFunctions->DrawCurrent();
	}

	////////////////////////////////////
	// Render out line

	apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);
	apFunctions->SetBlendMode(eMaterialBlendMode_None);
	apFunctions->SetChannelMode(eMaterialChannelMode_RGBA);

	apFunctions->GetLowLevelGfx()->SetStencil(eStencilFunc_NotEqual,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Keep);

	apFunctions->SetTextureRange(NULL, 0);

	//Iterate objects and render them
	for(tRenderableListIt it = lstObjects.begin(); it != lstObjects.end(); ++it)
	{
		iRenderable *pObject = *it;
		cMaterial *pMat = pObject->GetMaterial();

		cBoundingVolume* pBV = pObject->GetBoundingVolume();
		
        if(pMat->GetTexture(eMaterialTexture_Alpha))
		{
			apFunctions->SetTexture(0, pMat->GetTexture(eMaterialTexture_Alpha));
			apFunctions->SetProgram(mpOutlineColorProgram[1]);
			mpOutlineColorProgram[1]->SetColor3f(mpOutlineColorProgram[1]->GetVariableId("gvColor"), cColor(0,0,0.5f,0));
		}
		else
		{
			apFunctions->SetTexture(0, NULL);
			apFunctions->SetProgram(mpOutlineColorProgram[0]);
			mpOutlineColorProgram[0]->SetColor3f(mpOutlineColorProgram[0]->GetVariableId("gvColor"), cColor(0,0,0.5f,0));
		}
		
		cVector3f vLocalSize = pBV->GetLocalMax() - pBV->GetLocalMin();
		cVector3f vScale = (cVector3f(1.0f)/vLocalSize) * fScaleAdd  + cVector3f(1.0f);

		cMatrixf mtxScale = cMath::MatrixMul(cMath::MatrixScale(vScale), cMath::MatrixTranslate(pBV->GetLocalCenter()*-1));
		mtxScale.SetTranslation(mtxScale.GetTranslation() + pBV->GetLocalCenter());
		m_mtxTemp = cMath::MatrixMul(pObject->GetWorldMatrix(), mtxScale);

		apFunctions->SetVertexBuffer(pObject->GetVertexBuffer());
		apFunctions->SetMatrix(NULL);
		apFunctions->SetMatrix(&m_mtxTemp);
		apFunctions->DrawCurrent();
	}

	////////////////////////////////////
	// Blur the color buffer
	apFunctions->SetFlatProjection();

	apFunctions->SetStencilActive(false);
	apFunctions->SetDepthTest(false);

	for(int i=0; i<2; ++i)
	{
		//Reverse order so blur program is not set unneeded times
		if(mpBlurProgram[1-i])
			mpBlurProgram[1-i]->SetFloat(kVar_afBlurSize, 1.0f);	
	}

	int lBlurIterations = 2;
	RenderOutlineBlur(apFunctions, mpOutlineColorTexture);
	for(int i=1; i<lBlurIterations;++i)
	{
		RenderOutlineBlur(apFunctions, mpBlurTexture[1]);
	}	


	////////////////////////////////////
	// Draw the color buffer
	apFunctions->SetStencilActive(true);
	apFunctions->SetBlendMode(eMaterialBlendMode_Add);

	apFunctions->SetProgram(NULL);
	apFunctions->SetFrameBuffer(mpDeferredAccumBuffer);

	//No scissor. It was only ever an optimisation -- the blurred outline texture
	//is black everywhere except around the object and this is an ADD, so the
	//unclipped quad gives an identical image. Getting it right instead would mean
	//knowing the viewport's on-screen offset here, which the callback interface
	//does not expose, and getting it wrong costs the whole outline.
	//Under a scaled render only the top-left of the blur buffer holds this frame.
	//At scale 1 this is (0,0)..(texture size), i.e. the call it replaces.
	cVector2f vBlurMinUV, vBlurMaxUV;
	apFunctions->GetScaledUVRange(mpBlurTexture[1]->GetSizeFloat2D(), vBlurMinUV, vBlurMaxUV);

	apFunctions->SetTexture(0, mpBlurTexture[1]);	
	apFunctions->DrawQuad(0,1,vBlurMinUV,vBlurMaxUV,true);
	//apFunctions->SetTexture(0, mpOutlineColorTexture);	
	//apFunctions->DrawQuad(0,1,0,mpOutlineColorTexture->GetSizeFloat2D(),true);
	
	////////////////////////////////////
	// Reset 
	apFunctions->SetNormalFrustumProjection();
	apFunctions->SetStencilActive(false);
	apFunctions->SetScissorActive(false);
	
	apFunctions->SetTextureRange(NULL, 0);

	//Debug:
	/*apFunctions->SetBlendMode(eMaterialBlendMode_None);
	apFunctions->SetTexture(0, NULL);
	apFunctions->SetMatrix(NULL);
	apFunctions->GetLowLevelGfx()->DrawBoxMinMax(totalBV.GetMin(),totalBV.GetMax(),cColor(1,1));*/
}

void cLuxEffectRenderer::RenderOutlineBlur(cRendererCallbackFunctions* apFunctions, iTexture *apInputTex)
{
	apFunctions->SetFrameBuffer(mpBlurBuffer[0]);
	
	apFunctions->SetProgram(mpBlurProgram[0]);
	apFunctions->SetTexture(0, apInputTex);
	apFunctions->DrawQuad(0,1,apInputTex->GetSizeFloat2D(),true);

	apFunctions->SetFrameBuffer(mpBlurBuffer[1]);
	apFunctions->SetProgram(mpBlurProgram[1]);
	apFunctions->SetTexture(0, mpBlurTexture[0]);
	apFunctions->DrawQuad(0,1,mpBlurTexture[0]->GetSizeFloat2D(),true);
}

//-----------------------------------------------------------------------



