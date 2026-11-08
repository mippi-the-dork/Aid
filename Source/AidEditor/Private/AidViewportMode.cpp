#include "AidViewportMode.h"
#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PrimitiveDrawInterface.h"
#include "SceneView.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"

const FEditorModeID FAidViewportMode::ModeId = TEXT("EM_Aid" );

void FAidViewportMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
    FEdMode::Render(View, Viewport, PDI);
    if (!GEditor || !View || !PDI)
    {
        return;
    }
    const UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    const UAidDocument* Doc = State ? State->ActiveDocument.Get() : nullptr;
    if (!State || !State->bViewportVisible || !Doc)
    {
        return;
    }

    const UWorld* World = GetWorld();
    TMap<FGuid, FVector> Locations;
    Doc->BuildResolvedPositions(World, Locations);
    for (const FAidConnection& Edge : Doc->Connections)
    {
        const FAidNode* Start = Doc->FindNode(Edge.SourceNode);
        const FAidNode* End = Doc->FindNode(Edge.TargetNode);
        if (!Edge.bVisible || !Start || !End || !Start->bVisible || !End->bVisible)
        {
            continue;
        }
        // First iteration draws a straight line. Style metadata is saved but custom caps/dashes are pending.
        PDI->DrawLine(Locations.FindRef(Start->Id), Locations.FindRef(End->Id),
            Edge.Color, SDPG_Foreground, Edge.Thickness);
    }

    for (const FAidNode& Node : Doc->Nodes)
    {
        if (Node.bVisible)
        {
            PDI->DrawPoint(Locations.FindRef(Node.Id), Node.Style.BorderColor, 14.f, SDPG_Foreground);
        }
    }
}

void FAidViewportMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport,
    const FSceneView* View, FCanvas* Canvas)
{
    FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
    if (!GEditor || !View || !Canvas || !GEngine)
    {
        return;
    }
    const UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    const UAidDocument* Doc = State ? State->ActiveDocument.Get() : nullptr;
    if (!State || !State->bViewportVisible || !Doc)
    {
        return;
    }

    const UWorld* World = ViewportClient ? ViewportClient->GetWorld() : GetWorld();
    TMap<FGuid, FVector> Locations;
    Doc->BuildResolvedPositions(World, Locations);
    for (const FAidNode& Node : Doc->Nodes)
    {
        if (!Node.bVisible)
        {
            continue;
        }
        const FVector Location = Locations.FindRef(Node.Id);
        FVector2D ScreenPosition;
        if (!View->WorldToPixel(Location, ScreenPosition))
        {
            continue;
        }
        const FLinearColor TitleColor = Node.Style.HeaderTextColor;
        FCanvasTextItem Item(ScreenPosition + FVector2D(14., -8.), Node.Title, GEngine->GetSmallFont(), TitleColor);
        Item.EnableShadow(FLinearColor::Black, FVector2D(1., 1.));
        Canvas->DrawItem(Item);
    }
}
