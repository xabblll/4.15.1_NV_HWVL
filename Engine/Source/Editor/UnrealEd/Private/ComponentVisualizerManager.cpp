// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ComponentVisualizerManager.h"

void FComponentVisualizerManager::HandleProxyForComponentVis(HHitProxy *HitProxy)
{
	if (HitProxy->IsA(HComponentVisProxy::StaticGetType()))
	{
		HComponentVisProxy* VisProxy = (HComponentVisProxy*)HitProxy;
		const UActorComponent* ClickedComponent = VisProxy->Component.Get();
		if (ClickedComponent != NULL)
		{
			TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(ClickedComponent->GetClass()->GetFName());
			if (Visualizer.IsValid())
			{
				bool bIsActive = Visualizer->VisProxyHandleClick(VisProxy);
				if (bIsActive)
				{
					// call EndEditing on any currently edited visualizer, if we are going to change it
					if (EditedVisualizer.IsValid() && Visualizer.Get() != EditedVisualizer.Pin().Get())
					{
						EditedVisualizer.Pin()->EndEditing();
					}

					EditedVisualizer = Visualizer;
				}
			}
		}
	}
	else
	{
		ClearActiveComponentVis();
	}
}

void FComponentVisualizerManager::ClearActiveComponentVis()
{
	if (EditedVisualizer.IsValid())
	{
		EditedVisualizer.Pin()->EndEditing();
		EditedVisualizer = NULL;
	}
}

bool FComponentVisualizerManager::HandleInputKey(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if(EditedVisualizer.IsValid())
	{
		if(EditedVisualizer.Pin()->HandleInputKey(ViewportClient, Viewport, Key, Event))
		{
			return true;
		}
	}

	return false;
}

bool FComponentVisualizerManager::HandleInputDelta(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (EditedVisualizer.IsValid() && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (EditedVisualizer.Pin()->HandleInputDelta(InViewportClient, InViewport, InDrag, InRot, InScale))
		{
			return true;
		}
	}

	return false;
}


bool FComponentVisualizerManager::GetWidgetLocation(FVector& OutLocation)
{
	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer.Pin()->GetWidgetLocation(OutLocation);
	}

	return false;
}
