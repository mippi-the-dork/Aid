#include "AidBrowser.h"
#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "AidProjectSettings.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "ScopedTransaction.h"
#include "FileHelpers.h"
#include "UObject/Package.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "AidBrowser"

namespace
{
    UAidEditorSubsystem* AidState() { return GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr; }
    TSharedRef<SWidget> TextLabel(const FText& Value) { return SNew(STextBlock).Text(Value); }
    ECheckBoxState Check(bool Value) { return Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
    const FSlateBrush* Icon(const FName Name) { return FAppStyle::GetBrush(Name); }
    const FSlateBrush* WhiteBrush() { return FAppStyle::GetBrush("WhiteBrush"); }
    FLinearColor AidSelectionBlue() { return FAppStyle::Get().GetSlateColor("Colors.AccentBlue").GetSpecifiedColor(); }
    FText AidCapGlyph(EAidCapStyle Value)
    {
        switch (Value)
        {
        case EAidCapStyle::Arrowhead: return FText::FromString(TEXT("▲"));
        case EAidCapStyle::Circle: return FText::FromString(TEXT("●"));
        case EAidCapStyle::Square: return FText::FromString(TEXT("■"));
        default: return FText::FromString(TEXT("×"));
        }
    }
    FText AidLineGlyph(EAidLineStyle Style)
    {
        switch (Style)
        {
        case EAidLineStyle::Dashed: return FText::FromString(TEXT("━ ━"));
        case EAidLineStyle::Dotted: return FText::FromString(TEXT("· · ·"));
        default: return FText::FromString(TEXT("━━━━"));
        }
    }

    /** Preview mouse events select connection labels without consuming their text-edit events. */
    class SAidPickRegion : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SAidPickRegion) {} SLATE_EVENT(FSimpleDelegate, OnPicked)
            SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
        void Construct(const FArguments& Args) { OnPicked = Args._OnPicked; ChildSlot[Args._Content.Widget]; }
        virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
        {
            if (OnPicked.IsBound()) OnPicked.Execute();
            return FReply::Unhandled();
        }
    private:
        FSimpleDelegate OnPicked;
    };
}

UAidDocument* SAidBrowser::GetDocument() const
{
    return AidState() ? AidState()->ActiveDocument.Get() : nullptr;
}

FString SAidBrowser::GetLevelPackage() const
{
    const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
}

void SAidBrowser::SetEditingDocument(UAidDocument* Document)
{
    if (UAidEditorSubsystem* S = AidState())
    {
        S->SetActiveDocument(Document);
        if (Document) S->SetViewportVisible(true);
    }
    if (Details.IsValid()) Details->SetObject(Document);
    bNeedsDocumentRebuild = true;
    bNeedsNoteRebuild = true;
}

void SAidBrowser::ChooseDocument(const FAssetData& Asset)
{
    SetEditingDocument(Cast<UAidDocument>(Asset.GetAsset()));
}

void SAidBrowser::OpenAidSettings() const
{
    ISettingsModule& Settings = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
    Settings.ShowViewer("Project", "Plugins", "Aid");
}

void SAidBrowser::Construct(const FArguments& Args)
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs Params;
    Params.bHideSelectionTip = true;
    Params.bUpdatesFromSelection = false;
    Params.bLockable = false;
    Details = PropertyEditor.CreateDetailView(Params);
    Details->SetObject(GetDocument());

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)
        [ SNew(SSplitter).Orientation(Orient_Vertical)
            + SSplitter::Slot().Value(0.19f)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(7.f, 6.f, 7.f, 2.f))
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(LOCTEXT("DocHeading", "Documents")).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SCheckBox).ToolTipText(LOCTEXT("FilterLevelHelp", "When checked, list only documents associated with the current open level"))
                        .IsChecked_Lambda([]() { return AidState() ? Check(!AidState()->bShowAllDocuments) : ECheckBoxState::Checked; })
                        .OnCheckStateChanged_Lambda([this](ECheckBoxState V) { if (AidState()) AidState()->bShowAllDocuments = V != ECheckBoxState::Checked; bNeedsDocumentRebuild = true; })
                        [ TextLabel(LOCTEXT("CurrentLevel", "Current Level")) ] ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(STextBlock).Text(LOCTEXT("InLevelCol", "In Level"))
                        .ToolTipText(LOCTEXT("InLevelMeaning", "Each document row's checkbox controls its association with this level")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ToolTipText(LOCTEXT("PresetsSettings", "Edit Aid project presets and document categories"))
                        .OnClicked_Lambda([this]() { OpenAidSettings(); return FReply::Handled(); })
                        [ SNew(SImage).Image(Icon("Icons.Settings")) ] ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 3.f)
                [ SNew(SEditableTextBox).HintText(LOCTEXT("SearchDocuments", "Search documents..."))
                    .OnTextChanged_Lambda([this](const FText& V) { DocumentSearch = V; bNeedsDocumentRebuild = true; }) ]
                + SVerticalBox::Slot().FillHeight(1.f).Padding(4.f, 2.f)
                [ SNew(SBorder).Padding(2.f).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                    [ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(DocumentHost, SBox)[ SAssignNew(DocumentRows, SVerticalBox) ] ] ] ]
            ]
            + SSplitter::Slot().Value(0.81f)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 4.f)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text_Lambda([this]()
                        {
                            const UAidDocument* Doc = GetDocument();
                            if (!Doc) return LOCTEXT("NoDoc", "Select a document");
                            return FText::FromString(Doc->GetName() + (Doc->GetOutermost()->IsDirty() ? TEXT(" *") : TEXT("")));
                        }).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .IsEnabled_Lambda([this]() { return GetDocument() && GetDocument()->GetOutermost()->IsDirty(); })
                        .ToolTipText(LOCTEXT("SaveDocTip", "Save the selected Aid Document asset (not the level)"))
                        .OnClicked(this, &SAidBrowser::SaveActiveDocument)
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SImage).Image(Icon("Icons.Save")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[ TextLabel(LOCTEXT("Save", "Save")) ] ] ]
]
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 2.f)
                [ SNew(SMultiLineEditableTextBox)
                    .IsEnabled_Lambda([this]() { return GetDocument() != nullptr; })
                    .HintText(LOCTEXT("DescriptionHint", "Document description..."))
                    .AutoWrapText(true)
                    .Text_Lambda([this]() { return GetDocument() ? GetDocument()->Description : FText::GetEmpty(); })
                    .OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type)
                    {
                        if (UAidDocument* Doc = GetDocument())
                        {
                            if (Doc->Description.EqualTo(Value)) return;
                            const FScopedTransaction Tx(LOCTEXT("ChangeDesc", "Edit Aid Document Description"));
                            Doc->Modify(); Doc->Description = Value; Doc->MarkPackageDirty();
                        }
                    }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 4.f)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SButton).IsEnabled_Lambda([this]() { return GetDocument() != nullptr; })
                        .Text(LOCTEXT("NewNote", "+ Note")).OnClicked(this, &SAidBrowser::CreateNote) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SCheckBox).ToolTipText(LOCTEXT("EditModeHelp", "In Edit mode, click notes or start a placement/connection in the viewport"))
                        .IsChecked_Lambda([]() { return AidState() ? Check(AidState()->bEditMode) : ECheckBoxState::Unchecked; })
                        .OnCheckStateChanged_Lambda([](ECheckBoxState V) { if (AidState()) AidState()->SetEditMode(V == ECheckBoxState::Checked); })
                        [ TextLabel(LOCTEXT("EditMode", "Edit Mode")) ] ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                    [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ToolTipText(LOCTEXT("ValidateHelp", "Validate node IDs and connections"))
                        .OnClicked(this, &SAidBrowser::Validate)
                        [ SNew(SImage).Image(Icon("Icons.Check")) ] ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 2.f)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f)
                    [ SNew(SEditableTextBox).HintText(LOCTEXT("NoteSearch", "Filter notes..."))
                        .OnTextChanged_Lambda([this](const FText& V) { NoteSearch = V; bNeedsNoteRebuild = true; }) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
                    [ SNew(STextBlock).Text_Lambda([this]()
                        {
                            const UAidDocument* Doc = GetDocument();
                            return FText::Format(LOCTEXT("Counts", "Notes {0}  |  Connections {1}"),
                                FText::AsNumber(Doc ? Doc->Nodes.Num() : 0), FText::AsNumber(Doc ? Doc->Connections.Num() : 0));
                        }) ] ]
                + SVerticalBox::Slot().FillHeight(1.f).Padding(3.f)
                [ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(NoteHost, SBox)[ SAssignNew(NoteRows, SVerticalBox) ] ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(7.f, 2.f)
                [ SNew(STextBlock).Text(this, &SAidBrowser::GetActionInstruction).AutoWrapText(true)
                    .Visibility_Lambda([]()
                    {
                        const UAidEditorSubsystem* State = AidState();
                        return State && (State->bPlaceNoteOnClick || State->PendingConnectionSource.IsValid())
                            ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .ColorAndOpacity(FSlateColor::UseForeground()) ]
                + SVerticalBox::Slot().AutoHeight().Padding(4.f)
                [ SNew(SExpandableArea).InitiallyCollapsed(true)
                    .HeaderContent()[ TextLabel(LOCTEXT("Diagnostics", "Advanced Diagnostics")) ]
                    .BodyContent()[ SNew(SBox).HeightOverride(180.f)[ Details.ToSharedRef() ] ] ]
            ]
        ]
    ];
    LastLevelPackage = GetLevelPackage();
    RebuildDocuments();
    RebuildRows();
}

void SAidBrowser::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    UAidEditorSubsystem* S = AidState();
    UAidDocument* Doc = GetDocument();
    if (GetLevelPackage() != LastLevelPackage)
    {
        LastLevelPackage = GetLevelPackage();
        bNeedsDocumentRebuild = true;
    }
    if (Doc != LastDocument.Get())
    {
        if (Details.IsValid()) Details->SetObject(Doc);
        LastDocument = Doc;
        bNeedsNoteRebuild = true;
        bNeedsDocumentRebuild = true;
    }
    if (S && (S->SelectedNodeId != LastSelected || S->SelectedConnectionId != LastSelectedConnection))
    {
        if (GetDefault<UAidProjectSettings>()->bAutoCollapseUnselectedNotes)
        {
            ExpandedNotes.Reset();
            if (S->SelectedNodeId.IsValid()) ExpandedNotes.Add(S->SelectedNodeId);
        }
        bNeedsNoteRebuild = true;
        LastSelectedConnection = S->SelectedConnectionId;
        if (S->SelectedNodeId.IsValid() && !ExpandedNotes.Contains(S->SelectedNodeId))
        {
            ExpandedNotes.Add(S->SelectedNodeId);
            bNeedsNoteRebuild = true;
        }
        LastSelected = S->SelectedNodeId;
    }
    if (Doc && (Doc->Nodes.Num() != LastNoteCount || Doc->Connections.Num() != LastEdgeCount))
        bNeedsNoteRebuild = true;
    if (S && S->DataRevision != LastDataRevision) bNeedsNoteRebuild = true;
    if (bNeedsDocumentRebuild) { RebuildDocuments(); bNeedsDocumentRebuild = false; }
    if (bNeedsNoteRebuild) { RebuildRows(); bNeedsNoteRebuild = false; }
}

FText SAidBrowser::GetModeLabel() const { return AidState() && AidState()->bEditMode ? LOCTEXT("Edit", "Edit Mode") : LOCTEXT("Review", "Review Mode"); }
FText SAidBrowser::GetActionInstruction() const
{
    UAidEditorSubsystem* S = AidState();
    if (!GetDocument()) return LOCTEXT("ChooseDoc", "Select a document above, or show all to associate a document with this level.");
    if (S && S->bPlaceNoteOnClick) return S->bHasPlacementPreview
        ? LOCTEXT("PlacementReady", "Placement preview ready. Change its position with another click, then Confirm or Cancel in the viewport.")
        : LOCTEXT("PlacementPick", "Click a surface or actor in the viewport to preview the note position. Confirm or Cancel in the viewport.");
    if (S && S->PendingConnectionSource.IsValid()) return LOCTEXT("Connecting", "Click another note to connect. Escape cancels.");
    return LOCTEXT("AuthoringHint", "Create a note first, then choose World, Actor, or Camera from that note's placement menu.");
}
FReply SAidBrowser::SaveActiveDocument()
{
    UAidDocument* Doc = GetDocument();
    if (!Doc) return FReply::Handled();
    TArray<UPackage*> Packages;
    Packages.Add(Doc->GetOutermost());
    FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
    bNeedsDocumentRebuild = true;
    return FReply::Handled();
}

FReply SAidBrowser::ToggleEditMode() { if (AidState()) AidState()->SetEditMode(!AidState()->bEditMode); return FReply::Handled(); }
FReply SAidBrowser::CancelAction() { if (AidState()) AidState()->CancelPendingAction(); return FReply::Handled(); }
FReply SAidBrowser::CreateNote()
{
    if (AidState())
    {
        const FGuid Id = AidState()->AddUnplacedNote();
        ExpandedNotes.Add(Id);
        bNeedsNoteRebuild = true;
    }
    return FReply::Handled();
}
FReply SAidBrowser::Validate()
{
    const UAidDocument* Doc = GetDocument();
    const TArray<FString> Problems = Doc ? Doc->ValidateGraph() : TArray<FString>{TEXT("Select an Aid Document.")};
    FNotificationInfo Info(Problems.IsEmpty() ? LOCTEXT("Valid", "Aid document is valid") : FText::FromString(FString::Join(Problems, TEXT("\n"))));
    Info.ExpireDuration = Problems.IsEmpty() ? 3.f : 8.f;
    FSlateNotificationManager::Get().AddNotification(Info);
    return FReply::Handled();
}
void SAidBrowser::FocusNode(FGuid Id)
{
    const FAidNode* Note = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
    if (Note && Note->Anchor.Type != EAidAnchorType::Unplaced && GCurrentLevelEditingViewportClient)
    {
        GCurrentLevelEditingViewportClient->CenterViewportAtPoint(Note->Anchor.Resolve(GCurrentLevelEditingViewportClient->GetWorld()), true);
        GCurrentLevelEditingViewportClient->Invalidate();
    }
}

TSharedRef<SWidget> SAidBrowser::MakeColorSwatch(FGuid Id, uint8 Field, FLinearColor Color, const FText& Hint)
{
    return SNew(SBox).WidthOverride(24.f).HeightOverride(24.f)
        [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(Hint).ContentPadding(FMargin(1.f))
        .OnClicked_Lambda([this, Id, Field, Color]()
        {
            TWeakObjectPtr<UAidDocument> Owner = GetDocument();
            FColorPickerArgs Picker;
            Picker.InitialColor = Color;
            Picker.bUseAlpha = true;
            Picker.bOnlyRefreshOnOk = true;
            Picker.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, Owner, Id, Field](FLinearColor Value)
            {
                if (Owner.IsValid() && Owner.Get() == GetDocument() && AidState())
                    AidState()->SetNodeColor(Id, Field, Value);
            });
            OpenColorPicker(Picker);
            return FReply::Handled();
        })
        [ SNew(SBox).WidthOverride(20.f).HeightOverride(20.f)
            [ SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Color) ] ] ];
}

TSharedRef<SWidget> SAidBrowser::MakeConnectionSwatch(FGuid Id, FLinearColor Color)
{
    return SNew(SBox).WidthOverride(24.f).HeightOverride(24.f)
        [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(1.f)).ToolTipText(LOCTEXT("LineColor", "Connection color"))
        .OnClicked_Lambda([this, Id, Color]()
        {
            TWeakObjectPtr<UAidDocument> Owner = GetDocument();
            FColorPickerArgs Picker;
            Picker.InitialColor = Color;
            Picker.bUseAlpha = true;
            Picker.bOnlyRefreshOnOk = true;
            Picker.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, Owner, Id](FLinearColor Value)
            {
                if (Owner.IsValid() && Owner.Get() == GetDocument() && AidState())
                    AidState()->EditConnection(Id, [Value](FAidConnection& E)
                    {
                        const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
                        const FLinearColor PresetColor = Settings->FindRelationship(E.RelationshipType)
                            ? Settings->FindRelationship(E.RelationshipType)->BaseColor : Settings->DefaultConnectionColor;
                        E.Color = Value;
                        E.bCustomAppearance = !Value.Equals(PresetColor, KINDA_SMALL_NUMBER);
                    }, LOCTEXT("LineColorTx", "Change Aid Connection Color"));
            });
            OpenColorPicker(Picker);
            return FReply::Handled();
        })
        [ SNew(SBox).WidthOverride(20.f).HeightOverride(20.f)
            [ SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Color) ] ] ];
}

TSharedRef<SWidget> SAidBrowser::MakeCategoryMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    auto AddOption = [this, &Menu, Id](FName Name)
    {
        Menu->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SButton).Text_Lambda([this, Id, Name]()
            {
                const FAidNode* Node = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                const FString Label = (Node && Node->Category == Name ? TEXT("✓  ") : TEXT("   "))
                    + (Name.IsNone() ? FString(TEXT("Default")) : Name.ToString());
                return FText::FromString(Label);
            })
            .OnClicked_Lambda([this, Id, Name]()
            {
                if (AidState()) AidState()->SetNodeCategory(Id, Name);
                bNeedsNoteRebuild = true;
                return FReply::Handled();
            }) ];
    };
    AddOption(NAME_None);
    for (const FAidCategoryPreset& P : GetDefault<UAidProjectSettings>()->Categories) if (!P.Name.IsNone()) AddOption(P.Name);
    Menu->AddSlot().AutoHeight().Padding(4.f)[ SNew(SButton).Text(LOCTEXT("EditCategories", "Edit Presets in Project Settings..."))
        .OnClicked_Lambda([this]() { OpenAidSettings(); return FReply::Handled(); }) ];
    return SNew(SBox).WidthOverride(235.f)[ Menu ];
}

TSharedRef<SWidget> SAidBrowser::MakeRelationshipMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    auto AddOption = [this, &Menu, Id](FName Name)
    {
        Menu->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SButton).Text_Lambda([this, Id, Name]()
            {
                const FAidConnection* Edge = GetDocument() ? GetDocument()->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; }) : nullptr;
                const FString Label = (Edge && Edge->RelationshipType == Name ? TEXT("✓  ") : TEXT("   "))
                    + (Name.IsNone() ? FString(TEXT("Default")) : Name.ToString());
                return FText::FromString(Label);
            })
            .OnClicked_Lambda([this, Id, Name]()
            {
                if (AidState()) AidState()->SetRelationshipType(Id, Name);
                bNeedsNoteRebuild = true;
                return FReply::Handled();
            }) ];
    };
    AddOption(NAME_None);
    for (const FAidRelationshipPreset& P : GetDefault<UAidProjectSettings>()->Relationships) if (!P.Name.IsNone()) AddOption(P.Name);
    Menu->AddSlot().AutoHeight().Padding(4.f)[ SNew(SButton).Text(LOCTEXT("EditRelations", "Edit Presets in Project Settings..."))
        .OnClicked_Lambda([this]() { OpenAidSettings(); return FReply::Handled(); }) ];
    return SNew(SBox).WidthOverride(235.f)[ Menu ];
}

TSharedRef<SWidget> SAidBrowser::MakeDocumentCategoryMenu(UAidDocument* Document)
{
    TWeakObjectPtr<UAidDocument> Ref = Document;
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    auto AddOption = [this, &Menu, Ref](FName Name)
    {
        Menu->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SButton).Text_Lambda([Ref, Name]()
            {
                const FString Label = (Ref.IsValid() && Ref->DocumentCategory == Name ? TEXT("✓  ") : TEXT("   "))
                    + (Name.IsNone() ? FString(TEXT("None / Ungrouped")) : Name.ToString());
                return FText::FromString(Label);
            })
            .OnClicked_Lambda([this, Ref, Name]()
            {
                if (Ref.IsValid())
                {
                    const FScopedTransaction Tx(LOCTEXT("DocumentGroupTx", "Change Aid Document Category"));
                    Ref->Modify(); Ref->DocumentCategory = Name; Ref->MarkPackageDirty();
                    bNeedsDocumentRebuild = true;
                }
                return FReply::Handled();
            }) ];
    };
    AddOption(NAME_None);
    for (FName Name : GetDefault<UAidProjectSettings>()->DocumentCategories) if (!Name.IsNone()) AddOption(Name);
    Menu->AddSlot().AutoHeight().Padding(4.f)[ SNew(SButton).Text(LOCTEXT("DocumentSettings", "Edit Document Categories..."))
        .OnClicked_Lambda([this]() { OpenAidSettings(); return FReply::Handled(); }) ];
    return SNew(SBox).WidthOverride(230.f)[ Menu ];
}

TSharedRef<SWidget> SAidBrowser::MakeNotePlacementMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    Menu->AddSlot().AutoHeight().Padding(2.f)
    [ SNew(SButton).Text(LOCTEXT("PlaceSurface", "Place in World..."))
        .OnClicked_Lambda([Id]() { if (AidState()) AidState()->ArmPlacementForNode(Id, false); return FReply::Handled(); }) ];
    Menu->AddSlot().AutoHeight().Padding(2.f)
    [ SNew(SButton).Text(LOCTEXT("PlaceActor", "Attach to Actor..."))
        .OnClicked_Lambda([Id]() { if (AidState()) AidState()->ArmPlacementForNode(Id, true); return FReply::Handled(); }) ];
    Menu->AddSlot().AutoHeight().Padding(2.f)
    [ SNew(SButton).Text(LOCTEXT("PlaceCamera", "Place at Camera..."))
        .OnClicked_Lambda([Id]()
        {
            if (AidState() && GCurrentLevelEditingViewportClient)
            {
                const FVector Pos = GCurrentLevelEditingViewportClient->GetViewLocation()
                    + GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 600.f;
                AidState()->StageCameraPlacement(Id, Pos);
            }
            return FReply::Handled();
        }) ];
    return SNew(SBox).WidthOverride(180.f)[ Menu ];
}


TSharedRef<SWidget> SAidBrowser::MakeNoteSettingsMenu(FGuid Id)
{
    // Only tags require a popup; frequently edited fields are directly on the note row.
    return SNew(SBox).WidthOverride(300.f)[ SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [ SNew(STextBlock).Text(LOCTEXT("TagsHeader", "Tags"))
            .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [ SNew(STextBlock).Text(LOCTEXT("TagHint", "Use comma-separated tags; duplicates and empty names are removed."))
            .AutoWrapText(true) ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f)
        [ SNew(SEditableTextBox)
            .HintText(LOCTEXT("AddTagHint", "Puzzle, Critical, QA..."))
            .Text_Lambda([this, Id]()
            {
                const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                if (!N) return FText::GetEmpty();
                TArray<FString> Names;
                for (FName Tag : N->Tags) Names.Add(Tag.ToString());
                return FText::FromString(FString::Join(Names, TEXT(", ")));
            })
            .OnTextCommitted_Lambda([Id](const FText& Value, ETextCommit::Type)
            {
                TArray<FString> Names;
                Value.ToString().ParseIntoArray(Names, TEXT(","), true);
                if (AidState()) AidState()->EditNode(Id, [&Names](FAidNode& N)
                {
                    N.Tags.Reset();
                    for (FString& Name : Names)
                    {
                        Name.TrimStartAndEndInline();
                        if (!Name.IsEmpty()) N.Tags.AddUnique(FName(*Name));
                    }
                }, LOCTEXT("TagsTx", "Set Aid Note Tags"));
            }) ] ];
}

TSharedRef<SWidget> SAidBrowser::MakeTargetMenu(FGuid EdgeId)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    const UAidDocument* Document = GetDocument();
    const FAidConnection* Current = Document ? Document->Connections.FindByPredicate(
        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
    if (!Document || !Current) return Menu;
    const FGuid SourceId = Current->SourceNode;
    for (const FAidNode& Option : Document->Nodes)
    {
        if (Option.Id == SourceId) continue;
        const FGuid TargetId = Option.Id;
        const bool bSelected = Current->TargetNode == TargetId;
        Menu->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SButton).Text(FText::FromString(FString(bSelected ? TEXT("✓  ") : TEXT("   ")) + Option.Title.ToString()))
            .OnClicked_Lambda([this, EdgeId, TargetId]()
            {
                if (UAidEditorSubsystem* State = AidState())
                {
                    if (!State->RetargetConnection(EdgeId, TargetId))
                    {
                        FNotificationInfo Info(LOCTEXT("RetargetConflict", "This note pair already has a connection, or the target is invalid."));
                        Info.ExpireDuration = 3.f;
                        FSlateNotificationManager::Get().AddNotification(Info);
                    }
                    bNeedsNoteRebuild = true;
                }
                return FReply::Handled();
            }) ];
    }
    return SNew(SBox).WidthOverride(240.f)[ Menu ];
}

void SAidBrowser::AddDocumentRow(const TSharedRef<SVerticalBox>& Parent, UAidDocument* Document)
{
    if (!Document) return;
    TWeakObjectPtr<UAidDocument> Ref = Document;
    Parent->AddSlot().AutoHeight().Padding(1.f, 1.f)
    [ SNew(SBorder).Padding(FMargin(1.2f))
        .BorderImage(WhiteBrush())
        .BorderBackgroundColor_Lambda([Ref]()
        {
            return FSlateColor(AidState() && Ref.IsValid() && AidState()->ActiveDocument == Ref.Get()
                ? AidSelectionBlue() : FLinearColor(.12f, .13f, .14f, 1.f));
        })
        [ SNew(SBorder).Padding(FMargin(2.f, 1.f))
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(LOCTEXT("VisibilityDoc", "Show / hide the annotations from this document (per-user editor visibility)"))
                .OnClicked_Lambda([Ref]()
                {
                    if (Ref.IsValid() && AidState())
                    {
                        const bool NowVisible = !AidState()->IsDocumentVisible(Ref.Get());
                        AidState()->SetDocumentVisible(Ref.Get(), NowVisible);
                        if (NowVisible) AidState()->SetViewportVisible(true);
                    }
                    return FReply::Handled();
                })
                [ SNew(SImage).Image_Lambda([Ref]()
                    { return Icon(AidState() && Ref.IsValid() && AidState()->IsDocumentVisible(Ref.Get())
                        ? TEXT("Icons.Visible") : TEXT("Icons.Hidden")); }) ] ]
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(2.f, 0.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(LOCTEXT("EditDocument", "Select this document for note authoring"))
                .OnClicked_Lambda([this, Ref]() { if (Ref.IsValid()) SetEditingDocument(Ref.Get()); return FReply::Handled(); })
                [ SNew(STextBlock).Text_Lambda([Ref]()
                {
                    return Ref.IsValid() ? FText::FromString(Ref->GetName() + (Ref->GetOutermost()->IsDirty() ? TEXT(" *") : TEXT(""))) : FText::GetEmpty();
                }) ] ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SCheckBox)
                .ToolTipText(LOCTEXT("AssociateLevel", "Associate / disassociate this document with the current level. This changes the Aid Document, not the level."))
                .IsChecked_Lambda([this, Ref]() { return Ref.IsValid() ? Check(Ref->IsAssociatedWithLevel(GetLevelPackage())) : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this, Ref](ECheckBoxState V)
                {
                    if (Ref.IsValid() && AidState() && AidState()->AssociateDocumentWithCurrentLevel(Ref.Get(), V == ECheckBoxState::Checked))
                        bNeedsDocumentRebuild = true;
                }) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SComboButton).HasDownArrow(false).ToolTipText(LOCTEXT("DocCategoryTip", "Set the document's organizational category"))
                .ButtonContent()[ SNew(SImage).Image(Icon("Icons.Settings")) ]
                .MenuContent()[ MakeDocumentCategoryMenu(Document) ] ] ] ] ];
}

void SAidBrowser::RebuildDocuments()
{
    if (!DocumentHost.IsValid()) return;
    // Atomic swap prevents a partial layout during rapid association changes.
    DocumentRows = SNew(SVerticalBox);
    DocumentHost->SetContent(DocumentRows.ToSharedRef());
    const bool bAll = AidState() && AidState()->bShowAllDocuments;
    const FString CurrentLevel = GetLevelPackage();
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByClass(UAidDocument::StaticClass()->GetClassPathName(), Assets, true);
    Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
    TMap<FName, TArray<UAidDocument*>> Groups;
    int32 Count = 0;
    for (const FAssetData& Asset : Assets)
    {
        UAidDocument* Doc = Cast<UAidDocument>(Asset.GetAsset());
        if (!Doc || (!bAll && !Doc->IsAssociatedWithLevel(CurrentLevel))) continue;
        if (!DocumentSearch.IsEmpty() && !Doc->GetName().Contains(DocumentSearch.ToString(), ESearchCase::IgnoreCase)
            && !Doc->DocumentCategory.ToString().Contains(DocumentSearch.ToString(), ESearchCase::IgnoreCase)) continue;
        Groups.FindOrAdd(Doc->DocumentCategory).Add(Doc);
        ++Count;
    }
    if (TArray<UAidDocument*>* Ungrouped = Groups.Find(NAME_None))
        for (UAidDocument* Doc : *Ungrouped) AddDocumentRow(DocumentRows.ToSharedRef(), Doc);

    TArray<FName> GroupNames;
    Groups.GetKeys(GroupNames);
    GroupNames.Remove(NAME_None);
    GroupNames.Sort([](FName A, FName B) { return A.ToString() < B.ToString(); });
    for (FName Group : GroupNames)
    {
        TSharedRef<SVerticalBox> ChildRows = SNew(SVerticalBox);
        for (UAidDocument* Doc : Groups.FindChecked(Group)) AddDocumentRow(ChildRows, Doc);
        DocumentRows->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SExpandableArea).InitiallyCollapsed(CollapsedGroups.Contains(Group))
            .OnAreaExpansionChanged_Lambda([this, Group](bool bOpen)
            {
                if (bOpen) CollapsedGroups.Remove(Group); else CollapsedGroups.Add(Group);
            })
            .HeaderContent()[ SNew(STextBlock).Text(FText::FromName(Group)) ]
            .BodyContent()[ ChildRows ] ];
    }
    if (Count == 0)
        DocumentRows->AddSlot().AutoHeight().Padding(8.f)
        [ SNew(STextBlock).AutoWrapText(true)
            .Text(bAll ? LOCTEXT("NoDocs", "No Aid Documents found.") : LOCTEXT("NoLevelDocs", "No documents associated with this level. Uncheck Current Level to show all, then check a document's level association.")) ];
}

void SAidBrowser::RebuildRows()
{
    if (!NoteHost.IsValid()) return;
    NoteRows = SNew(SVerticalBox);
    NoteHost->SetContent(NoteRows.ToSharedRef());
    const UAidDocument* Doc = GetDocument();
    LastDocument = const_cast<UAidDocument*>(Doc);
    LastNoteCount = Doc ? Doc->Nodes.Num() : 0;
    LastEdgeCount = Doc ? Doc->Connections.Num() : 0;
    LastDataRevision = AidState() ? AidState()->DataRevision : 0;
    if (!Doc)
    {
        NoteRows->AddSlot().AutoHeight().Padding(8.f)[ TextLabel(LOCTEXT("SelectDoc", "Choose a document above to edit its notes.")) ];
        return;
    }
    const FString Filter = NoteSearch.ToString();
    for (const FAidNode& N : Doc->Nodes)
    {
        if (!Filter.IsEmpty() && !N.Title.ToString().Contains(Filter, ESearchCase::IgnoreCase)
            && !N.Body.ToString().Contains(Filter, ESearchCase::IgnoreCase)
            && !N.Category.ToString().Contains(Filter, ESearchCase::IgnoreCase)) continue;
        const FGuid Id = N.Id;
        TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
        // One base color, a labeled preset, and optional custom footer text.
        Content->AddSlot().AutoHeight().Padding(FMargin(3.f, 4.f))
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 1.f)
            [ SNew(SEditableTextBox)
                .Text_Lambda([this, Id]() { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return N ? N->Title : FText::GetEmpty(); })
                .OnTextCommitted_Lambda([Id](const FText& V, ETextCommit::Type) { if (AidState()) AidState()->SetNodeTitle(Id, V); }) ] ];
        Content->AddSlot().AutoHeight().Padding(FMargin(3.f, 2.f))
        [ SNew(SBox).MinDesiredHeight(60.f)
            [ SNew(SMultiLineEditableTextBox).AutoWrapText(true)
                .Text_Lambda([this, Id]() { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return N ? N->Body : FText::GetEmpty(); })
                .OnTextCommitted_Lambda([Id](const FText& V, ETextCommit::Type) { if (AidState()) AidState()->SetNodeBody(Id, V); }) ] ];
        Content->AddSlot().AutoHeight().Padding(FMargin(3.f, 3.f))
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(3.f, 0.f)
            [ SNew(SEditableTextBox).HintText(LOCTEXT("CustomNoteFooter", "Custom footer label"))
                .Visibility_Lambda([this, Id]()
                {
                    const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                    return N && N->bCustomAppearance ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .Text_Lambda([this, Id]()
                {
                    const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                    return N ? N->CustomFooterText : FText::GetEmpty();
                })
                .OnTextCommitted_Lambda([Id](const FText& V, ETextCommit::Type)
                {
                    if (AidState()) AidState()->EditNode(Id, [V](FAidNode& N){ N.CustomFooterText = V; },
                        LOCTEXT("CustomNoteFooterTx", "Set Aid Note Footer"));
                }) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
            [ MakeColorSwatch(Id, 0, GetDefault<UAidProjectSettings>()->ResolveNoteColor(N), LOCTEXT("BaseNoteColor", "Note base color")) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
            [ SNew(SComboButton).ToolTipText(LOCTEXT("ChooseCategory", "Note style preset"))
                .ButtonContent()[ SNew(STextBlock).Text_Lambda([this, Id]()
                {
                    const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                    return N ? FText::FromString(GetDefault<UAidProjectSettings>()->GetNotePresetLabel(*N)) : FText::GetEmpty();
                }) ]
                .MenuContent()[ MakeCategoryMenu(Id) ] ] ];
        // Type and distance are frequently edited; no settings gear required.
        Content->AddSlot().AutoHeight().Padding(FMargin(3.f, 3.f))
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f)
            [ SNew(STextBlock).Text(LOCTEXT("NoteTypeLabel", "Type")) ]
            + SHorizontalBox::Slot().FillWidth(.5f).VAlign(VAlign_Center).Padding(2.f)
            [ SNew(SComboButton).ButtonContent()[ SNew(STextBlock).Text_Lambda([this, Id]()
                {
                    const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                    const UEnum* Enum = StaticEnum<EAidNoteType>();
                    return N && Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(N->Type)) : FText::GetEmpty();
                }) ]
                .OnGetMenuContent_Lambda([Id]() -> TSharedRef<SWidget>
                {
                    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
                    const UEnum* Enum = StaticEnum<EAidNoteType>();
                    if (Enum)
                        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
                        {
                            const EAidNoteType Choice = static_cast<EAidNoteType>(Enum->GetValueByIndex(Index));
                            Menu->AddSlot().AutoHeight()[ SNew(SButton).Text(Enum->GetDisplayNameTextByIndex(Index))
                                .OnClicked_Lambda([Choice, Id]()
                                {
                                    if (AidState()) AidState()->EditNode(Id, [Choice](FAidNode& N){ N.Type = Choice; },
                                        LOCTEXT("SetNoteTypeTx", "Set Aid Note Type"));
                                    return FReply::Handled();
                                }) ];
                        }
                    return Menu;
                }) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f)
            [ SNew(STextBlock).Text(LOCTEXT("NoteDistanceInline", "Distance")) ]
            + SHorizontalBox::Slot().FillWidth(.5f).VAlign(VAlign_Center).Padding(2.f)
            [ SNew(SNumericEntryBox<float>).MinValue(100.f).MinSliderValue(500.f).MaxSliderValue(20000.f)
                .Value_Lambda([this, Id]() -> TOptional<float>
                {
                    const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr;
                    return N ? TOptional<float>(N->ViewDistance) : TOptional<float>();
                })
                .OnValueCommitted_Lambda([Id](float V, ETextCommit::Type)
                {
                    if (AidState()) AidState()->EditNode(Id, [V](FAidNode& N){ N.ViewDistance = FMath::Max(100.f, V); },
                        LOCTEXT("SetDistanceTx", "Set Aid Note View Distance"));
                }) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f)
            [ SNew(SCheckBox).ToolTipText(LOCTEXT("NoteVisibleInline", "Show this note in the viewport"))
                .IsChecked_Lambda([this, Id]() { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return Check(N && N->bVisible); })
                .OnCheckStateChanged_Lambda([Id](ECheckBoxState V)
                {
                    if (AidState()) AidState()->EditNode(Id, [V](FAidNode& N){ N.bVisible = V == ECheckBoxState::Checked; },
                        LOCTEXT("NoteVisibilityTx", "Change Aid Note Visibility"));
                }) ] ];
        int32 ConnectionCount = 0;
        TSharedRef<SVerticalBox> Connections = SNew(SVerticalBox);
        for (const FAidConnection& E : Doc->Connections)
        {
            if (E.SourceNode != Id) continue;
            ++ConnectionCount;
            const FGuid EdgeId = E.Id;
            const FGuid TargetId = E.TargetNode;
            Connections->AddSlot().AutoHeight().Padding(8.f, 2.f, 1.f, 2.f)
            [ SNew(SBorder).Padding(1.25f).BorderImage(WhiteBrush())
                .BorderBackgroundColor_Lambda([EdgeId]() { return FSlateColor(AidState() && AidState()->SelectedConnectionId == EdgeId
                    ? AidSelectionBlue() : FLinearColor(.12f, .13f, .15f, 1.f)); })
                [ SNew(SBorder).Padding(4.f).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                    [ SNew(SAidPickRegion).OnPicked_Lambda([EdgeId]() { if (AidState()) AidState()->SelectConnection(EdgeId); })
                    [ SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(1.f)
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(.51f).Padding(1.f)
                            [ SNew(SEditableTextBox).HintText(LOCTEXT("EdgeLabelHint", "Connection label"))
                                .Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* Edge = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& C) { return C.Id == EdgeId; }) : nullptr;
                                    return Edge ? Edge->Label : FText::GetEmpty();
                                })
                                .OnTextCommitted_Lambda([EdgeId](const FText& V, ETextCommit::Type)
                                    { if (AidState()) AidState()->SetConnectionLabel(EdgeId, V); }) ]
                            + SHorizontalBox::Slot().FillWidth(.49f).Padding(FMargin(12.f, 1.f, 2.f, 1.f))
                            [ SNew(SComboButton).ToolTipText(LOCTEXT("RetargetTip", "Choose a new target note without recreating this connection"))
                                .ButtonContent()[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this, EdgeId]()
                                {
                                    const UAidDocument* DocNow = GetDocument();
                                    const FAidConnection* Edge = DocNow ? DocNow->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& C){ return C.Id == EdgeId; }) : nullptr;
                                    const FAidNode* Target = Edge ? DocNow->FindNode(Edge->TargetNode) : nullptr;
                                    return Target ? Target->Title : LOCTEXT("MissingTarget", "Missing target");
                                }) ]
                                .OnGetMenuContent_Lambda([this, EdgeId]() { return MakeTargetMenu(EdgeId); }) ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(1.f)
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)
                            [ SNew(SEditableTextBox).HintText(LOCTEXT("CustomConnectionFooter", "Custom footer label"))
                                .Visibility_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* E = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& C){ return C.Id == EdgeId; }) : nullptr;
                                    return E && E->bCustomAppearance ? EVisibility::Visible : EVisibility::Collapsed;
                                })
                                .Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* E = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& C){ return C.Id == EdgeId; }) : nullptr;
                                    return E ? E->CustomFooterText : FText::GetEmpty();
                                })
                                .OnTextCommitted_Lambda([EdgeId](const FText& V, ETextCommit::Type)
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [V](FAidConnection& E){ E.CustomFooterText = V; },
                                        LOCTEXT("CustomConnectionFooterTx", "Set Aid Connection Footer"));
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ MakeConnectionSwatch(EdgeId, GetDefault<UAidProjectSettings>()->ResolveConnectionColor(E)) ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
                            [ SNew(SComboButton).ToolTipText(LOCTEXT("RelPresetTip", "Relationship style preset"))
                                .ButtonContent()[ SNew(STextBlock).Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* Edge = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& C) { return C.Id == EdgeId; }) : nullptr;
                                    return Edge ? FText::FromString(GetDefault<UAidProjectSettings>()->GetConnectionPresetLabel(*Edge)) : FText::GetEmpty();
                                }) ]
                                .MenuContent()[ MakeRelationshipMenu(EdgeId) ] ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(1.f)
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ToolTipText(LOCTEXT("StartCapCycle", "Start cap: None → Arrow → Circle → Square"))
                                .Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return AidCapGlyph(C ? C->StartCap : EAidCapStyle::None);
                                })
                                .OnClicked_Lambda([EdgeId]()
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [](FAidConnection& C)
                                    { C.StartCap = static_cast<EAidCapStyle>((static_cast<int32>(C.StartCap) + 1) % 4); },
                                        LOCTEXT("CycleStartCap", "Cycle Aid Start Cap"));
                                    return FReply::Handled();
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ToolTipText(LOCTEXT("StyleCycle", "Line: Solid → Dashed → Dotted"))
                                .Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return AidLineGlyph(C ? C->LineStyle : EAidLineStyle::Solid);
                                })
                                .OnClicked_Lambda([EdgeId]()
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [](FAidConnection& C)
                                    { C.LineStyle = static_cast<EAidLineStyle>((static_cast<int32>(C.LineStyle) + 1) % 3); },
                                        LOCTEXT("CycleStyle", "Cycle Aid Line Style"));
                                    return FReply::Handled();
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ToolTipText(LOCTEXT("EndCapCycle", "End cap: None → Arrow → Circle → Square"))
                                .Text_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return AidCapGlyph(C ? C->EndCap : EAidCapStyle::None);
                                })
                                .OnClicked_Lambda([EdgeId]()
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [](FAidConnection& C)
                                    { C.EndCap = static_cast<EAidCapStyle>((static_cast<int32>(C.EndCap) + 1) % 4); },
                                        LOCTEXT("CycleEndCap", "Cycle Aid End Cap"));
                                    return FReply::Handled();
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f)
                            [ SNew(STextBlock).Text(LOCTEXT("ConnectionThickness", "Width")) ]
                            + SHorizontalBox::Slot().FillWidth(.5f).Padding(2.f)
                            [ SNew(SNumericEntryBox<float>).MinValue(.1f).MaxValue(20.f)
                                .Value_Lambda([this, EdgeId]() -> TOptional<float>
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return C ? TOptional<float>(C->Thickness) : TOptional<float>();
                                })
                                .OnValueCommitted_Lambda([EdgeId](float V, ETextCommit::Type)
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [V](FAidConnection& C){ C.Thickness = FMath::Clamp(V, .1f, 20.f); },
                                        LOCTEXT("EdgeThicknessTx", "Set Aid Connection Thickness"));
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f)
                            [ SNew(STextBlock).Text(LOCTEXT("ConnectionDistance", "Distance")) ]
                            + SHorizontalBox::Slot().FillWidth(.5f).Padding(2.f)
                            [ SNew(SNumericEntryBox<float>).MinValue(0.f)
                                .Value_Lambda([this, EdgeId]() -> TOptional<float>
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return C ? TOptional<float>(C->ViewDistance) : TOptional<float>();
                                })
                                .OnValueCommitted_Lambda([EdgeId](float V, ETextCommit::Type)
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [V](FAidConnection& C){ C.ViewDistance = FMath::Max(0.f, V); },
                                        LOCTEXT("EdgeDistanceTx", "Set Aid Connection View Distance"));
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
                            [ SNew(SCheckBox).ToolTipText(LOCTEXT("ConnectionVisible", "Show this connection"))
                                .IsChecked_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    return Check(C && C->bVisible);
                                })
                                .OnCheckStateChanged_Lambda([EdgeId](ECheckBoxState V)
                                {
                                    if (AidState()) AidState()->EditConnection(EdgeId, [V](FAidConnection& C){ C.bVisible = V == ECheckBoxState::Checked; },
                                        LOCTEXT("ConnVisibleTx", "Toggle Aid Connection Visibility"));
                                }) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ToolTipText(LOCTEXT("FocusTarget", "View target note"))
                                .OnClicked_Lambda([this, EdgeId]()
                                {
                                    const FAidConnection* C = GetDocument() ? GetDocument()->Connections.FindByPredicate(
                                        [EdgeId](const FAidConnection& E){ return E.Id == EdgeId; }) : nullptr;
                                    if (C) FocusNode(C->TargetNode);
                                    return FReply::Handled();
                                })
                                [ SNew(SImage).Image(Icon("Icons.Search")) ] ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
                            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ToolTipText(LOCTEXT("DeleteConnection", "Delete connection"))
                                .OnClicked_Lambda([this, EdgeId]()
                                {
                                    if (AidState()) AidState()->RemoveConnection(EdgeId);
                                    bNeedsNoteRebuild = true;
                                    return FReply::Handled();
                                })
                                [ SNew(SImage).Image(Icon("Icons.Delete")) ] ] ] ] ] ] ];
        }
        if (ConnectionCount)
        {
            Content->AddSlot().AutoHeight().Padding(4.f, 5.f)
            [ SNew(STextBlock).Text(FText::Format(LOCTEXT("EdgeCount", "Connections ({0})"), FText::AsNumber(ConnectionCount))) ];
            Content->AddSlot().AutoHeight()[ Connections ];
        }

        const bool bExpanded = ExpandedNotes.Contains(Id);
        const bool bPlaced = N.Anchor.Type != EAidAnchorType::Unplaced;
        const FString Name = N.Title.ToString();
        TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.f)
            [ SNew(SImage).Image(Icon(bPlaced ? TEXT("Icons.Edit") : TEXT("Icons.Transform"))) ];
        Header->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).Padding(3.f, 0.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(bPlaced ? LOCTEXT("SelectNote", "Select note") : LOCTEXT("Unplaced", "Unplaced note: choose its placement from the pin menu"))
                .OnClicked_Lambda([this, Id]()
                {
                    if (AidState()) AidState()->SelectNode(Id);
                    ExpandedNotes.Add(Id);
                    bNeedsNoteRebuild = true;
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(Name)) ] ];
        if (!bPlaced)
            Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f)
                [ SNew(STextBlock).Text(LOCTEXT("NotPlacedStatus", "Not Placed"))
                    .ColorAndOpacity(FLinearColor(.55f, .67f, .76f)) ];
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SComboButton).HasDownArrow(false)
                .ToolTipText(LOCTEXT("PlaceNoteTip", "Position this note in the world, on an actor, or at camera"))
                .ButtonContent()[ SNew(SImage).Image(Icon("Icons.Transform")) ]
                .MenuContent()[ MakeNotePlacementMenu(Id) ] ];
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .IsEnabled(bPlaced).ToolTipText(LOCTEXT("ViewNote", "View note in viewport"))
                .OnClicked_Lambda([this, Id]() { FocusNode(Id); return FReply::Handled(); })
                [ SNew(SImage).Image(Icon("Icons.Search")) ] ];
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(LOCTEXT("ConnectNote", "Draw a connection from this note to another"))
                .OnClicked_Lambda([Id]() { if (AidState()) AidState()->StartConnection(Id); return FReply::Handled(); })
                [ SNew(SImage).Image(Icon("Icons.Link")) ] ];
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SComboButton).HasDownArrow(false).ToolTipText(LOCTEXT("NoteOptions", "Edit tags"))
                .ButtonContent()[ SNew(SImage).Image(Icon("Icons.Settings")) ]
                .MenuContent()[ MakeNoteSettingsMenu(Id) ] ];
        Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(1.f)
            [ SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(LOCTEXT("DeleteNote", "Delete this note and its connections"))
                .OnClicked_Lambda([this, Id]()
                {
                    if (AidState()) AidState()->RemoveNode(Id);
                    ExpandedNotes.Remove(Id);
                    bNeedsNoteRebuild = true;
                    return FReply::Handled();
                })
                [ SNew(SImage).Image(Icon("Icons.Delete")) ] ];
        NoteRows->AddSlot().AutoHeight().Padding(4.f, 2.f)
        [ SNew(SBorder).Padding(1.25f).BorderImage(WhiteBrush())
            .BorderBackgroundColor_Lambda([Id]()
            {
                return FSlateColor(AidState() && AidState()->SelectedNodeId == Id
                    ? AidSelectionBlue() : FLinearColor(.14f, .15f, .16f, 1.f));
            })
            [ SNew(SBorder).Padding(FMargin(4.f, 2.f))
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [ SNew(SExpandableArea).InitiallyCollapsed(!bExpanded)
            .OnAreaExpansionChanged_Lambda([this, Id](bool bOpen)
            {
                if (bOpen)
                {
                    if (GetDefault<UAidProjectSettings>()->bAutoCollapseUnselectedNotes)
                    {
                        ExpandedNotes.Reset();
                        if (AidState()) AidState()->SelectNode(Id);
                        bNeedsNoteRebuild = true;
                    }
                    ExpandedNotes.Add(Id);
                }
                else ExpandedNotes.Remove(Id);
            })
            .HeaderContent()[ Header ]
            .BodyContent()[ Content ] ] ] ];
    }
}

#undef LOCTEXT_NAMESPACE
