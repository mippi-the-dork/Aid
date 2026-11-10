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
#include "ScopedTransaction.h"

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

void UAidEditorSubsystem::SetActiveDocument(UAidDocument* InDocument)
{
    if (ActiveDocument != InDocument)
    {
        SelectedNodeId.Invalidate();
        PendingConnectionSource.Invalidate();
        bPlaceNoteOnClick = false;
        bPlaceOnActor = false;
    }
    ActiveDocument = InDocument;
    if (InDocument) SetDocumentVisible(InDocument, true);
    NotifyAidChanged(nullptr);
}

void UAidEditorSubsystem::SetDocumentVisible(UAidDocument* InDocument, bool bVisible)
{
    if (!InDocument) return;
    if (bVisible) VisibleDocuments.AddUnique(InDocument);
    else VisibleDocuments.Remove(InDocument);
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
    bEditMode = true;
    bPlaceNoteOnClick = true;
    bPlaceOnActor = bActor;
    PendingConnectionSource.Invalidate();
    SetViewportVisible(true);
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
            Node->Style = Preset->CardStyle;
            Node->CategoryColor = Preset->CategoryColor;
        }
    }
    SelectedNodeId = Id;
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
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
            Node->Style = Preset->CardStyle;
            Node->CategoryColor = Preset->CategoryColor;
        }
    }
    Actor->MarkPackageDirty();
    SelectedNodeId = Id;
    bPlaceNoteOnClick = false;
    bPlaceOnActor = false;
    NotifyAidChanged(ActiveDocument);
    return Id;
}

FGuid UAidEditorSubsystem::ConnectNodes(FGuid Source, FGuid Target)
{
    if (!ActiveDocument || Source == Target || !ActiveDocument->HasNode(Source) || !ActiveDocument->HasNode(Target)) return FGuid();
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "Connect", "Connect Aid Notes"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddConnection(Source, Target, FText::FromString(TEXT("References")));
    if (FAidConnection* Edge = ActiveDocument->Connections.FindByPredicate([Id](const FAidConnection& E) { return E.Id == Id; }))
    {
        if (const FAidRelationshipPreset* Preset = GetDefault<UAidProjectSettings>()->FindRelationship(Edge->RelationshipType))
        {
            Edge->Color = Preset->LineColor;
            Edge->LineStyle = Preset->LineStyle;
            Edge->Thickness = Preset->Thickness;
            Edge->StartCap = Preset->StartCap;
            Edge->EndCap = Preset->EndCap;
        }
    }
    if (Id.IsValid()) NotifyAidChanged(ActiveDocument);
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
    return EditNode(Id, [&](FAidNode& N)
    {
        N.Category = Value;
        if (const FAidCategoryPreset* Preset = GetDefault<UAidProjectSettings>()->FindCategory(Value))
        {
            N.CategoryColor = Preset->CategoryColor;
            N.Style = Preset->CardStyle;
        }
    }, NSLOCTEXT("Aid", "Category", "Set Aid Note Category"));
}

bool UAidEditorSubsystem::SetConnectionLabel(FGuid Id, const FText& Value)
{
    return EditConnection(Id, [&](FAidConnection& E) { E.Label = Value; }, NSLOCTEXT("Aid", "Label", "Edit Aid Connection Label"));
}

bool UAidEditorSubsystem::SetRelationshipType(FGuid Id, FName Value)
{
    return EditConnection(Id, [&](FAidConnection& E)
    {
        E.RelationshipType = Value;
        if (const FAidRelationshipPreset* Preset = GetDefault<UAidProjectSettings>()->FindRelationship(Value))
        {
            E.Color = Preset->LineColor;
            E.Thickness = Preset->Thickness;
            E.LineStyle = Preset->LineStyle;
            E.StartCap = Preset->StartCap;
            E.EndCap = Preset->EndCap;
        }
    }, NSLOCTEXT("Aid", "Relationship", "Set Aid Relationship Type"));
}

bool UAidEditorSubsystem::SetNodeColor(FGuid Id, uint8 Field, FLinearColor Value)
{
    return EditNode(Id, [&](FAidNode& N)
    {
        switch (Field)
        {
        case 0: N.Style.HeaderBackgroundColor = Value; break;
        case 1: N.Style.HeaderTextColor = Value; break;
        case 2: N.Style.BodyBackgroundColor = Value; break;
        case 3: N.Style.BodyTextColor = Value; break;
        case 4: N.CategoryColor = Value; break;
        default: break;
        }
    }, NSLOCTEXT("Aid", "Color", "Set Aid Note Color"));
}

bool UAidEditorSubsystem::RemoveNode(FGuid Id)
{
    if (!ActiveDocument || !ActiveDocument->HasNode(Id)) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "DeleteNote", "Delete Aid Note"));
    ActiveDocument->Modify();
    if (!ActiveDocument->RemoveNode(Id)) return false;
    if (SelectedNodeId == Id) SelectedNodeId.Invalidate();
    if (PendingConnectionSource == Id) PendingConnectionSource.Invalidate();
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
    NotifyAidChanged(ActiveDocument);
    return Result;
}

bool UAidEditorSubsystem::IsCurrentLevelAssociated() const
{
    return ActiveDocument && ActiveDocument->IsAssociatedWithLevel(CurrentLevelPackage());
}

bool UAidEditorSubsystem::AssociateCurrentLevel(bool bAssociate)
{
    if (!ActiveDocument) return false;
    const FString PackageName = CurrentLevelPackage();
    if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("/Temp/"))) return false;
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "LevelAssociation", "Change Aid Level Association"));
    ActiveDocument->Modify();
    if (bAssociate)
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
        const FSoftObjectPath LevelPath(PackageName + TEXT(".") + AssetName);
        ActiveDocument->AssociatedLevels.AddUnique(TSoftObjectPtr<UWorld>(LevelPath));
    }
    else
    {
        ActiveDocument->AssociatedLevels.RemoveAll([&](const TSoftObjectPtr<UWorld>& L)
        { return L.ToSoftObjectPath().GetLongPackageName() == PackageName; });
    }
    NotifyAidChanged(ActiveDocument);
    return true;
}
