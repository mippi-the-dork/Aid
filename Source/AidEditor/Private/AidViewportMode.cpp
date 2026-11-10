#include "AidViewportMode.h"
#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PrimitiveDrawInterface.h"
#include "SceneView.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "InputCoreTypes.h"

const FEditorModeID FAidViewportMode::ModeId = TEXT("EM_Aid");

namespace
{
    constexpr float MarkerRadius = 9.f;
    constexpr float LineCapGap = 16.f;

    void CanvasLine(FCanvas* Canvas, FVector2D A, FVector2D B, FLinearColor Color, float Width = 1.f)
    {
        FCanvasLineItem L(A, B);
        L.SetColor(Color);
        L.LineThickness = Width;
        Canvas->DrawItem(L);
    }

    void DrawRing(FCanvas* Canvas, FVector2D Center, FLinearColor Fill, FLinearColor Border, float Radius)
    {
        constexpr int32 Segments = 24;
        // Filled dot via nested outlines, no material or texture needed.
        for (int32 R = 1; R <= 6; ++R)
        {
            const float Inner = Radius * (static_cast<float>(R) / 9.f);
            for (int32 I = 0; I < Segments; ++I)
            {
                const float A = 2.f * PI * I / Segments;
                const float B = 2.f * PI * (I + 1) / Segments;
                CanvasLine(Canvas, Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Inner,
                    Center + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Inner, Fill, 1.5f);
            }
        }
        for (int32 I = 0; I < Segments; ++I)
        {
            const float A = 2.f * PI * I / Segments;
            const float B = 2.f * PI * (I + 1) / Segments;
            CanvasLine(Canvas, Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius,
                Center + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius, Border, 2.f);
        }
    }

    void DrawCap(FCanvas* Canvas, EAidCapStyle Cap, FVector2D At, FVector2D Travel, FLinearColor Color, float Width)
    {
        if (Cap == EAidCapStyle::None) return;
        const FVector2D Side(-Travel.Y, Travel.X);
        if (Cap == EAidCapStyle::Arrowhead)
        {
            CanvasLine(Canvas, At - Travel * 12.f + Side * 6.f, At, Color, Width);
            CanvasLine(Canvas, At - Travel * 12.f - Side * 6.f, At, Color, Width);
        }
        else if (Cap == EAidCapStyle::Circle)
        {
            DrawRing(Canvas, At, Color, Color, 5.f);
        }
        else if (Cap == EAidCapStyle::Square)
        {
            FCanvasTileItem Tile(At - FVector2D(5.f, 5.f), FVector2D(10.f), Color);
            Tile.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Tile);
        }
    }

    bool IsForward(const FVector& Camera, const FVector& Forward, const FVector& Point)
    {
        return FVector::DotProduct(Forward, Point - Camera) > 1.;
    }
}

void FAidViewportMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
    FEdMode::Render(View, Viewport, PDI);
    if (!GEditor || !View || !PDI) return;
    const UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    if (!State || !State->bViewportVisible) return;
    const FVector Camera = View->ViewLocation;
    for (const UAidDocument* Doc : State->VisibleDocuments)
    {
        if (!IsValid(Doc)) continue;
        TMap<FGuid, FVector> Positions;
        Doc->BuildResolvedPositions(GetWorld(), Positions);
        for (const FAidConnection& Edge : Doc->Connections)
        {
            const FAidNode* A = Doc->FindNode(Edge.SourceNode);
            const FAidNode* B = Doc->FindNode(Edge.TargetNode);
            if (!A || !B) continue;
            const FVector Start = Positions.FindRef(A->Id);
            const FVector End = Positions.FindRef(B->Id);
            if (!UAidDocument::IsConnectionVisible(Edge, *A, *B, Camera, Start, End)) continue;
            const float Length = FVector::Distance(Start, End);
            if (Length < 1.f) continue;
            // Keep world-space lines outside the radial markers. The screen-space
            // cap overlay draws at the same approximate inset distance.
            FVector DrawFrom = Start;
            FVector DrawTo = End;
            FVector2D ScreenFrom, ScreenTo;
            if (View->WorldToPixel(Start, ScreenFrom) && View->WorldToPixel(End, ScreenTo))
            {
                const float PixelLength = FVector2D::Distance(ScreenFrom, ScreenTo);
                if (PixelLength > 35.f)
                {
                    const float Fraction = FMath::Min(0.3f, LineCapGap / PixelLength);
                    DrawFrom = FMath::Lerp(Start, End, Fraction);
                    DrawTo = FMath::Lerp(Start, End, 1.f - Fraction);
                }
            }
            if (Edge.LineStyle == EAidLineStyle::Solid)
            {
                PDI->DrawLine(DrawFrom, DrawTo, Edge.Color, SDPG_Foreground, Edge.Thickness);
            }
            else
            {
                const float Pattern = Edge.LineStyle == EAidLineStyle::Dashed ? 120.f : 52.f;
                const int32 Count = FMath::Clamp(FMath::CeilToInt(FVector::Distance(DrawFrom, DrawTo) / Pattern), 1, 512);
                for (int32 I = 0; I < Count; ++I)
                {
                    const float T0 = static_cast<float>(I) / Count;
                    const float T1 = FMath::Min(1.f, T0 + (Edge.LineStyle == EAidLineStyle::Dashed ? 0.57f : 0.17f) / Count);
                    PDI->DrawLine(FMath::Lerp(DrawFrom, DrawTo, T0), FMath::Lerp(DrawFrom, DrawTo, T1),
                        Edge.Color, SDPG_Foreground, Edge.Thickness);
                }
            }
        }
    }
}

void FAidViewportMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport,
    const FSceneView* View, FCanvas* Canvas)
{
    FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
    if (!Viewport) return;
    TArray<FAidCardHit>& Regions = HitRegions.FindOrAdd(Viewport);
    Regions.Reset();
    if (!GEditor || !ViewportClient || !View || !Canvas || !GEngine) return;
    const UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    if (!State || !State->bViewportVisible) return;

    const FVector Camera = ViewportClient->GetViewLocation();
    const FVector Forward = ViewportClient->GetViewRotation().Vector();
    for (UAidDocument* Doc : State->VisibleDocuments)
    {
        if (!IsValid(Doc)) continue;
        TMap<FGuid, FVector> Locations;
        Doc->BuildResolvedPositions(ViewportClient->GetWorld(), Locations);
        // Render caps in screen space so their sizes remain readable over long lines.
        for (const FAidConnection& Edge : Doc->Connections)
        {
            const FAidNode* A = Doc->FindNode(Edge.SourceNode);
            const FAidNode* B = Doc->FindNode(Edge.TargetNode);
            if (!A || !B) continue;
            const FVector From = Locations.FindRef(A->Id);
            const FVector To = Locations.FindRef(B->Id);
            if (!UAidDocument::IsConnectionVisible(Edge, *A, *B, Camera, From, To)) continue;
            if (!IsForward(Camera, Forward, From) || !IsForward(Camera, Forward, To)) continue;
            FVector2D P0, P1;
            if (!View->WorldToPixel(From, P0) || !View->WorldToPixel(To, P1)) continue;
            const FVector2D Offset = P1 - P0;
            if (Offset.SizeSquared() < 1.f) continue;
            const FVector2D Direction = Offset.GetSafeNormal();
            // A 16 px inset keeps caps distinct from each node's 9 px ring.
            DrawCap(Canvas, Edge.StartCap, P0 + Direction * LineCapGap, -Direction, Edge.Color, Edge.Thickness);
            DrawCap(Canvas, Edge.EndCap, P1 - Direction * LineCapGap, Direction, Edge.Color, Edge.Thickness);
            if (!Edge.Label.IsEmpty())
            {
                const FVector2D Mid = (P0 + P1) * 0.5f;
                FString Label = Edge.Label.ToString();
                if (Label.Len() > 38) Label = Label.Left(35) + TEXT("...");
                const float Width = FMath::Clamp(30.f + Label.Len() * 7.f, 80.f, 295.f);
                FCanvasTileItem Backing(Mid - FVector2D(4.f, 3.f), FVector2D(Width, 22.f),
                    FLinearColor(0.025f, 0.06f, 0.08f, 0.90f));
                Backing.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(Backing);
                FCanvasTextItem Text(Mid, FText::FromString(Label), GEngine->GetSmallFont(), Edge.Color);
                Canvas->DrawItem(Text);
            }
        }

        for (const FAidNode& Node : Doc->Nodes)
        {
            const FVector Position = Locations.FindRef(Node.Id);
            const uint8 StateOfNote = UAidDocument::GetNoteVisibility(Node, Camera, Position);
            if (!StateOfNote || !IsForward(Camera, Forward, Position)) continue;
            FVector2D P;
            if (!View->WorldToPixel(Position, P)) continue;
            if (P.X < -350.f || P.Y < -200.f || P.X > Viewport->GetSizeXY().X + 100.f || P.Y > Viewport->GetSizeXY().Y + 200.f) continue;
            const bool bSelected = Doc == State->ActiveDocument && Node.Id == State->SelectedNodeId;
            const FLinearColor Ring = Node.Style.GetBorderColor();
            DrawRing(Canvas, P, Node.Style.HeaderBackgroundColor, Ring, MarkerRadius);
            if (bSelected) DrawRing(Canvas, P, Node.Style.HeaderBackgroundColor, FLinearColor::Yellow, 12.f);
            FAidCardHit MarkerHit;
            MarkerHit.Document = Doc;
            MarkerHit.NodeId = Node.Id;
            MarkerHit.TopLeft = P - FVector2D(14.f, 14.f);
            MarkerHit.BottomRight = P + FVector2D(14.f, 14.f);
            Regions.Add(MarkerHit);
            if (StateOfNote == 1) continue;

            const float FontScale = FMath::Clamp(Node.Style.FontSize / 14.f, 0.7f, 3.f);
            const float LineHeight = 18.f * FontScale;
            const float Width = FMath::Max(218.f, 218.f * FMath::Min(FontScale, 1.75f));
            const float CardHeight = 18.f + LineHeight * 2.f + 13.f;
            const FVector2D TopLeft = P + FVector2D(32.f, -23.f);
            const FVector2D Size(Width, CardHeight);
            if (TopLeft.X + Size.X < 0 || TopLeft.Y + Size.Y < 0 ||
                TopLeft.X > Viewport->GetSizeXY().X || TopLeft.Y > Viewport->GetSizeXY().Y) continue;
            const float Border = FMath::Clamp(Node.Style.BorderThickness, 1.f, 12.f);
            FCanvasTileItem Frame(TopLeft, Size, bSelected ? FLinearColor::Yellow : Ring);
            Frame.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Frame);
            FCanvasTileItem Header(TopLeft + FVector2D(Border, Border), FVector2D(Size.X - Border * 2.f, LineHeight + 12.f),
                Node.Style.HeaderBackgroundColor);
            Header.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Header);
            FCanvasTileItem Body(TopLeft + FVector2D(Border, Border + LineHeight + 12.f),
                FVector2D(Size.X - Border * 2.f, Size.Y - LineHeight - 12.f - Border * 2.f), Node.Style.BodyBackgroundColor);
            Body.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Body);
            FString Title = Node.Title.ToString();
            if (Title.Len() > 32) Title = Title.Left(29) + TEXT("...");
            FCanvasTextItem TitleItem(TopLeft + FVector2D(8.f, 5.f), FText::FromString(Title), GEngine->GetSmallFont(), Node.Style.HeaderTextColor);
            TitleItem.Scale = FVector2D(FontScale, FontScale);
            TitleItem.EnableShadow(FLinearColor::Black, FVector2D(1.f, 1.f));
            Canvas->DrawItem(TitleItem);
            FString BodyText = Node.Body.ToString().Replace(TEXT("\n"), TEXT(" "));
            if (BodyText.IsEmpty()) BodyText = TEXT("(No description)");
            if (BodyText.Len() > 37) BodyText = BodyText.Left(34) + TEXT("...");
            FCanvasTextItem BodyItem(TopLeft + FVector2D(8.f, 16.f + LineHeight), FText::FromString(BodyText),
                GEngine->GetSmallFont(), Node.Style.BodyTextColor);
            BodyItem.Scale = FVector2D(FontScale, FontScale);
            Canvas->DrawItem(BodyItem);
            FAidCardHit CardHit;
            CardHit.Document = Doc;
            CardHit.NodeId = Node.Id;
            CardHit.TopLeft = TopLeft;
            CardHit.BottomRight = TopLeft + Size;
            Regions.Add(CardHit);
        }
    }
}

bool FAidViewportMode::InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event)
{
    if (!GEditor || !Client || !Viewport) return FEdMode::InputKey(Client, Viewport, Key, Event);
    UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    if (!State || !State->ActiveDocument || !State->bViewportVisible || !State->bEditMode)
        return FEdMode::InputKey(Client, Viewport, Key, Event);
    if (Key == EKeys::Escape && Event == IE_Pressed &&
        (State->bPlaceNoteOnClick || State->PendingConnectionSource.IsValid()))
    {
        State->CancelPendingAction();
        return true;
    }
    if (Key != EKeys::LeftMouseButton || Event != IE_Pressed ||
        Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt) ||
        Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl))
        return FEdMode::InputKey(Client, Viewport, Key, Event);

    const FVector2D Mouse(static_cast<float>(Viewport->GetMouseX()), static_cast<float>(Viewport->GetMouseY()));
    if (!State->bPlaceNoteOnClick)
    {
        if (const TArray<FAidCardHit>* Hits = HitRegions.Find(Viewport))
        {
            for (int32 Index = Hits->Num() - 1; Index >= 0; --Index)
            {
                const FAidCardHit& H = (*Hits)[Index];
                if (Mouse.X >= H.TopLeft.X && Mouse.Y >= H.TopLeft.Y && Mouse.X <= H.BottomRight.X && Mouse.Y <= H.BottomRight.Y)
                {
                    if (H.Document.IsValid() && State->ActiveDocument != H.Document.Get()) State->SetActiveDocument(H.Document.Get());
                    State->SelectNode(H.NodeId);
                    return true;
                }
            }
        }
    }
    if (State->bPlaceNoteOnClick)
    {
        const FViewportCursorLocation Ray = Client->GetCursorWorldLocationFromMousePos();
        const FVector From = Ray.GetOrigin();
        const FVector Direction = Ray.GetDirection();
        FVector Position = From + Direction * 600.f;
        FHitResult Hit;
        bool bHit = false;
        if (UWorld* World = Client->GetWorld())
        {
            FCollisionQueryParams Params;
            Params.bTraceComplex = true;
            bHit = World->LineTraceSingleByChannel(Hit, From, From + Direction * 100000.f, ECC_Visibility, Params);
            if (bHit) Position = Hit.ImpactPoint + Hit.ImpactNormal * 10.f;
        }
        if (State->bPlaceOnActor)
        {
            if (bHit && IsValid(Hit.GetActor())) State->AddNoteAttachedToActor(Hit.GetActor());
            // Keep placement armed if the click was on empty space.
        }
        else State->AddNoteAt(Position);
        return true;
    }
    return FEdMode::InputKey(Client, Viewport, Key, Event);
}
