#include "AidDocument.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

FLinearColor FAidCardStyle::GetBorderColor() const
{
    // Lift the title value, preserving its hue; clamp for predictable SDR HUD colors.
    const FLinearColor C = HeaderBackgroundColor;
    return FLinearColor(FMath::Clamp(C.R * 1.35f + 0.08f, 0.f, 1.f),
        FMath::Clamp(C.G * 1.35f + 0.08f, 0.f, 1.f),
        FMath::Clamp(C.B * 1.35f + 0.08f, 0.f, 1.f), 1.f);
}

FVector FAidAnchor::Resolve(const UWorld* World) const
{
    if (Type == EAidAnchorType::ActorTag && World && !ActorTag.IsNone())
    {
        for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if (IsValid(*It) && It->ActorHasTag(ActorTag))
            {
                return It->GetActorTransform().TransformPosition(LocalOffset);
            }
        }
    }
    return WorldPosition;
}

FGuid UAidDocument::AddNode(const FText& Title, const FVector& WorldPosition)
{
    FAidNode& Node = Nodes.AddDefaulted_GetRef();
    Node.Id = FGuid::NewGuid();
    Node.Title = Title;
    Node.Anchor.WorldPosition = WorldPosition;
    return Node.Id;
}

FGuid UAidDocument::AddConnection(const FGuid& SourceId, const FGuid& TargetId, const FText& Label)
{
    if (!HasNode(SourceId) || !HasNode(TargetId) || SourceId == TargetId)
    {
        return FGuid();
    }

    FAidConnection& Edge = Connections.AddDefaulted_GetRef();
    Edge.Id = FGuid::NewGuid();
    Edge.SourceNode = SourceId;
    Edge.TargetNode = TargetId;
    Edge.Label = Label;
    return Edge.Id;
}

const FAidNode* UAidDocument::FindNode(const FGuid& Id) const
{
    if (!Id.IsValid())
    {
        return nullptr;
    }
    return Nodes.FindByPredicate([&Id](const FAidNode& N) { return N.Id == Id; });
}

bool UAidDocument::RemoveNode(const FGuid& Id)
{
    const int32 Removed = Nodes.RemoveAll([&Id](const FAidNode& Node) { return Node.Id == Id; });
    if (Removed == 0)
    {
        return false;
    }
    Connections.RemoveAll([&Id](const FAidConnection& Edge) {
        return Edge.SourceNode == Id || Edge.TargetNode == Id;
    });
    return true;
}

bool UAidDocument::RemoveConnection(const FGuid& Id)
{
    return Connections.RemoveAll([&](const FAidConnection& Edge) { return Edge.Id == Id; }) > 0;
}

uint8 UAidDocument::GetNoteVisibility(const FAidNode& Node, const FVector& Camera, const FVector& Anchor)
{
    if (!Node.bVisible || Node.ViewDistance <= 0.f) return 0;
    const double DistanceSq = FVector::DistSquared(Camera, Anchor);
    const double FullDistance = Node.ViewDistance * 0.5;
    if (DistanceSq > FMath::Square(static_cast<double>(Node.ViewDistance))) return 0;
    return DistanceSq <= FullDistance * FullDistance ? 2 : 1;
}

bool UAidDocument::IsConnectionVisible(const FAidConnection& Edge, const FAidNode& Source, const FAidNode& Target,
    const FVector& Camera, const FVector& From, const FVector& To)
{
    if (!Edge.bVisible) return false;
    if (!GetNoteVisibility(Source, Camera, From) && !GetNoteVisibility(Target, Camera, To)) return false;
    if (Edge.ViewDistance > 0.f)
    {
        const FVector Nearest = FMath::ClosestPointOnSegment(Camera, From, To);
        return FVector::DistSquared(Camera, Nearest) <= FMath::Square(static_cast<double>(Edge.ViewDistance));
    }
    return true;
}

bool UAidDocument::IsAssociatedWithLevel(const FString& PackageName) const
{
    for (const TSoftObjectPtr<UWorld>& Level : AssociatedLevels)
    {
        if (Level.ToSoftObjectPath().GetLongPackageName() == PackageName) return true;
    }
    return false;
}

void UAidDocument::BuildResolvedPositions(const UWorld* World, TMap<FGuid, FVector>& OutPositions) const
{
    OutPositions.Reset();
    OutPositions.Reserve(Nodes.Num());
    TSet<FName> WantedTags;
    for (const FAidNode& Node : Nodes)
    {
        if (Node.Anchor.Type == EAidAnchorType::ActorTag && !Node.Anchor.ActorTag.IsNone())
        {
            WantedTags.Add(Node.Anchor.ActorTag);
        }
    }

    TMap<FName, FTransform> MatchedTransforms;
    if (World && WantedTags.Num() > 0)
    {
        for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if (!IsValid(*It))
            {
                continue;
            }
            for (const FName& Tag : It->Tags)
            {
                if (WantedTags.Contains(Tag) && !MatchedTransforms.Contains(Tag))
                {
                    MatchedTransforms.Add(Tag, It->GetActorTransform());
                }
            }
            if (MatchedTransforms.Num() == WantedTags.Num())
            {
                break;
            }
        }
    }

    for (const FAidNode& Node : Nodes)
    {
        FVector Location = Node.Anchor.WorldPosition;
        if (Node.Anchor.Type == EAidAnchorType::ActorTag)
        {
            if (const FTransform* Transform = MatchedTransforms.Find(Node.Anchor.ActorTag))
            {
                Location = Transform->TransformPosition(Node.Anchor.LocalOffset);
            }
        }
        OutPositions.Add(Node.Id, Location);
    }
}

bool UAidDocument::HasNode(const FGuid& Id) const
{
    return FindNode(Id) != nullptr;
}

TArray<FString> UAidDocument::ValidateGraph() const
{
    TArray<FString> Errors;
    TSet<FGuid> SeenNodes;
    TSet<FGuid> SeenEdges;
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        const FAidNode& Node = Nodes[Index];
        if (!Node.Id.IsValid())
        {
            Errors.Add(FString::Printf(TEXT("Node %d has an invalid ID. Create notes through the Aid Browser."), Index));
        }
        else if (SeenNodes.Contains(Node.Id))
        {
            Errors.Add(FString::Printf(TEXT("Node %d has a duplicate ID."), Index));
        }
        SeenNodes.Add(Node.Id);
        if (Node.Anchor.Type == EAidAnchorType::ActorTag && Node.Anchor.ActorTag.IsNone())
        {
            Errors.Add(FString::Printf(TEXT("Node %d uses ActorTag binding but has no tag."), Index));
        }
    }

    for (int32 Index = 0; Index < Connections.Num(); ++Index)
    {
        const FAidConnection& Edge = Connections[Index];
        if (!Edge.Id.IsValid() || SeenEdges.Contains(Edge.Id))
        {
            Errors.Add(FString::Printf(TEXT("Connection %d has an invalid or duplicate ID."), Index));
        }
        SeenEdges.Add(Edge.Id);
        if (!HasNode(Edge.SourceNode) || !HasNode(Edge.TargetNode))
        {
            Errors.Add(FString::Printf(TEXT("Connection %d references a missing node."), Index));
        }
        if (Edge.SourceNode == Edge.TargetNode)
        {
            Errors.Add(FString::Printf(TEXT("Connection %d is a self-link (unsupported in alpha)."), Index));
        }
    }
    return Errors;
}
