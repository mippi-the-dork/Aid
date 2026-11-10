#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
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
    UPROPERTY(Transient) TObjectPtr<UAidDocument> ActiveDocument;
    UPROPERTY(Transient) TArray<TObjectPtr<UAidDocument>> VisibleDocuments;
    UPROPERTY(Transient) bool bViewportVisible = false;
    UPROPERTY(Transient) bool bEditMode = false;
    UPROPERTY(Transient) bool bPlaceNoteOnClick = false;
    UPROPERTY(Transient) bool bPlaceOnActor = false;
    UPROPERTY(Transient) bool bShowAllDocuments = false;
    UPROPERTY(Transient) FGuid SelectedNodeId;
    UPROPERTY(Transient) FGuid PendingConnectionSource;

    void SetActiveDocument(UAidDocument* InDocument);
    void SetDocumentVisible(UAidDocument* InDocument, bool bVisible);
    bool IsDocumentVisible(const UAidDocument* Document) const;
    void SetViewportVisible(bool bInVisible);
    void SetEditMode(bool bInEditMode);
    void ArmPlacement(bool bActor);
    void StartConnection(FGuid SourceId);
    void CancelPendingAction();
    void SelectNode(FGuid Id);
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
    bool RemoveSelectedNode();
    bool AssociateCurrentLevel(bool bAssociate);
    bool IsCurrentLevelAssociated() const;
};
