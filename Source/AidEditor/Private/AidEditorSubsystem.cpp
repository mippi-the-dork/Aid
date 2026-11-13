#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "AidProjectSettings.h"
#include "AidViewportMode.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
    void NotifyAidChanged(UAidDocument* Document)
    {
        if (Document) Document->MarkPackageDirty();
        if (GEditor) GEditor->RedrawLevelEditingViewports();
    }
    FString CurrentLevelPackage()
    {
        const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        return World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
    }
}

void UAidEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Editor subsystem initializes independently of the Aid tab. Do not rely on a user
    // opening an Aid Document to load the level's relevant documents.
    bLastAutomaticDiscoveryEnabled = GetDefault<UAidProjectSettings>()->bAutoShowAssociatedDocuments;
    DiscoveryTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UAidEditorSubsystem::TickLevelDiscovery), .5f);
}

void UAidEditorSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(DiscoveryTicker);
    DiscoveryTicker.Reset();
    Super::Deinitialize();
}

bool UAidEditorSubsystem::TickLevelDiscovery(float DeltaTime)
{
    if (!GEditor || GEditor->PlayWorld) return true;
    const FString Level = CurrentLevelPackage();
    if (!Level.IsEmpty() && Level != LastDiscoveredLevel)
    {
        LastDiscoveredLevel = Level;
        CancelPendingAction();
        SelectedNodeId.Invalidate();
        SelectedConnectionId.Invalidate();
        ActiveDocument = nullptr;
        VisibleDocuments.Reset();
        HiddenAutoDocuments.Reset();
        AutoLoadedDocuments.Reset();
        RefreshAutomaticDocuments();
        ++DataRevision;
    }
    const bool bAuto = GetDefault<UAidProjectSettings>()->bAutoShowAssociatedDocuments;
    if (bAuto != bLastAutomaticDiscoveryEnabled)
    {
        bLastAutomaticDiscoveryEnabled = bAuto;
        RefreshAutomaticDocuments();
    }
    else if (bAutomaticScanPending)
    {
        RefreshAutomaticDocuments();
    }
    return true;
}

void UAidEditorSubsystem::RefreshAutomaticDocuments()
{
    if (!GEditor) return;
    const bool bEnable = GetDefault<UAidProjectSettings>()->bAutoShowAssociatedDocuments;
    const FString Level = CurrentLevelPackage();
    if (Level.IsEmpty() || Level.StartsWith(TEXT("/Temp/"))) return;

    TArray<UAidDocument*> Matching;
    if (bEnable)
    {
        FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        if (Registry.Get().IsLoadingAssets())
        {
            // Startup may load the editor world before Content Browser assets finish indexing.
            // Retry after registry discovery rather than silently leaving a level without Aid.
            bAutomaticScanPending = true;
            return;
        }
        TArray<FAssetData> Assets;
        Registry.Get().GetAssetsByClass(UAidDocument::StaticClass()->GetClassPathName(), Assets, true);
        for (const FAssetData& Asset : Assets)
        {
            UAidDocument* Doc = Cast<UAidDocument>(Asset.GetAsset());
            if (IsValid(Doc) && Doc->IsAssociatedWithLevel(Level)) Matching.AddUnique(Doc);
        }
    }

    bAutomaticScanPending = false;
    bool bChanged = false;
    for (int32 Index = AutoLoadedDocuments.Num() - 1; Index >= 0; --Index)
    {
        UAidDocument* Doc = AutoLoadedDocuments[Index];
        if (!IsValid(Doc) || !Matching.Contains(Doc))
        {
            VisibleDocuments.Remove(Doc);
            AutoLoadedDocuments.RemoveAt(Index);
            bChanged = true;
        }
    }
    for (UAidDocument* Doc : Matching)
    {
        if (HiddenAutoDocuments.Contains(Doc) || VisibleDocuments.Contains(Doc)) continue;
        VisibleDocuments.Add(Doc);
        AutoLoadedDocuments.AddUnique(Doc);
        bChanged = true;
    }
    if (!VisibleDocuments.IsEmpty() && !bViewportVisible) SetViewportVisible(true);
    if (VisibleDocuments.IsEmpty() && bViewportVisible && !bPlaceNoteOnClick && !PendingConnectionSource.IsValid())
        SetViewportVisible(false);
    if (bChanged) NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::SetActiveDocument(UAidDocument* InDocument)
{
    if (ActiveDocument != InDocument)
    {
        SelectedNodeId.Invalidate();
        PendingConnectionSource.Invalidate();
        bPlaceNoteOnClick = false;
        bPlaceOnActor = false;
        bHasPlacementPreview = false;
        PlacementNodeId.Invalidate();
        PlacementPreviewActor.Reset();
        SelectedConnectionId.Invalidate();
    }
    ActiveDocument = InDocument;
    if (InDocument) SetDocumentVisible(InDocument, true);
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::SetDocumentVisible(UAidDocument* InDocument, bool bVisible)
{
    if (!InDocument) return;
    if (bVisible)
    {
        HiddenAutoDocuments.Remove(InDocument);
        AutoLoadedDocuments.Remove(InDocument);
        VisibleDocuments.AddUnique(InDocument);
        if (!bViewportVisible) SetViewportVisible(true);
    }
    else
    {
        HiddenAutoDocuments.AddUnique(InDocument);
        AutoLoadedDocuments.Remove(InDocument);
        VisibleDocuments.Remove(InDocument);
        if (VisibleDocuments.IsEmpty() && bViewportVisible && !bPlaceNoteOnClick && !PendingConnectionSource.IsValid())
            SetViewportVisible(false);
    }
    NotifyAidChanged(nullptr);
}

bool UAidEditorSubsystem::IsDocumentVisible(const UAidDocument* Document) const
{
    return Document && VisibleDocuments.Contains(Document);
}

void UAidEditorSubsystem::SetViewportVisible(bool bInVisible)
{
    bViewportVisible = bInVisible;
    if (GEditor)
    {
        if (bInVisible) GLevelEditorModeTools().ActivateMode(FAidViewportMode::ModeId, false);
        else GLevelEditorModeTools().DeactivateMode(FAidViewportMode::ModeId);
        GEditor->RedrawLevelEditingViewports();
    }
}

void UAidEditorSubsystem::SetEditMode(bool bInEditMode)
{
    bEditMode = bInEditMode;
    if (!bEditMode) CancelPendingAction();
    if (bEditMode) SetViewportVisible(true);
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::ArmPlacement(bool bActor)
{
    if (!ActiveDocument) return;
    if (!SelectedNodeId.IsValid()) AddUnplacedNote();
    ArmPlacementForNode(SelectedNodeId, bActor);
}

void UAidEditorSubsystem::ArmPlacementForNode(FGuid NodeId, bool bActor)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(NodeId)) return;
    bEditMode = true;
    SelectedNodeId = NodeId;
    SelectedConnectionId.Invalidate();
    PlacementNodeId = NodeId;
    bPlaceNoteOnClick = true;
    bPlaceOnActor = bActor;
    bHasPlacementPreview = false;
    PlacementPreviewActor.Reset();
    PendingConnectionSource.Invalidate();
    SetViewportVisible(true);
}

void UAidEditorSubsystem::StagePlacement(const FVector& Position, AActor* Actor)
{
    if (!bPlaceNoteOnClick || !PlacementNodeId.IsValid()) return;
    if (bPlaceOnActor && !IsValid(Actor)) return;
    PlacementPreviewPosition = Position;
    PlacementPreviewActor = Actor;
    bHasPlacementPreview = true;
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::StageCameraPlacement(FGuid NodeId, const FVector& Position)
{
    ArmPlacementForNode(NodeId, false);
    StagePlacement(Position, nullptr);
}

bool UAidEditorSubsystem::ConfirmPlacement()
{
    if (!ActiveDocument || !bPlaceNoteOnClick || !bHasPlacementPreview) return false;
    FAidNode* Node = ActiveDocument->Nodes.FindByPredicate([this](const FAidNode& N) { return N.Id == PlacementNodeId; });
    if (!Node) return false;
    AActor* Actor = PlacementPreviewActor.Get();
    if (bPlaceOnActor && !IsValid(Actor)) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "ConfirmPlacement", "Place Aid Note"));
    ActiveDocument->Modify();
    Node->Anchor.WorldPosition = PlacementPreviewPosition;
    if (bPlaceOnActor)
    {
        Actor->Modify();
        const FName UniqueTag(*FString::Printf(TEXT("Aid_%s"), *Node->Id.ToString(EGuidFormats::Digits)));
        Actor->Tags.AddUnique(UniqueTag);
        Node->Anchor.Type = EAidAnchorType::ActorTag;
        Node->Anchor.ActorTag = UniqueTag;
        Node->Anchor.LocalOffset = Actor->GetActorTransform().InverseTransformPosition(PlacementPreviewPosition);
        Actor->MarkPackageDirty();
    }
    else
    {
        Node->Anchor.Type = EAidAnchorType::World;
        Node->Anchor.ActorTag = NAME_None;
    }
    CancelPendingAction();
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return true;
}

FGuid UAidEditorSubsystem::AddUnplacedNote()
{
    if (!ActiveDocument) return FGuid();
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "CreateUnplacedNote", "Create Aid Note"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddUnplacedNode(FText::FromString(TEXT("New Aid Note")));
    if (FAidNode* Node = ActiveDocument->Nodes.FindByPredicate([Id](const FAidNode& N) { return N.Id == Id; }))
    {
        if (const FAidCategoryPreset* Preset = GetDefault<UAidProjectSettings>()->FindCategory(Node->Category))
        {
            Node->Style.ApplyBaseColor(Preset->BaseColor);
        }
    }
    SelectedNodeId = Id;
    SelectedConnectionId.Invalidate();
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return Id;
}

void UAidEditorSubsystem::StartConnection(FGuid SourceId)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(SourceId)) return;
    bEditMode = true;
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
    PendingConnectionSource = SourceId;
    SelectedNodeId = SourceId;
    SetViewportVisible(true);
}

void UAidEditorSubsystem::CancelPendingAction()
{
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
    bHasPlacementPreview = false;
    PlacementPreviewActor.Reset();
    PlacementNodeId.Invalidate();
    PendingConnectionSource.Invalidate();
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::SelectNode(FGuid Id)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(Id)) return;
    if (PendingConnectionSource.IsValid() && PendingConnectionSource != Id)
    {
        ConnectNodes(PendingConnectionSource, Id);
        PendingConnectionSource.Invalidate();
    }
    SelectedNodeId = Id;
    SelectedConnectionId.Invalidate();
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::SelectConnection(FGuid Id)
{
    if (!ActiveDocument) return;
    const FAidConnection* Edge = ActiveDocument->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; });
    if (!Edge) return;
    SelectedConnectionId = Id;
    SelectedNodeId = Edge->SourceNode;
    NotifyAidChanged(nullptr);
}

FGuid UAidEditorSubsystem::AddNoteAt(const FVector& WorldPosition)
{
    if (!ActiveDocument) return FGuid();
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "AddNote", "Add Aid Note"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddNode(FText::FromString(TEXT("New Aid Note")), WorldPosition);
    if (FAidNode* Node = ActiveDocument->Nodes.FindByPredicate([Id](const FAidNode& N) { return N.Id == Id; }))
    {
        if (const FAidCategoryPreset* Preset = GetDefault<UAidProjectSettings>()->FindCategory(Node->Category))
        {
            Node->Style.ApplyBaseColor(Preset->BaseColor);
        }
    }
    SelectedNodeId = Id;
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return Id;
}

FGuid UAidEditorSubsystem::AddNoteAttachedToActor(AActor* Actor)
{
    if (!ActiveDocument || !IsValid(Actor)) return FGuid();
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "AttachNote", "Attach Aid Note to Actor"));
    ActiveDocument->Modify();
    Actor->Modify();
    const FVector World = Actor->GetActorTransform().TransformPosition(FVector(0., 0., 120.));
    const FGuid Id = ActiveDocument->AddNode(FText::FromString(TEXT("New Aid Note")), World);
    if (FAidNode* Node = ActiveDocument->Nodes.FindByPredicate([Id](const FAidNode& N) { return N.Id == Id; }))
    {
        // The explicitly selected actor is tagged for stable editor/PIE lookup.
        const FName UniqueTag(*FString::Printf(TEXT("Aid_%s"), *Id.ToString(EGuidFormats::Digits)));
        Actor->Tags.AddUnique(UniqueTag);
        Node->Anchor.Type = EAidAnchorType::ActorTag;
        Node->Anchor.ActorTag = UniqueTag;
        Node->Anchor.LocalOffset = FVector(0., 0., 120.);
        if (const FAidCategoryPreset* Preset = GetDefault<UAidProjectSettings>()->FindCategory(Node->Category))
        {
            Node->Style.ApplyBaseColor(Preset->BaseColor);
        }
    }
    Actor->MarkPackageDirty();
    SelectedNodeId = Id;
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return Id;
}

FGuid UAidEditorSubsystem::ConnectNodes(FGuid Source, FGuid Target)
{
    if (!ActiveDocument || Source == Target || !ActiveDocument->HasNode(Source) || !ActiveDocument->HasNode(Target)) return FGuid();
    if (ActiveDocument->HasConnectionBetween(Source, Target))
    {
        FNotificationInfo Info(NSLOCTEXT("Aid", "ConnectionExists", "These two Aid notes are already connected. Select the existing connection to edit it."));
        Info.ExpireDuration = 4.f;
        FSlateNotificationManager::Get().AddNotification(Info);
        return FGuid();
    }
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "Connect", "Connect Aid Notes"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddConnection(Source, Target, FText::FromString(TEXT("References")));
    if (FAidConnection* Edge = ActiveDocument->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; }))
    {
        if (const FAidRelationshipPreset* Preset = GetDefault<UAidProjectSettings>()->FindRelationship(Edge->RelationshipType))
        {
            Edge->Color = Preset->BaseColor;
            Edge->LineStyle = Preset->LineStyle;
            Edge->Thickness = Preset->Thickness;
            Edge->StartCap = Preset->StartCap;
            Edge->EndCap = Preset->EndCap;
        }
    }
    if (Id.IsValid()) { ++DataRevision; NotifyAidChanged(ActiveDocument); }
    return Id;
}

bool UAidEditorSubsystem::EditNode(FGuid Id, TFunctionRef<void(FAidNode&)> Edit, const FText& TransactionText)
{
    if (!ActiveDocument) return false;
    FAidNode* Node = ActiveDocument->Nodes.FindByPredicate([Id](const FAidNode& N) { return N.Id == Id; });
    if (!Node) return false;
    const FScopedTransaction Transaction(TransactionText);
    ActiveDocument->Modify();
    Edit(*Node);
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return true;
}

bool UAidEditorSubsystem::EditConnection(FGuid Id, TFunctionRef<void(FAidConnection&)> Edit, const FText& TransactionText)
{
    if (!ActiveDocument) return false;
    FAidConnection* Edge = ActiveDocument->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; });
    if (!Edge) return false;
    const FScopedTransaction Transaction(TransactionText);
    ActiveDocument->Modify();
    Edit(*Edge);
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return true;
}

bool UAidEditorSubsystem::SetNodeTitle(FGuid Id, const FText& Value)
{
    if (Value.IsEmpty()) return false;
    return EditNode(Id, [&](FAidNode& N) { N.Title = Value; }, NSLOCTEXT("Aid", "Title", "Change Aid Note Title"));
}

bool UAidEditorSubsystem::SetNodeBody(FGuid Id, const FText& Value)
{
    return EditNode(Id, [&](FAidNode& N) { N.Body = Value; }, NSLOCTEXT("Aid", "Body", "Change Aid Note Body"));
}

bool UAidEditorSubsystem::SetNodeCategory(FGuid Id, FName Value)
{
    if (!Value.IsNone() && !GetDefault<UAidProjectSettings>()->FindCategory(Value)) return false;
    return EditNode(Id, [&](FAidNode& N)
    {
        N.Category = Value;
        N.bCustomAppearance = false;
        const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
        const FLinearColor Base = Settings->FindCategory(Value)
            ? Settings->FindCategory(Value)->BaseColor : Settings->DefaultNoteColor;
        N.Style.ApplyBaseColor(Base);
    }, NSLOCTEXT("Aid", "Category", "Set Aid Note Category"));
}

bool UAidEditorSubsystem::SetConnectionLabel(FGuid Id, const FText& Value)
{
    return EditConnection(Id, [&](FAidConnection& E) { E.Label = Value; }, NSLOCTEXT("Aid", "Label", "Edit Aid Connection Label"));
}

bool UAidEditorSubsystem::SetRelationshipType(FGuid Id, FName Value)
{
    if (!Value.IsNone() && !GetDefault<UAidProjectSettings>()->FindRelationship(Value)) return false;
    return EditConnection(Id, [&](FAidConnection& E)
    {
        E.RelationshipType = Value;
        E.bCustomAppearance = false;
        const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
        if (const FAidRelationshipPreset* Preset = Settings->FindRelationship(Value))
        {
            E.Color = Preset->BaseColor;
            E.Thickness = Preset->Thickness;
            E.LineStyle = Preset->LineStyle;
            E.StartCap = Preset->StartCap;
            E.EndCap = Preset->EndCap;
        }
        else
        {
            E.Color = Settings->DefaultConnectionColor;
            E.Thickness = 2.f;
            E.LineStyle = EAidLineStyle::Solid;
            E.StartCap = EAidCapStyle::None;
            E.EndCap = EAidCapStyle::None;
        }
    }, NSLOCTEXT("Aid", "Relationship", "Set Aid Relationship Type"));
}

bool UAidEditorSubsystem::SetNodeColor(FGuid Id, uint8 Field, FLinearColor Value)
{
    // The legacy Field parameter is intentionally ignored: a note authors one base color.
    return EditNode(Id, [Value](FAidNode& N)
    {
        const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
        const FAidCategoryPreset* Preset = Settings->FindCategory(N.Category);
        const FLinearColor PresetColor = Preset ? Preset->BaseColor : Settings->DefaultNoteColor;
        N.bCustomAppearance = !Value.Equals(PresetColor, KINDA_SMALL_NUMBER);
        N.Style.ApplyBaseColor(Value);
    }, NSLOCTEXT("Aid", "Color", "Set Aid Note Base Color"));
}

bool UAidEditorSubsystem::RetargetConnection(FGuid Id, FGuid NewTarget)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(NewTarget)) return false;
    const FAidConnection* Current = ActiveDocument->Connections.FindByPredicate(
        [Id](const FAidConnection& E){ return E.Id == Id; });
    if (!Current || NewTarget == Current->SourceNode) return false;
    // Reject before opening a transaction; the data-layer also enforces this invariant.
    for (const FAidConnection& E : ActiveDocument->Connections)
    {
        if (E.Id == Id) continue;
        if ((E.SourceNode == Current->SourceNode && E.TargetNode == NewTarget) ||
            (E.TargetNode == Current->SourceNode && E.SourceNode == NewTarget)) return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "RetargetConnection", "Retarget Aid Connection"));
    ActiveDocument->Modify();
    if (!ActiveDocument->RetargetConnection(Id, NewTarget)) return false;
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return true;
}

bool UAidEditorSubsystem::RemoveNode(FGuid Id)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(Id)) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "DeleteNote", "Delete Aid Note"));
    ActiveDocument->Modify();
    if (!ActiveDocument->RemoveNode(Id)) return false;
    if (SelectedNodeId == Id) SelectedNodeId.Invalidate();
    if (PlacementNodeId == Id) CancelPendingAction();
    SelectedConnectionId.Invalidate();
    if (PendingConnectionSource == Id) PendingConnectionSource.Invalidate();
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return true;
}

bool UAidEditorSubsystem::RemoveSelectedNode() { return RemoveNode(SelectedNodeId); }

bool UAidEditorSubsystem::RemoveConnection(FGuid Id)
{
    if (!ActiveDocument) return false;
    if (!ActiveDocument->Connections.ContainsByPredicate([Id](const FAidConnection& E) { return E.Id == Id; })) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "DeleteConnection", "Delete Aid Connection"));
    ActiveDocument->Modify();
    const bool Result = ActiveDocument->RemoveConnection(Id);
    if (SelectedConnectionId == Id) SelectedConnectionId.Invalidate();
    ++DataRevision;
    NotifyAidChanged(ActiveDocument);
    return Result;
}

bool UAidEditorSubsystem::IsCurrentLevelAssociated() const
{
    return ActiveDocument && ActiveDocument->IsAssociatedWithLevel(CurrentLevelPackage());
}

bool UAidEditorSubsystem::AssociateCurrentLevel(bool bAssociate)
{
    return AssociateDocumentWithCurrentLevel(ActiveDocument.Get(), bAssociate);
}

bool UAidEditorSubsystem::AssociateDocumentWithCurrentLevel(UAidDocument* Document, bool bAssociate)
{
    if (!Document) return false;
    const FString PackageName = CurrentLevelPackage();
    if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("/Temp/"))) return false;
    if (Document->IsAssociatedWithLevel(PackageName) == bAssociate) return true;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "LevelAssociation", "Change Aid Level Association"));
    Document->Modify();
    if (bAssociate)
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
        const FSoftObjectPath LevelPath(PackageName + TEXT(".") + AssetName);
        Document->AssociatedLevels.AddUnique(TSoftObjectPtr<UWorld>(LevelPath));
    }
    else
    {
        Document->AssociatedLevels.RemoveAll([&](const TSoftObjectPtr<UWorld>& L)
        { return L.ToSoftObjectPath().GetLongPackageName() == PackageName; });
    }
    NotifyAidChanged(Document);
    RefreshAutomaticDocuments();
    return true;
}
