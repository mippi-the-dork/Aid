#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAidDocument;
class IDetailsView;
class SVerticalBox;

class SAidBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAidBrowser) {} SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SVerticalBox> NoteRows;

    UAidDocument* GetDocument() const;
    void RebuildRows();
    FReply AddNote();
    FReply ConnectLastTwo();
    FReply ToggleOverlay();
    FReply RefreshRows();
    FReply Validate();
    void ChooseDocument(const struct FAssetData& Asset);
    void FocusNode(FGuid Id);
    FText GetOverlayLabel() const;
    FString GetCurrentPath() const;
};
