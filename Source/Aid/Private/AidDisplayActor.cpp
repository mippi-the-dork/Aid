#include "AidDisplayActor.h"
#include "AidDocument.h"
#include "Components/SceneComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

AAidDisplayActor::AAidDisplayActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false; // Static level placement exists on each client; display is local only.
    AidRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AidRoot"));
    RootComponent = AidRoot;
    ConnectionBatcher = CreateDefaultSubobject<ULineBatchComponent>(TEXT("AidConnections"));
    ConnectionBatcher->SetupAttachment(AidRoot);
    ConnectionBatcher->SetCollisionEnabled(ECollisionEnabled::NoCollision);
#if WITH_EDITOR
    SetIsSpatiallyLoaded(false); // QA viewer remains available when spatial cells are unloaded.
#endif
}

bool AAidDisplayActor::ShouldDisplay() const
{
    if (!bShowInPlay || !Document || !GetWorld() || !GetWorld()->IsGameWorld())
    {
        return false;
    }
#if UE_BUILD_SHIPPING
    if (!bAllowInShipping)
    {
        return false;
    }
#endif
    return true;
}

void AAidDisplayActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshAidDisplay();
}

void AAidDisplayActor::SetAidDocument(UAidDocument* NewDocument)
{
    Document = NewDocument;
    RefreshAidDisplay();
}

void AAidDisplayActor::ClearRuntimeCards()
{
    for (UTextRenderComponent* Card : RuntimeCards)
    {
        if (IsValid(Card))
        {
            Card->DestroyComponent();
        }
    }
    RuntimeCards.Reset();
    RuntimeCardStates.Reset();
    if (ConnectionBatcher)
    {
        ConnectionBatcher->Flush();
    }
}

void AAidDisplayActor::RefreshAidDisplay()
{
    ClearRuntimeCards();
    if (!ShouldDisplay())
    {
        SetActorTickEnabled(false);
        return;
    }
    SetActorTickEnabled(true);

    for (const FAidNode& Node : Document->Nodes)
    {
        UTextRenderComponent* Card = NewObject<UTextRenderComponent>(this);
        Card->SetupAttachment(AidRoot);
        Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Card->SetHorizontalAlignment(EHTA_Center);
        Card->SetVerticalAlignment(EVRTA_TextCenter);
        Card->SetWorldSize(TextWorldSize);
        Card->SetTextRenderColor(Node.Style.BodyTextColor.ToFColor(true));
        const FString DisplayText = Node.Body.IsEmpty()
            ? Node.Title.ToString()
            : Node.Title.ToString() + TEXT("\n") + Node.Body.ToString();
        Card->SetText(FText::FromString(DisplayText));
        Card->RegisterComponent();
        Card->SetVisibility(false); // Per-camera view-distance state is evaluated in Tick.
        RuntimeCards.Add(Card);
        RuntimeCardStates.Add(0);
    }
    Tick(0.f);
}

void AAidDisplayActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ShouldDisplay())
    {
        return;
    }

    TMap<FGuid, FVector> Locations;
    Document->BuildResolvedPositions(GetWorld(), Locations);
    const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    for (int32 Index = 0; Index < Document->Nodes.Num() && Index < RuntimeCards.Num(); ++Index)
    {
        const FAidNode& Node = Document->Nodes[Index];
        UTextRenderComponent* Card = RuntimeCards[Index];
        if (!IsValid(Card)) continue;
        const FVector Position = Locations.FindRef(Node.Id);
        const FVector CameraLocation = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
        const uint8 Visibility = Camera ? UAidDocument::GetNoteVisibility(Node, CameraLocation, Position) : 0;
        Card->SetVisibility(Visibility != 0);
        if (!Visibility) continue;
        // Runtime QA uses the same 0/1/2 distance states; detailed Slate cards remain editor-only.
        if (RuntimeCardStates.IsValidIndex(Index) && RuntimeCardStates[Index] != Visibility)
        {
            const FString DisplayText = Visibility == 1 ? TEXT("(o)") :
                (Node.Body.IsEmpty() ? Node.Title.ToString() : Node.Title.ToString() + TEXT("\n") + Node.Body.ToString());
            Card->SetText(FText::FromString(DisplayText));
            Card->SetTextRenderColor((Visibility == 1 ? Node.Style.HeaderBackgroundColor : Node.Style.BodyTextColor).ToFColor(true));
            RuntimeCardStates[Index] = Visibility;
        }
        Card->SetWorldLocation(Position);
        if (Camera && bFaceLocalPlayerCamera)
        {
            Card->SetWorldRotation(FRotationMatrix::MakeFromX(Camera->GetCameraLocation() - Position).Rotator());
        }
    }

    if (ConnectionBatcher)
    {
        ConnectionBatcher->Flush();
        if (bShowConnectionLines)
        {
            for (const FAidConnection& Edge : Document->Connections)
            {
                const FAidNode* From = Document->FindNode(Edge.SourceNode);
                const FAidNode* To = Document->FindNode(Edge.TargetNode);
                if (!From || !To) continue;
                if (!Camera || !UAidDocument::IsConnectionVisible(Edge, *From, *To, Camera->GetCameraLocation(),
                        Locations.FindRef(From->Id), Locations.FindRef(To->Id))) continue;
                ConnectionBatcher->DrawLine(
                    Locations.FindRef(From->Id), Locations.FindRef(To->Id),
                    Edge.Color, SDPG_World, Edge.Thickness, 0.f, 0);
            }
        }
    }
}

void AAidDisplayActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearRuntimeCards();
    Super::EndPlay(EndPlayReason);
}
