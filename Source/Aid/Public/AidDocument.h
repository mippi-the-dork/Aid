#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AidDocument.generated.h"

class AActor;
class UWorld;

/** How a note locates its anchor. No hard reference to a level actor is serialized. */
UENUM(BlueprintType)
enum class EAidAnchorType : uint8
{
    World UMETA(DisplayName="World Position"),
    ActorTag UMETA(DisplayName="Actor Tag (with World Fallback)")
};

UENUM(BlueprintType)
enum class EAidLineStyle : uint8
{
    Solid,
    Dashed,
    Dotted
};

UENUM(BlueprintType)
enum class EAidCapStyle : uint8
{
    None,
    Arrowhead,
    Circle,
    Bar
};

USTRUCT(BlueprintType)
struct AID_API FAidCardStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header")
    FLinearColor HeaderBackgroundColor = FLinearColor(0.07f, 0.27f, 0.38f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header")
    FLinearColor HeaderTextColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body")
    FLinearColor BodyBackgroundColor = FLinearColor(0.03f, 0.07f, 0.10f, 0.94f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body")
    FLinearColor BodyTextColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame")
    FLinearColor BorderColor = FLinearColor(0.23f, 0.63f, 0.76f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(ClampMin="0.0", UIMin="0.0"))
    float BorderThickness = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(ClampMin="0.0", UIMin="0.0"))
    float CornerRadius = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Typography", meta=(ClampMin="8", ClampMax="96"))
    int32 FontSize = 16;
};

/** Position is always world-space: the viewer actor is NOT an annotation origin. */
USTRUCT(BlueprintType)
struct AID_API FAidAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor")
    EAidAnchorType Type = EAidAnchorType::World;

    /** Persistent fallback position, and actual position for World anchors. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor", meta=(MakeEditWidget="true"))
    FVector WorldPosition = FVector::ZeroVector;

    /** Target must have this tag. Multiple matching actors are ambiguous; first match wins in alpha. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor", meta=(EditCondition="Type == EAidAnchorType::ActorTag"))
    FName ActorTag = NAME_None;

    /** Local-space offset from the tagged actor, transformed by its rotation and scale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor", meta=(EditCondition="Type == EAidAnchorType::ActorTag"))
    FVector LocalOffset = FVector(0., 0., 120.);

    FVector Resolve(const UWorld* World) const;
};

USTRUCT(BlueprintType)
struct AID_API FAidNode
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FText Title = FText::FromString(TEXT("New Aid Note"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content", meta=(MultiLine="true"))
    FText Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FName Category = TEXT("Design");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    TArray<FName> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement")
    FAidAnchor Anchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    FAidCardStyle Style;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bVisible = true;
};

USTRUCT(BlueprintType)
struct AID_API FAidConnection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Endpoints")
    FGuid SourceNode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Endpoints")
    FGuid TargetNode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FText Label;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FName RelationshipType = TEXT("References");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    FLinearColor Color = FLinearColor(0.25f, 0.8f, 0.95f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidLineStyle LineStyle = EAidLineStyle::Solid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidCapStyle StartCap = EAidCapStyle::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidCapStyle EndCap = EAidCapStyle::Arrowhead;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin="0.1", ClampMax="20.0"))
    float Thickness = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bVisible = true;
};

/**
 * Shared source of truth for editor (actor-free) and runtime (optional AAidDisplayActor) views.
 * Stores authoring data in Content packages, never in the map.
 * Note: a strong asset reference from a display actor DOES change that actor's level package.
 */
UCLASS(BlueprintType)
class AID_API UAidDocument : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aid|About", meta=(MultiLine="true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Nodes", meta=(TitleProperty="Title"))
    TArray<FAidNode> Nodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Connections")
    TArray<FAidConnection> Connections;

    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    FGuid AddNode(const FText& Title, const FVector& WorldPosition);

    /** Returns invalid GUID if either endpoint is missing or both IDs are identical. */
    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    FGuid AddConnection(const FGuid& SourceId, const FGuid& TargetId, const FText& Label);

    UFUNCTION(BlueprintPure, Category="Aid|Graph")
    bool HasNode(const FGuid& Id) const;

    const FAidNode* FindNode(const FGuid& Id) const;

    /** Resolve all node anchors once per frame. Actor tags are scanned once, not once per note/edge. */
    void BuildResolvedPositions(const UWorld* World, TMap<FGuid, FVector>& OutPositions) const;

    /** Returns errors (does not mutate the asset). */
    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    TArray<FString> ValidateGraph() const;
};
