#include "AidDocument.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

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
