#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AidEditorSubsystem.generated.h"

class UAidDocument;

/** Session-only active document and viewport visibility. Never saved into a .umap. */
UCLASS()
class UAidEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient)
    TObjectPtr<UAidDocument> ActiveDocument;

    UPROPERTY(Transient)
    bool bViewportVisible = false;

    void SetActiveDocument(UAidDocument* InDocument);
    void SetViewportVisible(bool bInVisible);

    FGuid AddNoteAt(const FVector& WorldPosition);
    FGuid ConnectLastTwo();
};
