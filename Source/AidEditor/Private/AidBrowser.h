#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAidDocument;
class IDetailsView;
class SVerticalBox;

/** One-place authoring: every row is editable, no duplicate selected-note inspector. */
class SAidBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAidBrowser) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
private:
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SVerticalBox> NoteRows;
    TSharedPtr<SVerticalBox> DocumentRows;
    TWeakObjectPtr<UAidDocument> LastDocument;
    FGuid LastSelected;
    FString LastLevelPackage;
    int32 LastNoteCount = INDEX_NONE;
    int32 LastEdgeCount = INDEX_NONE;
    bool bNeedsRebuild = false;

    UAidDocument* GetDocument() const;
    void ChooseDocument(const struct FAssetData& Asset);
    FString GetCurrentPath() const;
    FString GetLevelPackage() const;
    void RebuildRows();
    void RebuildDocuments();
    void FocusNode(FGuid Id);
    FReply AddNote();
    FReply PlaceNote();
    FReply PlaceNoteOnActor();
    FReply ToggleEditMode();
    FReply ToggleOverlay();
    FReply CancelAction();
    FReply RefreshRows();
    FReply Validate();
    FText GetOverlayLabel() const;
    FText GetModeLabel() const;
    FText GetActionInstruction() const;
    TSharedRef<SWidget> MakeColorSwatch(FGuid Id, uint8 Field, FLinearColor Color, const FText& Hint);
    TSharedRef<SWidget> MakeConnectionSwatch(FGuid Id, FLinearColor Color);
    TSharedRef<SWidget> MakeCategoryMenu(FGuid Id);
    TSharedRef<SWidget> MakeRelationshipMenu(FGuid Id);
    TSharedRef<SWidget> MakeNoteSettingsMenu(FGuid Id);
    TSharedRef<SWidget> MakeEdgeSettingsMenu(FGuid Id);
    TSharedRef<SWidget> MakeCapMenu(FGuid Id, bool bStart);
    TSharedRef<SWidget> MakeStyleMenu(FGuid Id);
};
