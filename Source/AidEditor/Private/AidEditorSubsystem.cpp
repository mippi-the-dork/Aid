#include "AidEditorSubsystem.h"
#include "AidDocument.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "ScopedTransaction.h"
#include "AidViewportMode.h"

void UAidEditorSubsystem::SetActiveDocument(UAidDocument* InDocument)
{
    ActiveDocument = InDocument;
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

void UAidEditorSubsystem::SetViewportVisible(bool bInVisible)
{
    bViewportVisible = bInVisible;
    if (GEditor)
    {
        if (bInVisible)
        {
            GLevelEditorModeTools().ActivateMode(FAidViewportMode::ModeId, false);
        }
        else
        {
            GLevelEditorModeTools().DeactivateMode(FAidViewportMode::ModeId);
        }
        GEditor->RedrawLevelEditingViewports();
    }
}

FGuid UAidEditorSubsystem::AddNoteAt(const FVector& WorldPosition)
{
    if (!ActiveDocument)
    {
        return FGuid();
    }
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "AddNote", "Add Aid Note"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddNode(FText::FromString(TEXT("New Aid Note")), WorldPosition);
    ActiveDocument->MarkPackageDirty();
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
    return Id;
}

FGuid UAidEditorSubsystem::ConnectLastTwo()
{
    if (!ActiveDocument || ActiveDocument->Nodes.Num() < 2)
    {
        return FGuid();
    }
    const int32 Count = ActiveDocument->Nodes.Num();
    const FScopedTransaction Transaction(NSLOCTEXT("Aid", "Connect", "Add Aid Connection"));
    ActiveDocument->Modify();
    const FGuid Id = ActiveDocument->AddConnection(
        ActiveDocument->Nodes[Count - 2].Id,
        ActiveDocument->Nodes[Count - 1].Id,
        FText::FromString(TEXT("References")));
    if (Id.IsValid())
    {
        ActiveDocument->MarkPackageDirty();
    }
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
    return Id;
}
