#include "AidViewportMode.h"
#include "AidViewportGeometry.h"
#include "AidShowFlag.h"
#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "AidProjectSettings.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "PrimitiveDrawInterface.h"
#include "SceneView.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "GenericPlatform/ICursor.h"

const FEditorModeID FAidViewportMode::ModeId = TEXT("EM_Aid");

namespace
{
    constexpr float MarkerRadius = 9.f;
    constexpr float LineCapGap = 16.f;
    FLinearColor SelectionColor() { return FAppStyle::Get().GetSlateColor("Colors.AccentBlue").GetSpecifiedColor(); }

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

    bool IsOnScreen(const FVector2D& P, const FIntPoint& Size, float Margin = 20.f)
    {
        return P.X >= -Margin && P.Y >= -Margin && P.X <= Size.X + Margin && P.Y <= Size.Y + Margin;
    }

    /** Wrap against the *actual* canvas font metrics rather than a guessed character count.
        Retains every authored line; the card calculates its height after wrapping. */
    TArray<FString> WrapLines(const FString& Source, float AvailableWidth, float Scale)
    {
        TArray<FString> Result;
        const UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
        if (!Font) { Result.Add(Source); return Result; }
        const auto Fits = [Font, AvailableWidth, Scale](const FString& Text)
        {
            return Font->GetStringSize(*Text) * Scale <= AvailableWidth;
        };
        FString Normal = Source.Replace(TEXT("\r"), TEXT(""));
        TArray<FString> Paragraphs;
        Normal.ParseIntoArray(Paragraphs, TEXT("\n"), false);
        if (Paragraphs.IsEmpty()) Paragraphs.Add(TEXT(""));
        for (const FString& Paragraph : Paragraphs)
        {
            TArray<FString> Words;
            Paragraph.ParseIntoArrayWS(Words);
            FString Pending;
            if (Words.IsEmpty()) { Result.Add(TEXT("")); continue; }
            for (const FString& Word : Words)
            {
                FString Remaining = Word;
                while (!Remaining.IsEmpty())
                {
                    const FString Candidate = Pending.IsEmpty() ? Remaining : Pending + TEXT(" ") + Remaining;
                    if (Fits(Candidate)) { Pending = Candidate; break; }
                    if (!Pending.IsEmpty()) { Result.Add(Pending); Pending.Empty(); continue; }
                    // One unusually long word: split by measured glyph width, not by a hard char limit.
                    int32 Take = 1;
                    while (Take < Remaining.Len() && Fits(Remaining.Left(Take + 1))) ++Take;
                    Result.Add(Remaining.Left(Take));
                    Remaining.RightChopInline(Take);
                }
            }
            if (!Pending.IsEmpty()) Result.Add(Pending);
        }
        return Result;
    }

    /** Project the complete visible world segment, even when one endpoint is behind the camera.
        The near-plane intersection is only a drawing proxy, never an alternative note anchor. */
    bool ProjectVisibleSegment(const FSceneView* View, const FVector& Camera, const FVector& Forward,
        FVector From, FVector To, FVector2D& PixelFrom, FVector2D& PixelTo,
        bool& bFromInFront, bool& bToInFront)
    {
        constexpr double NearPlane = 10.0;
        const double D0 = FVector::DotProduct(Forward, From - Camera);
        const double D1 = FVector::DotProduct(Forward, To - Camera);
        bFromInFront = D0 > NearPlane;
        bToInFront = D1 > NearPlane;
        if (!bFromInFront && !bToInFront) return false;
        if (bFromInFront != bToInFront)
        {
            const double T = (NearPlane - D0) / (D1 - D0);
            const FVector Clipped = FMath::Lerp(From, To, FMath::Clamp(T, 0.0, 1.0));
            if (!bFromInFront) From = Clipped;
            else To = Clipped;
        }
        return View->WorldToPixel(From, PixelFrom) && View->WorldToPixel(To, PixelTo);
    }

    // All pieces of a connection share the same screen-space color, width and endpoints.
    void DrawConnectionLine(FCanvas* Canvas, FVector2D From, FVector2D To,
        const FIntPoint& ViewSize, FLinearColor Color, float Width, EAidLineStyle Style)
    {
        if (!AidViewportGeometry::ClipToViewport(From, To, ViewSize)) return;
        if (Style == EAidLineStyle::Solid)
        {
            CanvasLine(Canvas, From, To, Color, Width);
            return;
        }
        const FVector2D Delta = To - From;
        const float Length = Delta.Size();
        const FVector2D Direction = Delta / Length;
        const float Dash = Style == EAidLineStyle::Dashed ? 13.f : 3.f;
        const float Gap = Style == EAidLineStyle::Dashed ? 9.f : 6.f;
        for (float Offset = 0.f; Offset < Length; Offset += Dash + Gap)
        {
            CanvasLine(Canvas, From + Direction * Offset,
                From + Direction * FMath::Min(Length, Offset + Dash), Color, Width);
        }
    }

    void DrawHudButton(FCanvas* Canvas, const FVector2D TopLeft, float Width,
        const FText& Caption, const FLinearColor& Fill)
    {
        FCanvasTileItem Background(TopLeft, FVector2D(Width, 30.f), Fill);
        Background.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Background);
        FCanvasTextItem Text(TopLeft + FVector2D(10.f, 7.f), Caption,
            GEngine->GetSmallFont(), FLinearColor::White);
        Canvas->DrawItem(Text);
    }

}

void FAidViewportMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
    // Intentionally no Aid PDI lines: mixed world-space PDI + screen-space cap insets
    // caused truncation and mismatched color/thickness as the camera moved.
    FEdMode::Render(View, Viewport, PDI);
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
    if (!State || !State->bViewportVisible || !AidViewportShowFlag::Flag.IsEnabled(View->Family->EngineShowFlags)) return;

    const FVector Camera = ViewportClient->GetViewLocation();
    const FVector Forward = ViewportClient->GetViewRotation().Vector();
    // Resolve each document once. Connections render behind everything, followed by notes
    // ordered from farthest to nearest so nearer cards and markers occlude distant ones.
    struct FDocVisual
    {
        UAidDocument* Document = nullptr;
        TMap<FGuid, FVector> Locations;
    };
    TArray<FDocVisual> DocumentVisuals;
    for (UAidDocument* Document : State->VisibleDocuments)
    {
        if (!IsValid(Document)) continue;
        FDocVisual& Visual = DocumentVisuals.AddDefaulted_GetRef();
        Visual.Document = Document;
        Document->BuildResolvedPositions(ViewportClient->GetWorld(), Visual.Locations);
    }
    const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
    for (const FDocVisual& Visual : DocumentVisuals)
    {
        UAidDocument* Doc = Visual.Document;
        const TMap<FGuid, FVector>& Locations = Visual.Locations;
        for (const FAidConnection& Edge : Doc->Connections)
        {
            const FAidNode* A = Doc->FindNode(Edge.SourceNode);
            const FAidNode* B = Doc->FindNode(Edge.TargetNode);
            if (!A || !B) continue;
            const FVector From = Locations.FindRef(A->Id);
            const FVector To = Locations.FindRef(B->Id);
            if (!UAidDocument::IsConnectionVisible(Edge, *A, *B, Camera, From, To)) continue;
            FVector2D Start, End;
            bool bFrontStart = false, bFrontEnd = false;
            if (!ProjectVisibleSegment(View, Camera, Forward, From, To, Start, End, bFrontStart, bFrontEnd)) continue;
            const FVector2D Delta = End - Start;
            const float Length = Delta.Size();
            if (Length < 1.f) continue;
            const FVector2D Direction = Delta / Length;
            // Only actual projected note endpoints are inset. A clipped endpoint remains at
            // the near-plane or viewport boundary so the visible line never appears broken.
            const float StartGap = FMath::Min(
                Edge.StartCap == EAidCapStyle::Circle || Edge.StartCap == EAidCapStyle::Square ? 23.f : LineCapGap,
                Length * .30f);
            const float EndGap = FMath::Min(
                Edge.EndCap == EAidCapStyle::Circle || Edge.EndCap == EAidCapStyle::Square ? 23.f : LineCapGap,
                Length * .30f);
            const FVector2D LineStart = Start + Direction * (bFrontStart ? StartGap : 0.f);
            const FVector2D LineEnd = End - Direction * (bFrontEnd ? EndGap : 0.f);
            const bool bSourceOnScreen = bFrontStart && IsOnScreen(Start, Viewport->GetSizeXY(), 0.f);
            const bool bTargetOnScreen = bFrontEnd && IsOnScreen(End, Viewport->GetSizeXY(), 0.f);
            const bool bSelected = Doc == State->ActiveDocument && Edge.Id == State->SelectedConnectionId;
            const FLinearColor Color = Settings->ResolveConnectionColor(Edge);
            const float Thickness = FMath::Clamp(Edge.Thickness, .5f, 12.f);
            if (bSelected)
                DrawConnectionLine(Canvas, LineStart, LineEnd, Viewport->GetSizeXY(), SelectionColor(), Thickness + 3.f, Edge.LineStyle);
            DrawConnectionLine(Canvas, LineStart, LineEnd, Viewport->GetSizeXY(), Color, Thickness, Edge.LineStyle);
            // Caps are mutually exclusive. None means no geometry; Arrow is never drawn for a Circle/Square.
            if (bSourceOnScreen)
                DrawCap(Canvas, Edge.StartCap, LineStart, -Direction, Color, Thickness);
            if (bTargetOnScreen)
                DrawCap(Canvas, Edge.EndCap, LineEnd, Direction, Color, Thickness);
            if (bFrontStart && bFrontEnd)
            {
                const FVector2D Mid = (LineStart + LineEnd) * .5f;
                if (!IsOnScreen(Mid, Viewport->GetSizeXY(), 5.f)) continue;
                const TArray<FString> LabelLines = WrapLines(Edge.Label.ToString(), 282.f, 1.f);
                const FString FooterText = Settings->GetConnectionFooterLabel(Edge);
                const TArray<FString> FooterLines = WrapLines(FooterText, 282.f, 1.f);
                float ContentWidth = 0.f;
                for (const FString& Line : LabelLines)
                    ContentWidth = FMath::Max(ContentWidth, static_cast<float>(GEngine->GetSmallFont()->GetStringSize(*Line)));
                for (const FString& Line : FooterLines)
                    ContentWidth = FMath::Max(ContentWidth, static_cast<float>(GEngine->GetSmallFont()->GetStringSize(*Line)));
                const float LabelWidth = FMath::Clamp(ContentWidth + 20.f, 80.f, 300.f);
                const bool bHasLabel = !Edge.Label.IsEmpty();
                const float HeaderHeight = bHasLabel ? FMath::Max(23.f, LabelLines.Num() * 15.f + 8.f) : 0.f;
                const float FooterHeight = FMath::Max(21.f, FooterLines.Num() * 15.f + 6.f);
                const float LabelHeight = HeaderHeight + FooterHeight;
                const FVector2D LabelTopLeft = Mid - FVector2D(LabelWidth * .5f, LabelHeight * .5f);
                if (bSelected)
                {
                    FCanvasTileItem Outline(LabelTopLeft - FVector2D(2.f,2.f), FVector2D(LabelWidth + 4.f, LabelHeight + 4.f), SelectionColor());
                    Outline.BlendMode = SE_BLEND_Translucent;
                    Canvas->DrawItem(Outline);
                }
                if (bHasLabel)
                {
                    FCanvasTileItem Backing(LabelTopLeft, FVector2D(LabelWidth, HeaderHeight), Color);
                    Backing.BlendMode = SE_BLEND_Translucent;
                    Canvas->DrawItem(Backing);
                    for (int32 Line = 0; Line < LabelLines.Num(); ++Line)
                    {
                        FCanvasTextItem Item(LabelTopLeft + FVector2D(10.f, 4.f + Line * 15.f),
                            FText::FromString(LabelLines[Line]), GEngine->GetSmallFont(), FLinearColor::White);
                        Canvas->DrawItem(Item);
                    }
                }
                const FVector2D FooterTop = LabelTopLeft + FVector2D(0.f, HeaderHeight);
                FCanvasTileItem Footer(FooterTop, FVector2D(LabelWidth, FooterHeight), FAidCardStyle::GetFooterColor(Color));
                Footer.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(Footer);
                for (int32 Line = 0; Line < FooterLines.Num(); ++Line)
                {
                    FCanvasTextItem Item(FooterTop + FVector2D(10.f, 3.f + Line * 15.f),
                        FText::FromString(FooterLines[Line]), GEngine->GetSmallFont(), FLinearColor::White);
                    Canvas->DrawItem(Item);
                }
                FAidCardHit Hit;
                Hit.Document = Doc;
                Hit.EdgeId = Edge.Id;
                Hit.TopLeft = LabelTopLeft;
                Hit.BottomRight = LabelTopLeft + FVector2D(LabelWidth, LabelHeight);
                Regions.Add(Hit);
            }
        }
    }

    struct FDrawNote
    {
        UAidDocument* Document = nullptr;
        const FAidNode* Note = nullptr;
        FVector Position = FVector::ZeroVector;
        uint8 Visibility = 0;
        double DistanceSq = 0.0;
    };
    TArray<FDrawNote> NotesToDraw;
    for (const FDocVisual& Visual : DocumentVisuals)
    {
        for (const FAidNode& Node : Visual.Document->Nodes)
        {
            const FVector Position = Visual.Locations.FindRef(Node.Id);
            const uint8 Visibility = UAidDocument::GetNoteVisibility(Node, Camera, Position);
            if (!Visibility || !IsForward(Camera, Forward, Position)) continue;
            FDrawNote& Item = NotesToDraw.AddDefaulted_GetRef();
            Item.Document = Visual.Document;
            Item.Note = &Node;
            Item.Position = Position;
            Item.Visibility = Visibility;
            Item.DistanceSq = FVector::DistSquared(Camera, Position);
        }
    }
    NotesToDraw.Sort([](const FDrawNote& A, const FDrawNote& B) { return A.DistanceSq > B.DistanceSq; });
    for (const FDrawNote& DrawNote : NotesToDraw)
    {
        UAidDocument* Doc = DrawNote.Document;
        const FAidNode& Node = *DrawNote.Note;
        const FVector Position = DrawNote.Position;
        const uint8 StateOfNote = DrawNote.Visibility;

            FVector2D P;
            if (!View->WorldToPixel(Position, P)) continue;
            if (P.X < -350.f || P.Y < -200.f || P.X > Viewport->GetSizeXY().X + 100.f || P.Y > Viewport->GetSizeXY().Y + 200.f) continue;
            const bool bSelected = Doc == State->ActiveDocument && Node.Id == State->SelectedNodeId;
            FAidCardStyle EffectiveStyle = Node.Style;
            EffectiveStyle.ApplyBaseColor(Settings->ResolveNoteColor(Node));
            const FLinearColor Ring = EffectiveStyle.GetBorderColor();
            DrawRing(Canvas, P, EffectiveStyle.HeaderBackgroundColor, Ring, MarkerRadius);
            if (bSelected) DrawRing(Canvas, P, EffectiveStyle.HeaderBackgroundColor, SelectionColor(), 12.f);
            FAidCardHit MarkerHit;
            MarkerHit.Document = Doc;
            MarkerHit.NodeId = Node.Id;
            MarkerHit.TopLeft = P - FVector2D(14.f, 14.f);
            MarkerHit.BottomRight = P + FVector2D(14.f, 14.f);
            Regions.Add(MarkerHit);
            if (StateOfNote == 1) continue;

            const float FontScale = FMath::Clamp(Settings->NoteFontSize / 16.f, .5f, 6.f);
            const float LineHeight = 17.f * FontScale;
            const float Border = FMath::Clamp(Settings->NoteBorderThickness, 1.f, 12.f);
            const float Width = FMath::Clamp(270.f + (FontScale - 1.f) * 70.f, 250.f, 620.f);
            const float Pad = 10.f;
            const float TextWidth = Width - 2.f * (Border + Pad);
            const TArray<FString> TitleLines = WrapLines(Node.Title.ToString(), TextWidth, FontScale);
            const bool bHasBody = !Node.Body.IsEmpty();
            const TArray<FString> BodyLines = bHasBody ? WrapLines(Node.Body.ToString(), TextWidth, FontScale) : TArray<FString>();
            const float HeaderHeight = FMath::Max(1, TitleLines.Num()) * LineHeight + Pad * 2.f;
            const float BodyHeight = bHasBody ? FMath::Max(1, BodyLines.Num()) * LineHeight + Pad * 2.f : 0.f;
            const UEnum* TypeEnum = StaticEnum<EAidNoteType>();
            const FString TypeName = TypeEnum ? TypeEnum->GetDisplayNameTextByValue(static_cast<int64>(Node.Type)).ToString() : TEXT("Note");
            const FString FooterText = Settings->GetNoteFooterLabel(Node) + TEXT("  ·  ") + TypeName;
            const TArray<FString> FooterLines = WrapLines(FooterText, Width - 2.f * (Border + Pad), 1.f);
            const float FooterHeight = FMath::Max(21.f, FooterLines.Num() * 15.f + 7.f);
            const FVector2D Size(Width, HeaderHeight + BodyHeight + FooterHeight + Border * 2.f);
            const FVector2D TopLeft = P + FVector2D(32.f, -23.f);
            if (TopLeft.X + Size.X < 0 || TopLeft.Y + Size.Y < 0 ||
                TopLeft.X > Viewport->GetSizeXY().X || TopLeft.Y > Viewport->GetSizeXY().Y) continue;
            FCanvasTileItem Frame(TopLeft, Size, bSelected ? SelectionColor() : Ring);
            Frame.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Frame);
            FCanvasTileItem Header(TopLeft + FVector2D(Border, Border), FVector2D(Size.X - Border * 2.f, HeaderHeight),
                EffectiveStyle.HeaderBackgroundColor);
            Header.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Header);
            if (bHasBody)
            {
                FCanvasTileItem BodyBackground(TopLeft + FVector2D(Border, Border + HeaderHeight),
                    FVector2D(Size.X - Border * 2.f, BodyHeight), EffectiveStyle.BodyBackgroundColor);
                BodyBackground.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(BodyBackground);
            }
            // Footer makes category and note intent readable even when multiple categories share a color.
            const FVector2D FooterTopLeft = TopLeft + FVector2D(Border, Border + HeaderHeight + BodyHeight);
            FCanvasTileItem Footer(FooterTopLeft, FVector2D(Size.X - 2.f * Border, FooterHeight),
                FAidCardStyle::GetFooterColor(EffectiveStyle.HeaderBackgroundColor));
            Footer.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(Footer);
            for (int32 Line = 0; Line < TitleLines.Num(); ++Line)
            {
                FCanvasTextItem Item(TopLeft + FVector2D(Border + Pad, Border + Pad + Line * LineHeight),
                    FText::FromString(TitleLines[Line]), GEngine->GetSmallFont(), FLinearColor::White);
                Item.Scale = FVector2D(FontScale, FontScale);
                Item.EnableShadow(FLinearColor::Black, FVector2D(1.f, 1.f));
                Canvas->DrawItem(Item);
            }
            for (int32 Line = 0; Line < BodyLines.Num(); ++Line)
            {
                FCanvasTextItem Item(TopLeft + FVector2D(Border + Pad, Border + HeaderHeight + Pad + Line * LineHeight),
                    FText::FromString(BodyLines[Line]), GEngine->GetSmallFont(), FLinearColor::White);
                Item.Scale = FVector2D(FontScale, FontScale);
                Canvas->DrawItem(Item);
            }
            for (int32 Line = 0; Line < FooterLines.Num(); ++Line)
            {
                FCanvasTextItem FooterItem(FooterTopLeft + FVector2D(11.f, 4.f + Line * 15.f),
                    FText::FromString(FooterLines[Line]), GEngine->GetSmallFont(), FLinearColor(.85f, .86f, .88f, 1.f));
                Canvas->DrawItem(FooterItem);
            }
            FAidCardHit CardHit;
            CardHit.Document = Doc;
            CardHit.NodeId = Node.Id;
            CardHit.TopLeft = TopLeft;
            CardHit.BottomRight = TopLeft + Size;
            Regions.Add(CardHit);
    }
    if (State->bPlaceNoteOnClick)
    {
        // Compact editor-tool footer: native accent-blue ACCEPT and neutral CANCEL,
        // with the placement title on the left, following Modeling-mode vocabulary.
        const float CenterX = Viewport->GetSizeXY().X * .5f;
        const float TopY = Viewport->GetSizeXY().Y - 50.f;
        FCanvasTileItem Back(FVector2D(CenterX - 176.f, TopY - 6.f), FVector2D(352.f, 40.f),
            FLinearColor(.025f, .03f, .035f, .97f));
        Back.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Back);
        FCanvasTextItem Caption(FVector2D(CenterX - 165.f, TopY + 6.f),
            FText::FromString(State->bPlaceOnActor ? TEXT("Attach Aid Note") : TEXT("Place Aid Note")),
            GEngine->GetSmallFont(), FLinearColor::White);
        Canvas->DrawItem(Caption);
        DrawHudButton(Canvas, FVector2D(CenterX - 10.f, TopY), 89.f,
            FText::FromString(TEXT("ACCEPT")),
            State->bHasPlacementPreview ? SelectionColor() : FLinearColor(.17f, .18f, .20f, 1.f));
        DrawHudButton(Canvas, FVector2D(CenterX + 84.f, TopY), 89.f,
            FText::FromString(TEXT("CANCEL")), FLinearColor(.15f, .16f, .18f, 1.f));
        if (State->bHasPlacementPreview && IsForward(Camera, Forward, State->PlacementPreviewPosition))
        {
            FVector2D PreviewPixel;
            if (View->WorldToPixel(State->PlacementPreviewPosition, PreviewPixel))
                DrawRing(Canvas, PreviewPixel, SelectionColor(), FLinearColor::White, 12.f);
        }
    }

}

bool FAidViewportMode::UsesTransformWidget() const
{
    const UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr;
    return !State || (!State->bPlaceNoteOnClick && !State->PendingConnectionSource.IsValid());
}

bool FAidViewportMode::GetCursor(EMouseCursor::Type& OutCursor) const
{
    const UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr;
    if (State && (State->bPlaceNoteOnClick || State->PendingConnectionSource.IsValid()))
    {
        OutCursor = EMouseCursor::Crosshairs;
        return true;
    }
    // Do not own the cursor in review/edit unless an explicit operation is active.
    return false;
}

bool FAidViewportMode::InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event)
{
    if (!GEditor || !Client || !Viewport) return FEdMode::InputKey(Client, Viewport, Key, Event);
    UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>();
    if (!State || !State->bViewportVisible) return FEdMode::InputKey(Client, Viewport, Key, Event);

    if (Event == IE_Pressed && Key == EKeys::Escape &&
        (State->bPlaceNoteOnClick || State->PendingConnectionSource.IsValid()))
    {
        State->CancelPendingAction();
        return true;
    }
    if (Event == IE_Pressed && Key == EKeys::Enter && State->bPlaceNoteOnClick)
    {
        State->ConfirmPlacement();
        return true;
    }
    if (Event != IE_Pressed || Key != EKeys::LeftMouseButton ||
        Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt) ||
        Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl))
        return FEdMode::InputKey(Client, Viewport, Key, Event);

    const FVector2D Mouse(static_cast<float>(Viewport->GetMouseX()), static_cast<float>(Viewport->GetMouseY()));
    if (State->bPlaceNoteOnClick)
    {
        const float CenterX = Viewport->GetSizeXY().X * .5f;
        const float TopY = Viewport->GetSizeXY().Y - 50.f;
        if (Mouse.Y >= TopY - 6.f && Mouse.Y <= TopY + 34.f)
        {
            if (Mouse.X >= CenterX - 10.f && Mouse.X <= CenterX + 79.f)
            {
                State->ConfirmPlacement();
                return true;
            }
            if (Mouse.X >= CenterX + 84.f && Mouse.X <= CenterX + 173.f)
            {
                State->CancelPendingAction();
                return true;
            }
            // Clicking the footer's title must never place a note through the HUD.
            if (Mouse.X >= CenterX - 176.f && Mouse.X <= CenterX + 176.f) return true;
        }
        if (!State->bEditMode || !State->ActiveDocument) return true;
        const FViewportCursorLocation Ray = Client->GetCursorWorldLocationFromMousePos();
        const FVector Start = Ray.GetOrigin();
        const FVector Direction = Ray.GetDirection();
        FVector Position = Start + Direction * 600.f;
        FHitResult Hit;
        bool bHit = false;
        if (UWorld* World = Client->GetWorld())
        {
            FCollisionQueryParams Params;
            Params.bTraceComplex = true;
            bHit = World->LineTraceSingleByChannel(Hit, Start, Start + Direction * 100000.f, ECC_Visibility, Params);
            if (bHit) Position = Hit.ImpactPoint + Hit.ImpactNormal * 10.f;
        }
        if (!State->bPlaceOnActor || (bHit && IsValid(Hit.GetActor())))
            State->StagePlacement(Position, State->bPlaceOnActor ? Hit.GetActor() : nullptr);
        return true;
    }

    // Selection works in Review and Edit. Only authoring actions require Edit Mode.
    if (const TArray<FAidCardHit>* Hits = HitRegions.Find(Viewport))
    {
        for (int32 Index = Hits->Num() - 1; Index >= 0; --Index)
        {
            const FAidCardHit& H = (*Hits)[Index];
            if (Mouse.X < H.TopLeft.X || Mouse.Y < H.TopLeft.Y || Mouse.X > H.BottomRight.X || Mouse.Y > H.BottomRight.Y) continue;
            if (!H.Document.IsValid()) continue;
            if (State->ActiveDocument != H.Document.Get()) State->SetActiveDocument(H.Document.Get());
            if (H.EdgeId.IsValid()) State->SelectConnection(H.EdgeId);
            else if (H.NodeId.IsValid()) State->SelectNode(H.NodeId);
            return true;
        }
    }
    return FEdMode::InputKey(Client, Viewport, Key, Event);
}
