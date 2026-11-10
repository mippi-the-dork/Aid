#pragma once
#include "CoreMinimal.h"
#include "EdMode.h"

class UAidDocument;

/** Draw-only viewport overlay. Notes remain stored in Aid Documents, never as level actors. */
class FAidViewportMode final : public FEdMode
{
public:
    static const FEditorModeID ModeId;
    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport,
        const FSceneView* View, FCanvas* Canvas) override;
    virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport,
        FKey Key, EInputEvent Event) override;
private:
    struct FAidCardHit
    {
        TWeakObjectPtr<UAidDocument> Document;
        FGuid NodeId;
        FVector2D TopLeft;
        FVector2D BottomRight;
    };
    TMap<FViewport*, TArray<FAidCardHit>> HitRegions;
};
