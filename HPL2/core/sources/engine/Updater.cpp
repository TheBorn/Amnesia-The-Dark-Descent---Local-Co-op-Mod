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

#include "engine/Updater.h"

#include "engine/Updateable.h"

#include "impl/ImGuiDebugMenu.h"

#include <chrono>
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cUpdater::cUpdater(iLowLevelSystem *apLowLevelSystem)
	{
		mpCurrentUpdates = NULL;
		mpBackgroundUpdates = NULL;
		msCurrentUpdates = "";

		mpLowLevelSystem = apLowLevelSystem;
	}

	//-----------------------------------------------------------------------

	cUpdater::~cUpdater()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cUpdater::BroadcastMessageToAll(eUpdateableMessage aMessage, float afX)
	{
		for(tUpdateableListIt it = mlstGlobalUpdateableList.begin();it!=mlstGlobalUpdateableList.end();++it)
		{
			iUpdateable *pUpdateable = *it;
			pUpdateable->RunMessage(aMessage, afX);
		}

		tUpdateContainerMapIt contIt = m_mapUpdateContainer.begin();
		for(; contIt != m_mapUpdateContainer.end(); ++contIt)
		{
			tUpdateableList *pUpdateList = &contIt->second;
			
			for(tUpdateableListIt it = pUpdateList->begin();it!=pUpdateList->end();++it)
			{
				iUpdateable *pUpdateable = *it;
				pUpdateable->RunMessage(aMessage, afX);	
			}
		}
	}

	//-----------------------------------------------------------------------

	void cUpdater::RunMessage(eUpdateableMessage aMessage, float afX)
	{
		if(aMessage != eUpdateableMessage_Update)
		{
			for(tUpdateableListIt it = mlstGlobalUpdateableList.begin();it!=mlstGlobalUpdateableList.end();++it)
			{
				iUpdateable *pUpdateable = *it;
				pUpdateable->RunMessage(aMessage, afX);
			}

			if(mpCurrentUpdates)
			{
				tUpdateableList *pCurrentUpdateContainer = mpCurrentUpdates;
				for(tUpdateableListIt it = mpCurrentUpdates->begin();it!=mpCurrentUpdates->end();++it)
				{
					iUpdateable *pUpdateable = *it;
					pUpdateable->RunMessage(aMessage, afX);	
					
					//In case the container is change, do not do any more updating.
					if(mpCurrentUpdates != pCurrentUpdateContainer) break;;
				}
			}

			//Background container: the world carrying on under a menu. Skipped when it
			//IS the current container, so nothing is ever updated twice.
			if(mpBackgroundUpdates && mpBackgroundUpdates != mpCurrentUpdates)
			{
				tUpdateableList *pBackgroundContainer = mpBackgroundUpdates;
				for(tUpdateableListIt it = pBackgroundContainer->begin();it!=pBackgroundContainer->end();++it)
				{
					(*it)->RunMessage(aMessage, afX);

					if(mpBackgroundUpdates != pBackgroundContainer) break;
				}
			}
		}
		else
		{
			for(tUpdateableListIt it = mlstGlobalUpdateableList.begin();it!=mlstGlobalUpdateableList.end();++it)
			{
				iUpdateable *pUpdateable = *it;
				//Log("pUpdateable %d, ", pUpdateable);
				//Log("'%s'\n", pUpdateable->GetName().c_str());

				START_TIMING_EX(pUpdateable->GetName().c_str(),game)
				{
					std::chrono::high_resolution_clock::time_point profStart = std::chrono::high_resolution_clock::now();
					pUpdateable->RunMessage(aMessage, afX);
					std::chrono::duration<float, std::milli> profMs = std::chrono::high_resolution_clock::now() - profStart;
					::ImGuiDebugMenu::AddDiagModuleTime(pUpdateable->GetName().c_str(), profMs.count());
				}
				STOP_TIMING(game)
			}

			if(mpCurrentUpdates)
			{
				tUpdateableList *pCurrentUpdateContainer = mpCurrentUpdates;
				for(tUpdateableListIt it = mpCurrentUpdates->begin();it!=mpCurrentUpdates->end();++it)
				{
					iUpdateable *pUpdateable = *it;
					//Log("pUpdateable %d, ", pUpdateable);
					//Log("'%s'\n", pUpdateable->GetName().c_str());

					START_TIMING_EX(pUpdateable->GetName().c_str(),game)
					{
						std::chrono::high_resolution_clock::time_point profStart = std::chrono::high_resolution_clock::now();
						pUpdateable->RunMessage(aMessage, afX);
						std::chrono::duration<float, std::milli> profMs = std::chrono::high_resolution_clock::now() - profStart;
						::ImGuiDebugMenu::AddDiagModuleTime(pUpdateable->GetName().c_str(), profMs.count());
					}
					STOP_TIMING(game)
					
					//In case the container is change, do not do any more updating.
					if(mpCurrentUpdates != pCurrentUpdateContainer) break;;
				}
			}	

			//Background container: the world carrying on under a menu. Skipped when it
			//IS the current container, so nothing is ever updated twice.
			if(mpBackgroundUpdates && mpBackgroundUpdates != mpCurrentUpdates)
			{
				tUpdateableList *pBackgroundContainer = mpBackgroundUpdates;
				for(tUpdateableListIt it = pBackgroundContainer->begin();it!=pBackgroundContainer->end();++it)
				{
					(*it)->RunMessage(aMessage, afX);

					if(mpBackgroundUpdates != pBackgroundContainer) break;
				}
			}
		}
	}
	
	//-----------------------------------------------------------------------
	
	
	bool cUpdater::SetContainer(tString asContainer)
	{
		tUpdateContainerMapIt it = m_mapUpdateContainer.find(asContainer);
		if(it == m_mapUpdateContainer.end()) return false;

		if(&it->second == mpCurrentUpdates) return true;

		tString sOldContainer = msCurrentUpdates;

		/////////////////////////////////
		//Determine if the log updater should be active or not
		msCurrentUpdates = asContainer;
		/*if(msCurrentUpdates == "Default")
		{
			SetUpdateLogActive(true);
		}
		else
		{
			SetUpdateLogActive(false);
		}*/
		//SetUpdateLogActive(true);

		/////////////////////////////////
		// If was a previous container, send leave message
		if(mpCurrentUpdates)
		{
			for(tUpdateableListIt it = mpCurrentUpdates->begin(); it!=mpCurrentUpdates->end(); ++it)
			{
				iUpdateable *pUpdateable = *it;
				pUpdateable->OnLeaveContainer(asContainer);
			}
		}
		
		mpCurrentUpdates = &it->second;

		/////////////////////////////////
		// Apply the registered background for this container. Done HERE rather than
		// by the menus themselves: it is one assignment at the one moment the current
		// container changes, so the two can never disagree, whichever path got here.
		{
			tString sBackground = "";

			std::map<tString, tString>::iterator bgIt = m_mapContainerBackground.find(asContainer);
			if(bgIt != m_mapContainerBackground.end()) sBackground = bgIt->second;

			SetBackgroundContainer(sBackground);
		}

		/////////////////////////////////
		// Send enter message
		for(tUpdateableListIt it = mpCurrentUpdates->begin(); it!=mpCurrentUpdates->end(); ++it)
		{
			iUpdateable *pUpdateable = *it;
			pUpdateable->OnEnterContainer(sOldContainer);
		}
		
		return true;
	}

	//-----------------------------------------------------------------------

	tString cUpdater::GetCurrentContainerName()
	{
		if(mpCurrentUpdates==NULL) return "";

		return msCurrentUpdates;

	}
	
	//-----------------------------------------------------------------------

	bool cUpdater::AddContainer(tString asName)
	{
		//Create the value for the map with key and Updateable
		tUpdateContainerMap::value_type val = tUpdateContainerMap::value_type(
														asName, tUpdateableList());
		//Add it to the map
		m_mapUpdateContainer.insert(val);
		
		return true;
	}
	
	//-----------------------------------------------------------------------

	bool cUpdater::AddUpdate(tString asContainer, iUpdateable* apUpdate)
	{
		if(apUpdate==NULL){
			Error("Couldn't add NULL updatable!");
			return false;
		}

		//Search the map for the container name
		tUpdateContainerMapIt it = m_mapUpdateContainer.find(asContainer);
		if(it == m_mapUpdateContainer.end()) return false;
		
		//Add the updatable
		it->second.push_back(apUpdate);

		return true;
	}
	
	//-----------------------------------------------------------------------

	bool cUpdater::AddGlobalUpdate(iUpdateable* apUpdate)
	{
		mlstGlobalUpdateableList.push_back(apUpdate);
		return true;
	}

	//-----------------------------------------------------------------------

	void cUpdater::RunMessageOnContainer(const tString& asContainer, eUpdateableMessage aMessage, float afX)
	{
		tUpdateContainerMapIt it = m_mapUpdateContainer.find(asContainer);
		if(it == m_mapUpdateContainer.end()) return;

		tUpdateableList *pList = &it->second;
		if(pList == mpCurrentUpdates) return;	//already ran this frame

		tString sContainerAtStart = msCurrentUpdates;

		for(tUpdateableListIt listIt = pList->begin(); listIt != pList->end(); ++listIt)
		{
			(*listIt)->RunMessage(aMessage, afX);

			//Something in here switched container (a map change, a level door).
			//Stop touching a list that may no longer be the right one.
			if(msCurrentUpdates != sContainerAtStart) break;
		}
	}

	//-----------------------------------------------------------------------

	bool cUpdater::SetContainerBackground(const tString& asContainer, const tString& asBackground)
	{
		if(asBackground == "")
			m_mapContainerBackground.erase(asContainer);
		else
			m_mapContainerBackground[asContainer] = asBackground;

		//Take effect immediately if that container is already the current one.
		if(msCurrentUpdates == asContainer)
			SetBackgroundContainer(asBackground);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cUpdater::SetBackgroundContainer(const tString& asContainer)
	{
		if(asContainer == "")
		{
			msBackgroundUpdates = "";
			mpBackgroundUpdates = NULL;
			return true;
		}

		tUpdateContainerMapIt it = m_mapUpdateContainer.find(asContainer);
		if(it == m_mapUpdateContainer.end()) return false;

		msBackgroundUpdates = asContainer;
		mpBackgroundUpdates = &it->second;

		return true;
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	

	//-----------------------------------------------------------------------
}
