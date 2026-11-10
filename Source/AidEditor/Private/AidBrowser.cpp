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
#include "ScopedTransaction.h"
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
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "AidBrowser"

namespace
{
    UAidEditorSubsystem* State() { return GEditor ? GEditor->GetEditorSubsystem<UAidEditorSubsystem>() : nullptr; }
    TSharedRef<SWidget> Label(const FText& Text) { return SNew(STextBlock).Text(Text); }
    ECheckBoxState Checked(bool b) { return b ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
}

UAidDocument* SAidBrowser::GetDocument() const
{
    return State() ? State()->ActiveDocument.Get() : nullptr;
}

FString SAidBrowser::GetCurrentPath() const
{
    return GetDocument() ? GetDocument()->GetPathName() : FString();
}

FString SAidBrowser::GetLevelPackage() const
{
    const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
}

void SAidBrowser::ChooseDocument(const FAssetData& Asset)
{
    if (State())
    {
        State()->SetActiveDocument(Cast<UAidDocument>(Asset.GetAsset()));
        if (State()->ActiveDocument) State()->SetViewportVisible(true);
    }
    if (Details.IsValid()) Details->SetObject(GetDocument());
    bNeedsRebuild = true;
}

void SAidBrowser::Construct(const FArguments& Args)
{
    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    FDetailsViewArgs DetailArgs;
    DetailArgs.bHideSelectionTip = true;
    DetailArgs.bUpdatesFromSelection = false;
    DetailArgs.bLockable = false;
    DetailArgs.bAllowSearch = true;
    Details = PropertyEditor.CreateDetailView(DetailArgs);
    Details->SetObject(GetDocument());

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 5.f)
        [ Label(LOCTEXT("Subtitle", "Aid  |  Spatial notes and relationships  |  Document-driven")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
        [ SNew(SObjectPropertyEntryBox).AllowedClass(UAidDocument::StaticClass())
            .ObjectPath(this, &SAidBrowser::GetCurrentPath).OnObjectChanged(this, &SAidBrowser::ChooseDocument) ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 3.f)
        [ SNew(STextBlock).Text(LOCTEXT("DocDescription", "DOCUMENT DESCRIPTION")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
        [ SNew(SBox).MinDesiredHeight(44.f)
            [ SNew(SMultiLineEditableTextBox)
                .IsEnabled_Lambda([this]() { return GetDocument() != nullptr; })
                .Text_Lambda([this]() { return GetDocument() ? GetDocument()->Description : FText::GetEmpty(); })
                .OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type)
                {
                    UAidDocument* Doc = GetDocument();
                    if (!Doc) return;
                    const FScopedTransaction Tx(LOCTEXT("EditDescription", "Edit Aid Document Description"));
                    Doc->Modify(); Doc->Description = Value; Doc->MarkPackageDirty();
                }) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 3.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SCheckBox)
                .IsChecked_Lambda([]() { return State() ? Checked(!State()->bShowAllDocuments) : ECheckBoxState::Checked; })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState Value)
                {
                    if (State()) State()->bShowAllDocuments = Value != ECheckBoxState::Checked;
                    RebuildDocuments();
                }) [ Label(LOCTEXT("LevelOnly", "Current level only")) ] ]
            + SHorizontalBox::Slot().AutoWidth().Padding(10.f, 2.f)
            [ SNew(SCheckBox)
                .IsEnabled_Lambda([this]() { return GetDocument() != nullptr; })
                .IsChecked_Lambda([]() { return State() ? Checked(State()->IsCurrentLevelAssociated()) : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState V)
                {
                    if (State()) State()->AssociateCurrentLevel(V == ECheckBoxState::Checked);
                    bNeedsRebuild = true;
                }) [ Label(LOCTEXT("Associate", "Associate selected document with this level")) ] ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 1.f)
        [ SNew(SExpandableArea).InitiallyCollapsed(true)
            .HeaderContent()[ Label(LOCTEXT("Documents", "Documents / Multi-document visibility")) ]
            .BodyContent()[ SAssignNew(DocumentRows, SVerticalBox) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(5.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(1.f)
            [ SNew(SButton).Text(LOCTEXT("Place", "+ Place Note"))
                .OnClicked(this, &SAidBrowser::PlaceNote) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(1.f)
            [ SNew(SButton).Text(LOCTEXT("PlaceActor", "+ Note on Actor"))
                .OnClicked(this, &SAidBrowser::PlaceNoteOnActor) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(1.f)
            [ SNew(SButton).Text(LOCTEXT("Camera", "+ Note at Camera"))
                .OnClicked(this, &SAidBrowser::AddNote) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(5.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(this, &SAidBrowser::GetModeLabel).OnClicked(this, &SAidBrowser::ToggleEditMode) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(this, &SAidBrowser::GetOverlayLabel).OnClicked(this, &SAidBrowser::ToggleOverlay) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Cancel", "Cancel Tool")).OnClicked(this, &SAidBrowser::CancelAction) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &SAidBrowser::RefreshRows) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.f)
            [ SNew(SButton).Text(LOCTEXT("Validate", "Validate")).OnClicked(this, &SAidBrowser::Validate) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
        [ SNew(STextBlock).Text(this, &SAidBrowser::GetActionInstruction).AutoWrapText(true) ]
        + SVerticalBox::Slot().FillHeight(1.f)
        [ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(NoteRows, SVerticalBox) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(5.f, 1.f)
        [ SNew(SExpandableArea).InitiallyCollapsed(true)
            .HeaderContent()[ Label(LOCTEXT("Advanced", "Raw Document Properties (diagnostics / fallback)")) ]
            .BodyContent()[ SNew(SBox).HeightOverride(240.f)[ Details.ToSharedRef() ] ] ]
    ];
    LastLevelPackage = GetLevelPackage();
    RebuildDocuments();
    RebuildRows();
}

void SAidBrowser::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    const UAidDocument* Doc = GetDocument();
    const UAidEditorSubsystem* S = State();
    if (bNeedsRebuild || Doc != LastDocument.Get() || (S && S->SelectedNodeId != LastSelected) ||
        (Doc && (Doc->Nodes.Num() != LastNoteCount || Doc->Connections.Num() != LastEdgeCount)) ||
        GetLevelPackage() != LastLevelPackage)
    {
        if (GetLevelPackage() != LastLevelPackage) LastLevelPackage = GetLevelPackage();
        if (Doc != LastDocument.Get() && Details.IsValid()) Details->SetObject(GetDocument());
        RebuildRows();
        RebuildDocuments();
        bNeedsRebuild = false;
    }
}

FReply SAidBrowser::AddNote()
{
    if (State())
    {
        FVector At(0., 0., 100.);
        if (GCurrentLevelEditingViewportClient)
            At = GCurrentLevelEditingViewportClient->GetViewLocation() + GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 600.f;
        State()->AddNoteAt(At);
        State()->SetViewportVisible(true);
        bNeedsRebuild = true;
    }
    return FReply::Handled();
}
FReply SAidBrowser::PlaceNote() { if (State()) State()->ArmPlacement(false); return FReply::Handled(); }
FReply SAidBrowser::PlaceNoteOnActor() { if (State()) State()->ArmPlacement(true); return FReply::Handled(); }
FReply SAidBrowser::ToggleEditMode() { if (State()) State()->SetEditMode(!State()->bEditMode); return FReply::Handled(); }
FReply SAidBrowser::ToggleOverlay() { if (State()) State()->SetViewportVisible(!State()->bViewportVisible); return FReply::Handled(); }
FReply SAidBrowser::CancelAction() { if (State()) State()->CancelPendingAction(); return FReply::Handled(); }
FReply SAidBrowser::RefreshRows() { bNeedsRebuild = true; if (Details.IsValid()) Details->ForceRefresh(); return FReply::Handled(); }
FText SAidBrowser::GetModeLabel() const { return State() && State()->bEditMode ? LOCTEXT("EditOn", "Edit Mode ON") : LOCTEXT("EditOff", "Review Mode"); }
FText SAidBrowser::GetOverlayLabel() const { return State() && State()->bViewportVisible ? LOCTEXT("Hide", "Hide Notes") : LOCTEXT("Show", "Show Notes"); }
FText SAidBrowser::GetActionInstruction() const
{
    const UAidEditorSubsystem* S = State();
    if (!GetDocument()) return LOCTEXT("NoDocument", "Choose or create an Aid Document; enable Show All to find documents unrelated to this level.");
    if (S && S->bPlaceNoteOnClick) return S->bPlaceOnActor
        ? LOCTEXT("ActorClick", "Click an actor in the level viewport to attach a note. The actor receives a unique Aid tag; Escape cancels.")
        : LOCTEXT("WorldClick", "Click a surface to place a note. Escape cancels.");
    if (S && S->PendingConnectionSource.IsValid()) return LOCTEXT("Connecting", "Click another note or dot in Edit Mode to connect. Escape cancels.");
    return S && S->bEditMode ? LOCTEXT("Editing", "Edit Mode: click any visible note/dot to select it; edit fields directly below.")
        : LOCTEXT("Reviewing", "Review Mode: overlay is view-only; Unreal viewport selection works normally.");
}

FReply SAidBrowser::Validate()
{
    const UAidDocument* Doc = GetDocument();
    const TArray<FString> Problems = Doc ? Doc->ValidateGraph() : TArray<FString>{TEXT("Select an Aid Document first.")};
    FNotificationInfo Info(Problems.IsEmpty() ? LOCTEXT("Valid", "Aid document is valid.") : FText::FromString(FString::Join(Problems, TEXT("\n"))));
    Info.ExpireDuration = Problems.IsEmpty() ? 3.f : 8.f;
    FSlateNotificationManager::Get().AddNotification(Info);
    return FReply::Handled();
}

void SAidBrowser::FocusNode(FGuid Id)
{
    const UAidDocument* Doc = GetDocument();
    const FAidNode* N = Doc ? Doc->FindNode(Id) : nullptr;
    if (N && GCurrentLevelEditingViewportClient)
    {
        GCurrentLevelEditingViewportClient->CenterViewportAtPoint(N->Anchor.Resolve(GCurrentLevelEditingViewportClient->GetWorld()), true);
        GCurrentLevelEditingViewportClient->Invalidate();
    }
}

TSharedRef<SWidget> SAidBrowser::MakeColorSwatch(FGuid Id, uint8 Field, FLinearColor Color, const FText& Hint)
{
    return SNew(SButton).ToolTipText(Hint).ContentPadding(1.f)
        .OnClicked_Lambda([this, Id, Field, Color]()
        {
            TWeakObjectPtr<UAidDocument> Owner = GetDocument();
            FColorPickerArgs Args;
            Args.InitialColor = Color;
            Args.bUseAlpha = true;
            Args.bOnlyRefreshOnOk = true;
            Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, Id, Field, Owner](FLinearColor NewColor)
            {
                if (Owner.IsValid() && GetDocument() == Owner.Get() && State())
                {
                    State()->SetNodeColor(Id, Field, NewColor);
                    bNeedsRebuild = true;
                }
            });
            OpenColorPicker(Args);
            return FReply::Handled();
        })
        [ SNew(SBorder).BorderBackgroundColor(Color).Padding(0.f)
            [ SNew(SBox).WidthOverride(17.f).HeightOverride(17.f) ] ];
}

TSharedRef<SWidget> SAidBrowser::MakeConnectionSwatch(FGuid Id, FLinearColor Color)
{
    return SNew(SButton).ContentPadding(1.f).ToolTipText(LOCTEXT("EdgeColor", "Connection line color"))
        .OnClicked_Lambda([this, Id, Color]()
        {
            TWeakObjectPtr<UAidDocument> Owner = GetDocument();
            FColorPickerArgs Args;
            Args.InitialColor = Color;
            Args.bOnlyRefreshOnOk = true;
            Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, Id, Owner](FLinearColor Value)
            {
                if (Owner.IsValid() && GetDocument() == Owner.Get() && State())
                {
                    State()->EditConnection(Id, [Value](FAidConnection& E) { E.Color = Value; }, LOCTEXT("EdgeColorTx", "Set Aid Connection Color"));
                    bNeedsRebuild = true;
                }
            });
            OpenColorPicker(Args);
            return FReply::Handled();
        })
        [ SNew(SBorder).BorderBackgroundColor(Color).Padding(0.f)[ SNew(SBox).WidthOverride(17.f).HeightOverride(17.f) ] ];
}

TSharedRef<SWidget> SAidBrowser::MakeCategoryMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    for (const FAidCategoryPreset& Preset : GetDefault<UAidProjectSettings>()->Categories)
    {
        const FName Name = Preset.Name;
        Menu->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SButton).Text(FText::FromName(Name)).OnClicked_Lambda([this, Id, Name]()
        {
            if (State()) State()->SetNodeCategory(Id, Name);
            bNeedsRebuild = true;
            return FReply::Handled();
        }) ];
    }
    return Menu;
}

TSharedRef<SWidget> SAidBrowser::MakeRelationshipMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    for (const FAidRelationshipPreset& Preset : GetDefault<UAidProjectSettings>()->Relationships)
    {
        const FName Name = Preset.Name;
        Menu->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SButton).Text(FText::FromName(Name)).OnClicked_Lambda([this, Id, Name]()
        {
            if (State()) State()->SetRelationshipType(Id, Name);
            bNeedsRebuild = true;
            return FReply::Handled();
        }) ];
    }
    return Menu;
}

TSharedRef<SWidget> SAidBrowser::MakeCapMenu(FGuid Id, bool bStart)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    const TArray<TPair<EAidCapStyle, FString>> Caps = {
        {EAidCapStyle::None, TEXT("None")}, {EAidCapStyle::Arrowhead, TEXT("Arrow")},
        {EAidCapStyle::Circle, TEXT("Circle")}, {EAidCapStyle::Square, TEXT("Square")}};
    for (const auto& Cap : Caps)
    {
        const EAidCapStyle Value = Cap.Key;
        Menu->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SButton).Text(FText::FromString(Cap.Value))
            .OnClicked_Lambda([this, Id, bStart, Value]()
            {
                if (State()) State()->EditConnection(Id, [bStart, Value](FAidConnection& E)
                    { if (bStart) E.StartCap = Value; else E.EndCap = Value; },
                    LOCTEXT("SetCap", "Set Aid Connection Cap"));
                bNeedsRebuild = true;
                return FReply::Handled();
            }) ];
    }
    return Menu;
}

TSharedRef<SWidget> SAidBrowser::MakeStyleMenu(FGuid Id)
{
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
    for (const TPair<EAidLineStyle, FString>& Pair : TArray<TPair<EAidLineStyle, FString>>{
        {EAidLineStyle::Solid, TEXT("Solid")}, {EAidLineStyle::Dashed, TEXT("Dashed")}, {EAidLineStyle::Dotted, TEXT("Dotted")}})
    {
        const EAidLineStyle Value = Pair.Key;
        Menu->AddSlot().AutoHeight()[ SNew(SButton).Text(FText::FromString(Pair.Value))
            .OnClicked_Lambda([this, Id, Value]()
            {
                if (State()) State()->EditConnection(Id, [Value](FAidConnection& E) { E.LineStyle = Value; },
                    LOCTEXT("LineStyleTx", "Set Aid Line Style"));
                return FReply::Handled();
            }) ];
    }
    return Menu;
}

TSharedRef<SWidget> SAidBrowser::MakeNoteSettingsMenu(FGuid Id)
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(4.f)[ Label(LOCTEXT("NoteSettings", "NOTE SETTINGS")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f)[ Label(LOCTEXT("Visible", "Visible")) ]
            + SHorizontalBox::Slot().AutoWidth()[ SNew(SCheckBox)
                .IsChecked_Lambda([this, Id]() { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return Checked(N && N->bVisible); })
                .OnCheckStateChanged_Lambda([Id](ECheckBoxState V)
                {
                    if (State()) State()->EditNode(Id, [V](FAidNode& N) { N.bVisible = V == ECheckBoxState::Checked; },
                        LOCTEXT("NoteVisibility", "Set Aid Note Visibility"));
                }) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("NoteDistance", "View distance (cm)")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SNumericEntryBox<float>).MinValue(100.f).MinSliderValue(500.f).MaxSliderValue(20000.f)
            .Value_Lambda([this, Id]() -> TOptional<float> { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return N ? TOptional<float>(N->ViewDistance) : TOptional<float>(); })
            .OnValueCommitted_Lambda([Id](float V, ETextCommit::Type)
            {
                if (State()) State()->EditNode(Id, [V](FAidNode& N) { N.ViewDistance = FMath::Max(100.f, V); },
                    LOCTEXT("NodeDistanceTx", "Set Aid Note View Distance"));
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("Border", "Border thickness (px)")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SNumericEntryBox<float>).MinValue(1.f).MaxValue(12.f)
            .Value_Lambda([this, Id]() -> TOptional<float> { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return N ? TOptional<float>(N->Style.BorderThickness) : TOptional<float>(); })
            .OnValueCommitted_Lambda([Id](float V, ETextCommit::Type)
            {
                if (State()) State()->EditNode(Id, [V](FAidNode& N) { N.Style.BorderThickness = FMath::Clamp(V, 1.f, 12.f); },
                    LOCTEXT("BorderTx", "Set Aid Note Border Thickness"));
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("Font", "Font size")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SNumericEntryBox<int32>).MinValue(8).MaxValue(96)
            .Value_Lambda([this, Id]() -> TOptional<int32> { const FAidNode* N = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return N ? TOptional<int32>(N->Style.FontSize) : TOptional<int32>(); })
            .OnValueCommitted_Lambda([Id](int32 V, ETextCommit::Type)
            {
                if (State()) State()->EditNode(Id, [V](FAidNode& N) { N.Style.FontSize = FMath::Clamp(V, 8, 96); },
                    LOCTEXT("FontTx", "Set Aid Note Font Size"));
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("Tags", "Tags (comma separated)")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SEditableTextBox)
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
                if (State()) State()->EditNode(Id, [&Names](FAidNode& N)
                {
                    N.Tags.Reset();
                    for (FString& Name : Names)
                    {
                        Name.TrimStartAndEndInline();
                        if (!Name.IsEmpty()) N.Tags.AddUnique(FName(*Name));
                    }
                }, LOCTEXT("TagsTx", "Set Aid Note Tags"));
            }) ];
}

TSharedRef<SWidget> SAidBrowser::MakeEdgeSettingsMenu(FGuid Id)
{
    auto GetEdge = [this, Id]() -> const FAidConnection*
    {
        return GetDocument() ? GetDocument()->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; }) : nullptr;
    };
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("EdgeSettings", "CONNECTION SETTINGS")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f)[ Label(LOCTEXT("ConnVisible", "Visible")) ]
            + SHorizontalBox::Slot().AutoWidth()[ SNew(SCheckBox)
                .IsChecked_Lambda([GetEdge]() { const FAidConnection* E = GetEdge(); return Checked(E && E->bVisible); })
                .OnCheckStateChanged_Lambda([Id](ECheckBoxState V)
                {
                    if (State()) State()->EditConnection(Id, [V](FAidConnection& E) { E.bVisible = V == ECheckBoxState::Checked; },
                        LOCTEXT("EdgeVisibility", "Set Aid Connection Visibility"));
                }) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("EdgeThickness", "Thickness")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SNumericEntryBox<float>).MinValue(.1f).MaxValue(20.f)
            .Value_Lambda([GetEdge]() -> TOptional<float> { const FAidConnection* E = GetEdge(); return E ? TOptional<float>(E->Thickness) : TOptional<float>(); })
            .OnValueCommitted_Lambda([Id](float V, ETextCommit::Type)
            {
                if (State()) State()->EditConnection(Id, [V](FAidConnection& E) { E.Thickness = FMath::Clamp(V, .1f, 20.f); },
                    LOCTEXT("EdgeThicknessTx", "Set Aid Connection Thickness"));
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)[ Label(LOCTEXT("EdgeDistance", "View distance (0 = unlimited)")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SNumericEntryBox<float>).MinValue(0.f)
            .Value_Lambda([GetEdge]() -> TOptional<float> { const FAidConnection* E = GetEdge(); return E ? TOptional<float>(E->ViewDistance) : TOptional<float>(); })
            .OnValueCommitted_Lambda([Id](float V, ETextCommit::Type)
            {
                if (State()) State()->EditConnection(Id, [V](FAidConnection& E) { E.ViewDistance = FMath::Max(0.f, V); },
                    LOCTEXT("EdgeDistanceTx", "Set Aid Connection View Distance"));
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(3.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SComboButton).ButtonContent()[ Label(LOCTEXT("Style", "Line style")) ].MenuContent()[ MakeStyleMenu(Id) ] ]
            + SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SComboButton).ButtonContent()[ Label(LOCTEXT("StartCap", "Start cap")) ].MenuContent()[ MakeCapMenu(Id, true) ] ]
            + SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SComboButton).ButtonContent()[ Label(LOCTEXT("EndCap", "End cap")) ].MenuContent()[ MakeCapMenu(Id, false) ] ] ];
}

void SAidBrowser::RebuildDocuments()
{
    if (!DocumentRows.IsValid()) return;
    DocumentRows->ClearChildren();
    const FString Level = GetLevelPackage();
    const bool bAll = State() && State()->bShowAllDocuments;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    ARM.Get().GetAssetsByClass(UAidDocument::StaticClass()->GetClassPathName(), Assets, true);
    Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
    int32 Count = 0;
    for (const FAssetData& Asset : Assets)
    {
        UAidDocument* Doc = Cast<UAidDocument>(Asset.GetAsset());
        if (!Doc || (!bAll && !Doc->IsAssociatedWithLevel(Level))) continue;
        ++Count;
        TWeakObjectPtr<UAidDocument> Ref = Doc;
        DocumentRows->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ SNew(SCheckBox).ToolTipText(LOCTEXT("VisibleDoc", "Draw this document's annotations in the viewport"))
                .IsChecked_Lambda([Ref]() { return State() ? Checked(Ref.IsValid() && State()->IsDocumentVisible(Ref.Get())) : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([Ref](ECheckBoxState V)
                {
                    if (Ref.IsValid() && State())
                    {
                        State()->SetDocumentVisible(Ref.Get(), V == ECheckBoxState::Checked);
                        if (V == ECheckBoxState::Checked) State()->SetViewportVisible(true);
                    }
                }) ]
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(5.f, 1.f)
            [ SNew(STextBlock).Text(FText::FromName(Asset.AssetName)) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SButton).Text(LOCTEXT("EditDocument", "Edit"))
                .OnClicked_Lambda([this, Ref]()
                {
                    if (Ref.IsValid() && State()) State()->SetActiveDocument(Ref.Get());
                    bNeedsRebuild = true;
                    return FReply::Handled();
                }) ] ];
    }
    if (!Count)
        DocumentRows->AddSlot().AutoHeight().Padding(6.f)
        [ Label(LOCTEXT("NoMatchingDocs", "No documents associated with this level. Uncheck 'Current level only' to see all, then associate one above.")) ];
}

void SAidBrowser::RebuildRows()
{
    if (!NoteRows.IsValid()) return;
    NoteRows->ClearChildren();
    const UAidDocument* Doc = GetDocument();
    const UAidEditorSubsystem* S = State();
    LastDocument = const_cast<UAidDocument*>(Doc);
    LastSelected = S ? S->SelectedNodeId : FGuid();
    LastNoteCount = Doc ? Doc->Nodes.Num() : 0;
    LastEdgeCount = Doc ? Doc->Connections.Num() : 0;
    if (!Doc)
    {
        NoteRows->AddSlot().AutoHeight().Padding(10.f)
        [ Label(LOCTEXT("ChooseDoc", "Select an Aid Document above to begin visual authoring.")) ];
        return;
    }
    NoteRows->AddSlot().AutoHeight().Padding(6.f)
    [ SNew(STextBlock).Text(FText::Format(LOCTEXT("Counts", "NOTES ({0})   CONNECTIONS ({1})"),
        FText::AsNumber(Doc->Nodes.Num()), FText::AsNumber(Doc->Connections.Num()))) ];

    for (const FAidNode& N : Doc->Nodes)
    {
        const FGuid Id = N.Id;
        const bool bSelected = S && S->SelectedNodeId == Id;
        TSharedRef<SVerticalBox> NodePanel = SNew(SVerticalBox);
        NodePanel->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeColorSwatch(Id, 0, N.Style.HeaderBackgroundColor, LOCTEXT("TitleBG", "Title background color")) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeColorSwatch(Id, 1, N.Style.HeaderTextColor, LOCTEXT("TitleFG", "Title text color")) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 1.f)
            [ SNew(SEditableTextBox).Text_Lambda([this, Id]() { const FAidNode* Node = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return Node ? Node->Title : FText::GetEmpty(); })
                .OnTextCommitted_Lambda([Id](const FText& V, ETextCommit::Type) { if (State()) State()->SetNodeTitle(Id, V); }) ] ];
        NodePanel->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeColorSwatch(Id, 2, N.Style.BodyBackgroundColor, LOCTEXT("BodyBG", "Note background color")) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeColorSwatch(Id, 3, N.Style.BodyTextColor, LOCTEXT("BodyFG", "Note text color")) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 1.f)
            [ SNew(SBox).MinDesiredHeight(52.f)
                [ SNew(SMultiLineEditableTextBox)
                    .Text_Lambda([this, Id]() { const FAidNode* Node = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return Node ? Node->Body : FText::GetEmpty(); })
                    .OnTextCommitted_Lambda([Id](const FText& V, ETextCommit::Type) { if (State()) State()->SetNodeBody(Id, V); }) ] ] ];
        NodePanel->AddSlot().AutoHeight().Padding(1.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeColorSwatch(Id, 4, N.CategoryColor, LOCTEXT("CategoryColor", "Category color")) ]
            + SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 1.f)
            [ SNew(SEditableTextBox)
                .Text_Lambda([this, Id]() { const FAidNode* Node = GetDocument() ? GetDocument()->FindNode(Id) : nullptr; return Node ? FText::FromName(Node->Category) : FText::GetEmpty(); })
                .OnTextCommitted_Lambda([this, Id](const FText& V, ETextCommit::Type)
                {
                    if (State()) State()->SetNodeCategory(Id, FName(*V.ToString()));
                    bNeedsRebuild = true;
                }) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
            [ SNew(SComboButton).ToolTipText(LOCTEXT("CategoryPreset", "Choose a project category preset"))
                .ButtonContent()[ Label(LOCTEXT("PickCat", "Presets")) ].MenuContent()[ MakeCategoryMenu(Id) ] ] ];
        NodePanel->AddSlot().AutoHeight().Padding(2.f)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
            [ SNew(SButton).Text(LOCTEXT("View", "View"))
                .OnClicked_Lambda([this, Id]() { FocusNode(Id); return FReply::Handled(); }) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
            [ SNew(SButton).Text(LOCTEXT("Connect", "Connect"))
                .OnClicked_Lambda([Id]() { if (State()) State()->StartConnection(Id); return FReply::Handled(); }) ]
            + SHorizontalBox::Slot().FillWidth(1.f)[ SNullWidget::NullWidget ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
            [ SNew(SComboButton).ToolTipText(LOCTEXT("Gear", "Note settings: tags, size, visibility, distance"))
                .ButtonContent()[ SNew(SImage).Image(FAppStyle::GetBrush("Icons.Settings")) ]
                .MenuContent()[ MakeNoteSettingsMenu(Id) ] ]
            + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
            [ SNew(SButton).ToolTipText(LOCTEXT("DeleteNote", "Delete note and its connections"))
                .OnClicked_Lambda([this, Id]()
                {
                    if (State()) State()->RemoveNode(Id);
                    bNeedsRebuild = true;
                    return FReply::Handled();
                }) [ SNew(SImage).Image(FAppStyle::GetBrush("Icons.Delete")) ] ] ];
        NoteRows->AddSlot().AutoHeight().Padding(5.f, 6.f)
        [ SNew(SBorder).BorderBackgroundColor(bSelected ? FLinearColor(0.16f, 0.40f, 0.52f) : FLinearColor(0.08f, 0.10f, 0.13f))
            .Padding(5.f)[ NodePanel ] ];
        for (const FAidConnection& E : Doc->Connections)
        {
            if (E.SourceNode != Id) continue;
            const FGuid EdgeId = E.Id;
            const FAidNode* Target = Doc->FindNode(E.TargetNode);
            NoteRows->AddSlot().AutoHeight().Padding(16.f, 1.f)
            [ SNew(SBorder).Padding(4.f)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(.52f).Padding(2.f)
                        [ SNew(SEditableTextBox).ToolTipText(LOCTEXT("EdgeLabel", "Connection label"))
                            .Text_Lambda([this, EdgeId]()
                            {
                                const FAidConnection* Edge = GetDocument() ? GetDocument()->Connections.FindByPredicate([EdgeId](const FAidConnection& C) { return C.Id == EdgeId; }) : nullptr;
                                return Edge ? Edge->Label : FText::GetEmpty();
                            })
                            .OnTextCommitted_Lambda([EdgeId](const FText& V, ETextCommit::Type) { if (State()) State()->SetConnectionLabel(EdgeId, V); }) ]
                        + SHorizontalBox::Slot().FillWidth(.48f).VAlign(VAlign_Center).Padding(3.f)
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                            [ SNew(STextBlock).Text(Target ? Target->Title : LOCTEXT("MissingTarget", "[Missing target]")) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SButton).Text(LOCTEXT("ViewTarget", "View"))
                                .ToolTipText(LOCTEXT("ViewTargetTip", "Focus the connection target even if its note is currently beyond view distance"))
                                .OnClicked_Lambda([this, TargetId = E.TargetNode]() { FocusNode(TargetId); return FReply::Handled(); }) ] ] ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(1.f)[ MakeConnectionSwatch(EdgeId, E.Color) ]
                        + SHorizontalBox::Slot().FillWidth(1.f).Padding(1.f)
                        [ SNew(SEditableTextBox).ToolTipText(LOCTEXT("Relation", "Relationship type"))
                            .Text_Lambda([this, EdgeId]()
                            {
                                const FAidConnection* Edge = GetDocument() ? GetDocument()->Connections.FindByPredicate([EdgeId](const FAidConnection& C) { return C.Id == EdgeId; }) : nullptr;
                                return Edge ? FText::FromName(Edge->RelationshipType) : FText::GetEmpty();
                            })
                            .OnTextCommitted_Lambda([this, EdgeId](const FText& V, ETextCommit::Type)
                            {
                                if (State()) State()->SetRelationshipType(EdgeId, FName(*V.ToString()));
                                bNeedsRebuild = true;
                            }) ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SComboButton).ToolTipText(LOCTEXT("RelationPreset", "Choose relationship preset"))
                            .ButtonContent()[ Label(LOCTEXT("RelPresets", "Presets")) ].MenuContent()[ MakeRelationshipMenu(EdgeId) ] ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
                        [ SNew(SComboButton).ToolTipText(LOCTEXT("EdgeGear", "Line style, caps, thickness, visibility, distance"))
                            .ButtonContent()[ SNew(SImage).Image(FAppStyle::GetBrush("Icons.Settings")) ]
                            .MenuContent()[ MakeEdgeSettingsMenu(EdgeId) ] ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(1.f)
                        [ SNew(SButton).ToolTipText(LOCTEXT("DeleteEdge", "Delete connection"))
                            .OnClicked_Lambda([this, EdgeId]()
                            {
                                if (State()) State()->RemoveConnection(EdgeId);
                                bNeedsRebuild = true;
                                return FReply::Handled();
                            }) [ SNew(SImage).Image(FAppStyle::GetBrush("Icons.Delete")) ] ] ] ] ];
        }
    }
}

#undef LOCTEXT_NAMESPACE
