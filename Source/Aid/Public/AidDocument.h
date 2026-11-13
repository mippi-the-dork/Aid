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
    ActorTag UMETA(DisplayName="Actor Tag (with World Fallback)"),
    Unplaced UMETA(DisplayName="Not Placed")
};

UENUM(BlueprintType)
enum class EAidNoteType : uint8
{
    Note, DesignIntent, Question, Warning, Task, QA
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
    Square
};

USTRUCT(BlueprintType)
struct AID_API FAidCardStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header")
    FLinearColor HeaderBackgroundColor = FLinearColor(0.07f, 0.27f, 0.38f, 1.f);

    UPROPERTY()
    FLinearColor HeaderTextColor = FLinearColor::White;

    UPROPERTY()
    FLinearColor BodyBackgroundColor = FLinearColor(0.03f, 0.07f, 0.10f, 0.94f);

    UPROPERTY()
    FLinearColor BodyTextColor = FLinearColor::White;

    UPROPERTY()
    float BorderThickness = 1.5f;

    UPROPERTY()
    int32 FontSize = 16;

    /** Automatically derived from the header color. No redundant border picker. */
    FLinearColor GetBorderColor() const;

    /** Header is the only user-authored color. Other card surfaces are derived. */
    static FLinearColor GetBodyColor(const FLinearColor& Base);
    static FLinearColor GetFooterColor(const FLinearColor& Base);
    void ApplyBaseColor(const FLinearColor& Base);
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
    FName Category = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    TArray<FName> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    EAidNoteType Type = EAidNoteType::Note;

    UPROPERTY()
    FLinearColor CategoryColor = FLinearColor(0.25f, 0.8f, 0.95f, 1.f); // Legacy field, no longer used by viewport rendering.

    /** Color override; choosing a preset resets this to false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bCustomAppearance = false;

    /** Only shown in the viewport footer when using a custom color. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FText CustomFooterText;

    /** Authoring data may exist before it has a location in the level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement")
    FAidAnchor Anchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    FAidCardStyle Style;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bVisible = true;

    /** Beyond this camera distance the marker is hidden; at half this distance the full card appears. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin="100.0", UIMin="500.0", UIMax="100000.0"))
    float ViewDistance = 5000.f;
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
    FName RelationshipType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    FLinearColor Color = FLinearColor(0.25f, 0.8f, 0.95f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bCustomAppearance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Content")
    FText CustomFooterText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidLineStyle LineStyle = EAidLineStyle::Solid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidCapStyle StartCap = EAidCapStyle::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    EAidCapStyle EndCap = EAidCapStyle::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin="0.1", ClampMax="20.0"))
    float Thickness = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    bool bVisible = true;

    /** Optional additional distance to the closest point of the line. Zero keeps long connections visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin="0.0"))
    float ViewDistance = 0.f;
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

    /** Optional document grouping in the Aid Documents outliner. NAME_None = ungrouped. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|About")
    FName DocumentCategory = NAME_None;

    /** Optional associated maps. This is metadata, not a requirement to display the document elsewhere. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Levels")
    TArray<TSoftObjectPtr<UWorld>> AssociatedLevels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Nodes", meta=(TitleProperty="Title"))
    TArray<FAidNode> Nodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aid|Connections")
    TArray<FAidConnection> Connections;

    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    FGuid AddNode(const FText& Title, const FVector& WorldPosition);

    /** Creates a note with content but no viewport anchor. */
    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    FGuid AddUnplacedNode(const FText& Title);

    /** Returns invalid GUID if either endpoint is missing or both IDs are identical. */
    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    FGuid AddConnection(const FGuid& SourceId, const FGuid& TargetId, const FText& Label);

    UFUNCTION(BlueprintPure, Category="Aid|Graph")
    bool HasNode(const FGuid& Id) const;

    /** Only one connection between an unordered pair; reverse direction is a duplicate. */
    bool HasConnectionBetween(const FGuid& First, const FGuid& Second) const;

    /** Retarget without deleting authoring/styling and without creating duplicate unordered pairs. */
    bool RetargetConnection(const FGuid& ConnectionId, const FGuid& NewTarget);

    const FAidNode* FindNode(const FGuid& Id) const;

    /** Removes a node and all of its connections; caller owns transaction/dirty state. */
    bool RemoveNode(const FGuid& Id);

    bool RemoveConnection(const FGuid& Id);

    /** View-distance states are shared by the editor and the optional QA runtime display. */
    static uint8 GetNoteVisibility(const FAidNode& Node, const FVector& Camera, const FVector& Anchor);

    /** As long as at least one endpoint is visible, the whole line is eligible to draw. */
    static bool IsConnectionVisible(const FAidConnection& Edge, const FAidNode& Source, const FAidNode& Target,
        const FVector& Camera, const FVector& From, const FVector& To);

    bool IsAssociatedWithLevel(const FString& PackageName) const;

    /** Resolve all node anchors once per frame. Actor tags are scanned once, not once per note/edge. */
    void BuildResolvedPositions(const UWorld* World, TMap<FGuid, FVector>& OutPositions) const;

    /** Returns errors (does not mutate the asset). */
    UFUNCTION(BlueprintCallable, Category="Aid|Graph")
    TArray<FString> ValidateGraph() const;
};
