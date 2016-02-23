// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayPrivate.h"
#include "PrimitiveSceneInfo.h"

void IHeadMountedDisplay::GatherLateUpdatePrimitives(USceneComponent* Component, TArray<LateUpdatePrimitiveInfo>& Primitives)
{
	// If a scene proxy is present, cache it
	UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveComponent->SceneProxy->GetPrimitiveSceneInfo();
		if (PrimitiveSceneInfo)
		{
			LateUpdatePrimitiveInfo PrimitiveInfo;
			PrimitiveInfo.IndexAddress = PrimitiveSceneInfo->GetIndexAddress();
			PrimitiveInfo.SceneInfo = PrimitiveSceneInfo;
			Primitives.Add(PrimitiveInfo);
		}
	}

	// Gather children proxies
	const int32 ChildCount = Component->GetNumChildrenComponents();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		USceneComponent* ChildComponent = Component->GetChildComponent(ChildIndex);
		if (!ChildComponent)
		{
			continue;
		}

		GatherLateUpdatePrimitives(ChildComponent, Primitives);
	}
}

/**
* HMD device console vars
*/
static TAutoConsoleVariable<int32> CVarHiddenAreaMask(
	TEXT("vr.HiddenAreaMask"),
	1,
	TEXT("0 to disable hidden area mask, 1 to enable."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

class FHeadMountedDisplayModule : public IHeadMountedDisplayModule
{
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay()
	{
		TSharedPtr<IHeadMountedDisplay, ESPMode::ThreadSafe> DummyVal = NULL;
		return DummyVal;
	}

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("Default"));
	}
};

IMPLEMENT_MODULE( FHeadMountedDisplayModule, HeadMountedDisplay );

IHeadMountedDisplay::IHeadMountedDisplay()
{
	PreFullScreenRect = FSlateRect(-1.f, -1.f, -1.f, -1.f);
}

void IHeadMountedDisplay::PushPreFullScreenRect(const FSlateRect& InPreFullScreenRect)
{
	PreFullScreenRect = InPreFullScreenRect;
}

void IHeadMountedDisplay::PopPreFullScreenRect(FSlateRect& OutPreFullScreenRect)
{
	OutPreFullScreenRect = PreFullScreenRect;
	PreFullScreenRect = FSlateRect(-1.f, -1.f, -1.f, -1.f);
}

void IHeadMountedDisplay::SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component)
{
	LateUpdateParentToWorld = ParentToWorld;
	LateUpdatePrimitives.Reset();
	GatherLateUpdatePrimitives(Component, LateUpdatePrimitives);
}

void IHeadMountedDisplay::ApplyLateUpdate(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform)
{
	if (!LateUpdatePrimitives.Num())
	{
		return;
	}

	const FTransform OldCameraTransform = OldRelativeTransform * LateUpdateParentToWorld;
	const FTransform NewCameraTransform = NewRelativeTransform * LateUpdateParentToWorld;
	const FMatrix LateUpdateTransform = (OldCameraTransform.Inverse() * NewCameraTransform).ToMatrixWithScale();

	// Apply delta to the affected scene proxies
	for (auto PrimitiveInfo : LateUpdatePrimitives)
	{
		FPrimitiveSceneInfo* RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(*PrimitiveInfo.IndexAddress);
		FPrimitiveSceneInfo* CachedSceneInfo = PrimitiveInfo.SceneInfo;
		// If the retrieved scene info is different than our cached scene info then the primitive was removed from the scene
		if (CachedSceneInfo == RetrievedSceneInfo && CachedSceneInfo->Proxy)
		{
			CachedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
		}
	}
	LateUpdatePrimitives.Reset();
}