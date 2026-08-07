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

#ifndef HPL_UPDATER_H
#define HPL_UPDATER_H

#include <map>
#include <list>

#include "engine/EngineTypes.h"
#include "system/SystemTypes.h"

namespace hpl {

	class iUpdateable;
	class iLowLevelSystem;

	typedef std::list<iUpdateable*> tUpdateableList;
	typedef tUpdateableList::iterator tUpdateableListIt;

	typedef std::map<tString, tUpdateableList> tUpdateContainerMap;
	typedef tUpdateContainerMap::iterator tUpdateContainerMapIt;

	class cUpdater
	{
	public:
		cUpdater(iLowLevelSystem *apLowLevelSystem);
		~cUpdater();

		void Reset();
		
		void BroadcastMessageToAll(eUpdateableMessage aMessage, float afX=0);
		void RunMessage(eUpdateableMessage aMessage, float afX=0);
        
		/**
		 * Sets the active update container to be used.
		 * \param asContainer Name of the contianer
		 * \return 
		 */
		bool SetContainer(tString asContainer);

		/**
		 * Gets the name of the current container in use.
		 * \return name of current container.
		 */
		tString GetCurrentContainerName();
		/**
		 * Adds a new container
		 * \todo change name to state instead of container?
		 * \param asName Name for the new container.
		 * \return 
		 */
		bool AddContainer(tString asName);
		/**
		 * Adds a new update in a container.
		 * \param asContainer Container name
		 * \param apUpdate pointer to the class that will be updated
		 * \return 
		 */
		bool AddUpdate(tString asContainer, iUpdateable* apUpdate);
		/**
		 * Adds a global update that runs no matter what container is set
		 * \param apUpdate 
		 * \return 
		 */
		bool AddGlobalUpdate(iUpdateable* apUpdate);

		/**
		 * A second container that keeps updating UNDERNEATH the current one.
		 *
		 * Coop: opening a menu switches container, which is what stops the world --
		 * and stopped it for BOTH players. Running "Default" in the background lets
		 * the other player carry on while one is in their bag.
		 *
		 * Deliberately does NOT send enter/leave messages: the background container
		 * never left, so telling its modules otherwise would undo the very viewport
		 * and state handling the switch just did. Pass "" to clear.
		 */
		bool SetBackgroundContainer(const tString& asContainer);
		/**
		 * Register that entering asContainer should keep asBackground running.
		 *
		 * SetContainer applies this itself, so the background can never end up out of
		 * step with the current container -- which is what happens when each menu
		 * sets it by hand from OnEnterContainer and one code path forgets. Pass "" as
		 * the background to unregister.
		 */
		bool SetContainerBackground(const tString& asContainer, const tString& asBackground);

		/**
		 * Run one message across a container that is NOT the current one.
		 *
		 * Direct and unconditional -- no flags, no registration, no ordering. The
		 * caller is a module that is already running, so if the caller runs, this
		 * runs. That is the whole point: the background-container path depends on
		 * state being correct at a moment nobody observes, and it was not.
		 */
		void RunMessageOnContainer(const tString& asContainer, eUpdateableMessage aMessage, float afX=0);
		tString GetBackgroundContainerName(){ return msBackgroundUpdates; }
	
	private:
		tString msCurrentUpdates;
		tString msBackgroundUpdates;

        tUpdateContainerMap m_mapUpdateContainer;
		std::map<tString, tString> m_mapContainerBackground;

		iLowLevelSystem *mpLowLevelSystem;
		
		tUpdateableList *mpCurrentUpdates;
		tUpdateableList *mpBackgroundUpdates;
		tUpdateableList mlstGlobalUpdateableList;
	};
};
#endif // HPL_UPDATER_H
