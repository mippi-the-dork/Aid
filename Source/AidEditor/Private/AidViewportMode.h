#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

/** Minimal overlay proof: notes and straight connectors are rendered without any map actors. */
class FAidViewportMode final : public FEdMode
{
public:
    static const FEditorModeID ModeId;

    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport,
        const FSceneView* View, FCanvas* Canvas) override;
};
