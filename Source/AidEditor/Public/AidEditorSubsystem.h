#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Containers/Ticker.h"
#include "AidDocument.h"
#include "AidEditorSubsystem.generated.h"

class AActor;
class UAidDocument;

/** Session editing state. One document is edited at a time; any number can be displayed. */
UCLASS()
class UAidEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(Transient) TObjectPtr<UAidDocument> ActiveDocument;
    UPROPERTY(Transient) TArray<TObjectPtr<UAidDocument>> VisibleDocuments;
    /** A document manually hidden by its eye stays hidden when automatic discovery refreshes. */
    UPROPERTY(Transient) TArray<TObjectPtr<UAidDocument>> HiddenAutoDocuments;
    /** Tracks only auto-enabled eyes, so manual Show stays independent of level association. */
    UPROPERTY(Transient) TArray<TObjectPtr<UAidDocument>> AutoLoadedDocuments;
    UPROPERTY(Transient) bool bViewportVisible = false;
    UPROPERTY(Transient) bool bEditMode = false;
    UPROPERTY(Transient) bool bPlaceNoteOnClick = false;
    UPROPERTY(Transient) bool bPlaceOnActor = false;
    UPROPERTY(Transient) bool bShowAllDocuments = false;
    UPROPERTY(Transient) FGuid SelectedNodeId;
    UPROPERTY(Transient) int32 DataRevision = 0;
    UPROPERTY(Transient) FGuid PendingConnectionSource;
    UPROPERTY(Transient) FGuid SelectedConnectionId;
    UPROPERTY(Transient) FGuid PlacementNodeId;
    UPROPERTY(Transient) bool bHasPlacementPreview = false;
    UPROPERTY(Transient) FVector PlacementPreviewPosition = FVector::ZeroVector;
    UPROPERTY(Transient) TWeakObjectPtr<AActor> PlacementPreviewActor;

    void SetActiveDocument(UAidDocument* InDocument);
    void SetDocumentVisible(UAidDocument* InDocument, bool bVisible);
    bool IsDocumentVisible(const UAidDocument* Document) const;
    void SetViewportVisible(bool bInVisible);
    void SetEditMode(bool bInEditMode);
    void ArmPlacement(bool bActor); // Legacy entry point; uses selected note or creates one.
    void ArmPlacementForNode(FGuid NodeId, bool bActor);
    void StagePlacement(const FVector& Position, AActor* Actor);
    void StageCameraPlacement(FGuid NodeId, const FVector& Position);
    bool ConfirmPlacement();
    FGuid AddUnplacedNote();
    void StartConnection(FGuid SourceId);
    void CancelPendingAction();
    void SelectNode(FGuid Id);
    void SelectConnection(FGuid Id);
    FGuid AddNoteAt(const FVector& WorldPosition);
    FGuid AddNoteAttachedToActor(AActor* Actor);
    FGuid ConnectNodes(FGuid Source, FGuid Target);
    bool SetNodeTitle(FGuid Id, const FText& InTitle);
    bool SetNodeBody(FGuid Id, const FText& InBody);
    bool SetNodeCategory(FGuid Id, FName InCategory);
    bool SetConnectionLabel(FGuid Id, const FText& InLabel);
    bool SetRelationshipType(FGuid Id, FName InType);
    bool SetNodeColor(FGuid Id, uint8 ColorField, FLinearColor InColor);
    bool EditNode(FGuid Id, TFunctionRef<void(FAidNode&)> Edit, const FText& TransactionText);
    bool EditConnection(FGuid Id, TFunctionRef<void(FAidConnection&)> Edit, const FText& TransactionText);
    bool RemoveNode(FGuid Id);
    bool RemoveConnection(FGuid Id);
    bool RetargetConnection(FGuid Id, FGuid NewTarget);
    bool RemoveSelectedNode();
    bool AssociateCurrentLevel(bool bAssociate);
    bool AssociateDocumentWithCurrentLevel(UAidDocument* Document, bool bAssociate);
    bool IsCurrentLevelAssociated() const;
    /** Reconciles level-associated documents even when the Aid tab has never been opened. */
    void RefreshAutomaticDocuments();
private:
    bool TickLevelDiscovery(float DeltaTime);
    FString LastDiscoveredLevel;
    bool bLastAutomaticDiscoveryEnabled = true;
    /** An initial level scan may occur before the Asset Registry finishes loading. */
    bool bAutomaticScanPending = false;
    FTSTicker::FDelegateHandle DiscoveryTicker;
};
