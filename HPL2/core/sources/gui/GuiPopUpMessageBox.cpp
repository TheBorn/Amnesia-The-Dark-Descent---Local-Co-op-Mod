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

#include "gui/GuiPopUpMessageBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetButton.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetWindow.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::cGuiPopUpMessageBox(cGuiSet *apSet,
											const tWString& asLabel, const tWString& asText,
											const tWString& asButton1, const tWString& asButton2,
											void *apCallbackObject, tGuiCallbackFunc apCallback,
											float afMaxTextWidth) 
						: iGuiPopUp(apSet, false, 0)
	{
		//////////////////////////
		// Set up variables
		mpCallback = apCallback;
		mpCallbackObject = apCallbackObject;

		cGuiSkinFont *pFont = mpSkin->GetFont(eGuiSkinFont_Default);
		
		//////////////////////////
		// Word wrapped body. Same row height cWidgetLabel uses when it draws
		// wrapped text, so the box is as tall as what actually gets drawn.
		const bool bWrap = afMaxTextWidth > 0;
		float fRowHeight = pFont->mvSize.y + 2;
		float fTextHeight = pFont->mvSize.y;

		float fWindowMinLength = pFont->mpFont->GetLength(pFont->mvSize,asLabel.c_str());

		if(bWrap)
		{
			//The unwrapped length of the body is meaningless here -- that is the
			//whole point of wrapping. Width comes from the wrap width (and the
			//title, if that somehow needs more), and height from the row count.
			tWStringVec vRows;
			float fWrapRowHeight = fRowHeight;
			pFont->mpFont->GetWordWrapRows(afMaxTextWidth, fWrapRowHeight, pFont->mvSize, asText, &vRows);

			int lRows = (int)vRows.size();
			if(lRows < 1) lRows = 1;

			//One spare row: GetWordWrapRows counts breaks, and cWidgetLabel
			//advances a full row height per line as it draws, so the last line
			//needs somewhere to sit.
			fTextHeight = fRowHeight * (float)(lRows + 1);

			if(afMaxTextWidth > fWindowMinLength) fWindowMinLength = afMaxTextWidth;
		}
		else
		{
			float fTextLength = pFont->mpFont->GetLength(pFont->mvSize,asText.c_str());
			if(fTextLength > fWindowMinLength) fWindowMinLength = fTextLength;
		}

		//////////////////////////
		// Layout.
		//
		// The single-line box keeps the exact numbers it always had. A wrapped
		// one needs real margins instead: 20px of side padding is what the one
		// -liner could get away with, but a full paragraph run right up to the
		// frame reads as overflowing it, and starting at y=30 tucks the first
		// row under the title bar.
		//
		// 40 across matches where the skin indents the window's own title text,
		// so the body lines up under it instead of hanging out to its left, and
		// leaves room for the wrapper overshooting its width by the tail of a
		// word (GetWordWrapRows breaks AFTER the row exceeds the limit).
		float fSideMargin = bWrap ? 50.0f : 20.0f;
		float fTextTop    = bWrap ? 52.0f : 30.0f;
		float fTextToButtons = bWrap ? 30.0f : 20.0f;
		float fBottomMargin  = bWrap ? 30.0f : 10.0f;

		float fWindowWidth = fWindowMinLength + fSideMargin*2 > 200 ? fWindowMinLength + fSideMargin*2 : 200;

		cVector2f vVirtSize = mpSet->GetVirtualSize();

		float fButtonsTop = fTextTop + fTextHeight + fTextToButtons;
		float fWindowHeight = fButtonsTop + 30 + fBottomMargin;
		
		cVector3f vPos = cVector3f(vVirtSize.x/2 - fWindowWidth/2,vVirtSize.y/2- fWindowHeight/2,100);

		//////////////////////////
		// Window
		mpWindow->SetText(asLabel);
		mpWindow->SetPosition(vPos);
		mpWindow->SetSize(cVector2f(fWindowWidth, fWindowHeight));
		
		//////////////////////////
		// Buttons
		if(asButton2 == _W(""))
		{
			vPos = cVector3f(fWindowWidth/2 - 40, fButtonsTop,1);
			mvButtons[0] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton1,mpWindow);
			mvButtons[0]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[0]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[0]->SetGlobalUIInputListener(true);

			mvButtons[1] = NULL;
		}
		else
		{
			vPos = cVector3f(fWindowWidth/2 - (80*2+20)/2, fButtonsTop,1);
			mvButtons[0] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton1,mpWindow);
			mvButtons[0]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[0]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[0]->SetGlobalUIInputListener(true);

			vPos.x += 80+20;
			mvButtons[1] = mpSet->CreateWidgetButton(vPos,cVector2f(80,30),asButton2,mpWindow);
			mvButtons[1]->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(ButtonPress));
			mvButtons[1]->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(GamepadButtonPress));
			mvButtons[1]->SetGlobalUIInputListener(true);
			
			mvButtons[0]->SetFocusNavigation(eUIArrow_Right, mvButtons[1]);
			mvButtons[1]->SetFocusNavigation(eUIArrow_Left, mvButtons[0]);

		}

		SetUpDefaultFocus(mvButtons[0]);
		
		//////////////////////////
		// Label
		vPos = cVector3f(fSideMargin, fTextTop,1);
		mpLabel = mpSet->CreateWidgetLabel(vPos,cVector2f(bWrap ? afMaxTextWidth : fWindowWidth-10, fTextHeight),
											asText,mpWindow);
		if(bWrap) mpLabel->SetWordWrap(true);

		SetUpDefaultFocus(mvButtons[0]);
	}

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::~cGuiPopUpMessageBox()
	{
		if(mvButtons[0]) mpSet->DestroyWidget(mvButtons[0]);
		if(mvButtons[1]) mpSet->DestroyWidget(mvButtons[1]);
		if(mpLabel) mpSet->DestroyWidget(mpLabel);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGuiPopUpMessageBox::ButtonPress(iWidget* apWidget,const cGuiMessageData& aData)
	{
		int lButton = apWidget == mvButtons[0] ? 0 : 1;

		RunCallback(mpCallbackObject, mpCallback, apWidget, cGuiMessageData(lButton), true);

		SelfDestruct();
		
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpMessageBox,ButtonPress)

	
	//-----------------------------------------------------------------------

	bool cGuiPopUpMessageBox::GamepadButtonPress(iWidget* apWidget,const cGuiMessageData& aData)
	{
		if(!(aData.mlVal == eUIButton_Primary || aData.mlVal == eUIButton_Secondary)) return false;

		if(aData.mlVal == eUIButton_Secondary && mvButtons[1]) apWidget = mvButtons[1];

		return ButtonPress(apWidget, aData);
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpMessageBox,GamepadButtonPress)

	
	//-----------------------------------------------------------------------


}
