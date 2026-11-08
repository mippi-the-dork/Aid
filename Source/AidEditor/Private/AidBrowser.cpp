#include "AidBrowser.h"
#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "AidBrowser"

UAidDocument* SAidBrowser::GetDocument() const
{
    const UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr;
    return State ? State->ActiveDocument.Get() : nullptr;
}

void SAidBrowser::Construct(const FArguments& Args)
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs ViewArgs;
    ViewArgs.bHideSelectionTip = true;
    ViewArgs.bUpdatesFromSelection = false;
    ViewArgs.bLockable = false;
    ViewArgs.bAllowSearch = true;
    Details = PropertyEditor.CreateDetailView(ViewArgs);
    Details->SetObject(GetDocument());

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Subtitle", "Data-driven spatial notes  |  Level actors are not required"))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(UAidDocument::StaticClass())
            .ObjectPath(this, &SAidBrowser::GetCurrentPath)
            .OnObjectChanged(this, &SAidBrowser::ChooseDocument)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("AddNote", "Add Note at Camera")).OnClicked(this, &SAidBrowser::AddNote) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Connect", "Connect Last Two")).OnClicked(this, &SAidBrowser::ConnectLastTwo) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)
            [ SNew(SButton).Text(this, &SAidBrowser::GetOverlayLabel).OnClicked(this, &SAidBrowser::ToggleOverlay) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Refresh", "Refresh List")).OnClicked(this, &SAidBrowser::RefreshRows) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Validate", "Validate")).OnClicked(this, &SAidBrowser::Validate) ]
        ]
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            SNew(SSplitter).Orientation(Orient_Vertical)
            + SSplitter::Slot().Value(.38f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [ SAssignNew(NoteRows, SVerticalBox) ]
            ]
            + SSplitter::Slot().Value(.62f)
            [ Details.ToSharedRef() ]
        ]
    ];
    RebuildRows();
}

FString SAidBrowser::GetCurrentPath() const
{
    const UAidDocument* Doc = GetDocument();
    return Doc ? Doc->GetPathName() : FString();
}

void SAidBrowser::ChooseDocument(const FAssetData& Asset)
{
    UAidDocument* Doc = Cast<UAidDocument>(Asset.GetAsset());
    if (GEditor)
    {
        if (UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>())
        {
            State->SetActiveDocument(Doc);
            if (Doc)
            {
                State->SetViewportVisible(true);
            }
        }
    }
    Details->SetObject(Doc);
    RebuildRows();
}

FReply SAidBrowser::AddNote()
{
    if (UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr)
    {
        FVector Location(0., 0., 100.);
        if (GCurrentLevelEditingViewportClient)
        {
            Location = GCurrentLevelEditingViewportClient->GetViewLocation()
                + GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 600.f;
        }
        State->AddNoteAt(Location);
        State->SetViewportVisible(true);
        Details->ForceRefresh();
        RebuildRows();
    }
    return FReply::Handled();
}

FReply SAidBrowser::ConnectLastTwo()
{
    if (UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr)
    {
        State->ConnectLastTwo();
        Details->ForceRefresh();
        RebuildRows();
    }
    return FReply::Handled();
}

FReply SAidBrowser::ToggleOverlay()
{
    if (UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr)
    {
        State->SetViewportVisible(!State->bViewportVisible);
    }
    return FReply::Handled();
}

FText SAidBrowser::GetOverlayLabel() const
{
    const UAidEditorSubsystem* State = GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr;
    return State && State->bViewportVisible
        ? LOCTEXT("HideOverlay", "Hide Editor Overlay")
        : LOCTEXT("ShowOverlay", "Show Editor Overlay");
}

FReply SAidBrowser::RefreshRows()
{
    RebuildRows();
    if (Details.IsValid())
    {
        Details->ForceRefresh();
    }
    return FReply::Handled();
}

FReply SAidBrowser::Validate()
{
    const UAidDocument* Doc = GetDocument();
    const TArray<FString> Problems = Doc ? Doc->ValidateGraph() : TArray<FString>{TEXT("Select an Aid Document first.")};
    const FText Message = Problems.IsEmpty()
        ? LOCTEXT("Valid", "Aid: document is valid.")
        : FText::FromString(FString::Join(Problems, TEXT("\n")));
    FNotificationInfo Info(Message);
    Info.ExpireDuration = Problems.IsEmpty() ? 3.f : 8.f;
    FSlateNotificationManager::Get().AddNotification(Info);
    return FReply::Handled();
}

void SAidBrowser::FocusNode(FGuid Id)
{
    const UAidDocument* Doc = GetDocument();
    const FAidNode* Node = Doc ? Doc->FindNode(Id) : nullptr;
    if (Node && GCurrentLevelEditingViewportClient)
    {
        const FVector Position = Node->Anchor.Resolve(GCurrentLevelEditingViewportClient->GetWorld());
        GCurrentLevelEditingViewportClient->CenterViewportAtPoint(Position, true);
        GCurrentLevelEditingViewportClient->Invalidate();
    }
}

void SAidBrowser::RebuildRows()
{
    if (!NoteRows.IsValid())
    {
        return;
    }
    NoteRows->ClearChildren();
    const UAidDocument* Doc = GetDocument();
    if (!Doc)
    {
        NoteRows->AddSlot().AutoHeight().Padding(8.f)
        [ SNew(STextBlock).Text(LOCTEXT("ChooseDoc", "Create a Data Asset of type AidDocument and select it above."))
            .AutoWrapText(true) ];
        return;
    }

    NoteRows->AddSlot().AutoHeight().Padding(6.f)
    [ SNew(STextBlock).Text(FText::Format(LOCTEXT("NoteCount", "NOTES ({0})  •  CONNECTIONS ({1})"),
        FText::AsNumber(Doc->Nodes.Num()), FText::AsNumber(Doc->Connections.Num()))) ];
    for (const FAidNode& Node : Doc->Nodes)
    {
        const FGuid Id = Node.Id;
        NoteRows->AddSlot().AutoHeight().Padding(4.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f)
            [ SNew(STextBlock).Text(FText::Format(LOCTEXT("NoteEntry", "{0}  [{1}]"), Node.Title, FText::FromName(Node.Category)))
                .AutoWrapText(true) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(3.f)
            [ SNew(SButton).Text(LOCTEXT("Go", "Go")).OnClicked_Lambda([this, Id]() {
                FocusNode(Id);
                return FReply::Handled();
            }) ]
        ];
        for (const FAidConnection& Edge : Doc->Connections)
        {
            if (Edge.SourceNode != Id)
            {
                continue;
            }
            const FAidNode* Target = Doc->FindNode(Edge.TargetNode);
            NoteRows->AddSlot().AutoHeight().Padding(15.f, 2.f)
            [ SNew(STextBlock)
                .Text(FText::Format(LOCTEXT("EdgeEntry", "↳ {0} → {1}"),
                    Edge.Label, Target ? Target->Title : LOCTEXT("Missing", "[Missing Target]")))
                .AutoWrapText(true) ];
        }
    }
}

#undef LOCTEXT_NAMESPACE
