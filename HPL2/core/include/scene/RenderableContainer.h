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

#ifndef HPL_RENDERABLE_CONTAINER_H
#define HPL_RENDERABLE_CONTAINER_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/SceneTypes.h"

namespace hpl {

	//-------------------------------------------

	class iRenderableContainerNode;
	class cRendererCallbackFunctions;
	class iRenderable;
	class cFrustum;

	//-------------------------------------------
	
	class cVisibleRCNodeTracker
	{
	public:
		cVisibleRCNodeTracker();

		void SwitchAndClearVisibleNodeSet();
		void SetNodeVisible(iRenderableContainerNode *apNode);
		bool WasNodeVisible(iRenderableContainerNode *apNode);

		void Reset();

		/**
		 * Throw the remembered nodes away if any have been destroyed since this
		 * tracker last looked. Call at the start of a cull pass.
		 *
		 * The sets below hold RAW POINTERS to container nodes, and the dynamic
		 * container deletes every node it owns whenever it rebuilds its tree --
		 * which it does on a counter, after so many objects have been added or
		 * removed, so in practice at unpredictable moments during play. Nothing
		 * told the trackers, and the only Reset() there was reached solely from
		 * cViewport::SetWorld, i.e. once per map load.
		 *
		 * So WasNodeVisible was being asked about freed pointers. Sometimes that
		 * merely answers "no". Sometimes the allocator has handed the same address
		 * back out for a NEW node, and it answers "yes" for something never
		 * actually seen -- CHC then trusts last frame's visibility, skips the
		 * draw, and issues an occlusion query instead. Geometry vanishes for a
		 * frame or several and comes back on its own once the sets refill.
		 *
		 * Single player mostly gets away with it: one viewport, and the rebuild
		 * happens inside its own cull pass, so its sets are refilled immediately.
		 * Co-op does not. There are two viewports, each with its own tracker, and
		 * UpdateBeforeRendering runs at the top of EACH cull pass -- so the
		 * viewport that culls second meets a tree that the first one just rebuilt,
		 * holding a whole set of pointers into freed memory. That is one viewport
		 * going black at random intervals while the other is fine.
		 */
		void ValidateAgainstNodeDestruction();

	private:
		tRenderableContainerNodeSet m_setVisibleNodes[2];
		int mlCurrentVisibleNodeSet;
		int mlFrameCounter;

		int mlNodeDestroyCountAtLastUse;
	};

	//-------------------------------------------
	
	class cRenderableContainerObjectCallback : public iRenderableCallback
	{
	public:
		cRenderableContainerObjectCallback();

		void OnVisibleChange(iRenderable *apObject);
		void OnRenderFlagsChange(iRenderable *apObject);
	};

	//-------------------------------------------

	class iRenderableContainerNode
	{
	friend class iRenderableContainer;
	public:
		iRenderableContainerNode();

		//Counted rather than announced. Nodes are destroyed from several places --
		//a full tree rebuild, a split collapsing, the destructor chain -- and every
		//one of them leaves any cVisibleRCNodeTracker holding that pointer stale.
		//Counting in the one place they all pass through means no path can be
		//missed, and a tracker only has to compare one integer to know.
		virtual ~iRenderableContainerNode(){ ++mlNodeDestroyCount; }

		static int GetNodeDestroyCount(){ return mlNodeDestroyCount; }

		virtual void UpdateBeforeUse(){}

		inline tRenderableContainerNodeList* GetChildNodeList(){ return &mlstChildNodes; }
		inline bool HasChildNodes(){ return mlstChildNodes.empty() == false; }

		inline tRenderableList* GetObjectList() { return &mlstObjects; }
		inline bool HasObjects() { return mlstObjects.empty() == false; }

		inline iRenderableContainerNode* GetParent(){ return mpParent;}
		inline void SetParent(iRenderableContainerNode* apParent){ mpParent = apParent;}

		inline int GetObjectNum(){ return (int)mlstObjects.size();}
		
		inline const cVector3f& GetMin() const{ return mvMin;}
		inline const cVector3f& GetMax() const{ return mvMax;}

		inline const cVector3f GetCenter() const{ return mvCenter;}
		inline float GetRadius() const { return mfRadius;}

		inline float GetViewDistance()const{ return mfViewDistance;}
		inline void SetViewDistance(float afX){ mfViewDistance = afX;}

		inline bool IsInsideView() const{ return mbInsideView;}
		inline void SetInsideView(bool abX) { mbInsideView = abX;}

		inline bool UsesFlagsAndVisibility() { return mbUsesFlagsAndVisibility;}

		inline tRenderableFlag GetRenderFlags() const { return mlRenderFlags;}
		inline bool HasVisibleObjects() const { return mbVisibleObjects;}

		inline void SetRenderFlags(tRenderableFlag alFlags) { mlRenderFlags = alFlags;}
		inline void SetHasVisibleObjects(bool abX) { mbVisibleObjects = abX;}

		inline void SetNeedPropertyUpdate(bool abX){ mbNeedPropertyUpdate = abX;}
		inline bool GetNeedPropertyUpdate() const { return mbNeedPropertyUpdate;}

		void PushUpNeedAABBUpdate();
		inline bool GetNeedAABBUpdate() const { return mbNeedAABBUpdate;}

		inline void SetPrevFrustumCollision(eCollision aX){ mPrevFrustumCollision = aX;}
		inline eCollision GetPrevFrustumCollision() const { return mPrevFrustumCollision;}

		void CalculateMinMaxFromObjects();
	
	protected:
		cVector3f mvMin;
		cVector3f mvMax;
		float mfRadius;
		cVector3f mvCenter;

		tRenderableFlag mlRenderFlags;
		bool mbVisibleObjects;

		bool mbNeedPropertyUpdate;
		bool mbNeedAABBUpdate;

		bool mbUsesFlagsAndVisibility;
        
		//Temp structures
		float mfViewDistance;
		bool mbInsideView;
		eCollision mPrevFrustumCollision;

		iRenderableContainerNode *mpParent;
		tRenderableContainerNodeList mlstChildNodes;
		tRenderableList mlstObjects;

		static int mlNodeDestroyCount;
	};

	//-------------------------------------------
	
	class iRenderableContainer
	{
	public:
		virtual ~iRenderableContainer(){}

		void UpdateBeforeRendering();

		virtual void Add(iRenderable *apRenderable)=0;
		virtual void Remove(iRenderable *apRenderable)=0;

		virtual iRenderableContainerNode* GetRoot()=0;

        /**
         * This compiles the container. Even if the container is static, it should be possible to change orientation (scale, pos, rotation,radius etc) of added
		 * objects before this method is called. After compile is called, objects orientation can not be changed!
         */
        virtual void Compile()=0;

		virtual void RenderDebug(cRendererCallbackFunctions *apFunctions)=0;

	private:
		void CheckNeedPropertyUpdateIteration(iRenderableContainerNode* apNode);
		void CheckNeedAABBUpdateIteration(iRenderableContainerNode* apNode);

		virtual void SpecificUpdateBeforeRendering(){}
	};

	//-------------------------------------------
};
#endif // RENDERABLE_CONTAINER
