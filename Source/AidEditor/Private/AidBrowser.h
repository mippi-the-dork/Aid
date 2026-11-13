#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAidDocument;
class SVerticalBox;
class SBox;
class IDetailsView;

/** Native-feeling docked Aid workspace: document outliner over a collapsible notes editor. */
class SAidBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAidBrowser) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override;
private:
    TSharedPtr<SVerticalBox> DocumentRows;
    TSharedPtr<SBox> DocumentHost;
    TSharedPtr<SVerticalBox> NoteRows;
    TSharedPtr<SBox> NoteHost;
    TSharedPtr<IDetailsView> Details;
    TWeakObjectPtr<UAidDocument> LastDocument;
    FGuid LastSelected;
    FGuid LastSelectedConnection;
    FString LastLevelPackage;
    FText DocumentSearch;
    FText NoteSearch;
    TSet<FGuid> ExpandedNotes;
    TSet<FName> CollapsedGroups;
    bool bNeedsDocumentRebuild = false;
    bool bNeedsNoteRebuild = false;
    int32 LastDataRevision = INDEX_NONE;
    int32 LastNoteCount = INDEX_NONE;
    int32 LastEdgeCount = INDEX_NONE;

    UAidDocument* GetDocument() const;
    FString GetLevelPackage() const;
    void ChooseDocument(const struct FAssetData& Asset);
    void SetEditingDocument(UAidDocument* Document);
    void RebuildDocuments();
    void RebuildRows();
    void AddDocumentRow(const TSharedRef<SVerticalBox>& Parent, UAidDocument* Document);
    void FocusNode(FGuid Id);
    void OpenAidSettings() const;
    FText GetModeLabel() const;
    FText GetActionInstruction() const;
    FReply SaveActiveDocument();
    FReply ToggleEditMode();
    FReply CancelAction();
    FReply Validate();
    FReply CreateNote();
    TSharedRef<SWidget> MakeColorSwatch(FGuid Id, uint8 Field, FLinearColor Color, const FText& Hint);
    TSharedRef<SWidget> MakeConnectionSwatch(FGuid Id, FLinearColor Color);
    TSharedRef<SWidget> MakeCategoryMenu(FGuid Id);
    TSharedRef<SWidget> MakeRelationshipMenu(FGuid Id);
    TSharedRef<SWidget> MakeDocumentCategoryMenu(UAidDocument* Document);
    TSharedRef<SWidget> MakeNoteSettingsMenu(FGuid Id);
    TSharedRef<SWidget> MakeTargetMenu(FGuid EdgeId);
    TSharedRef<SWidget> MakeNotePlacementMenu(FGuid Id);
};
