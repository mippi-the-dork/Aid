#include "AssetDefinition_AidDocument.h"
#include "AidDocument.h"
#include "AidEditorSubsystem.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/AssetCategoryPath.h"

#define LOCTEXT_NAMESPACE "AidAssetDefinition"

FText UAssetDefinition_AidDocument::GetAssetDisplayName() const
{
    return LOCTEXT("AidDocument", "Aid Document");
}

FLinearColor UAssetDefinition_AidDocument::GetAssetColor() const
{
    return FLinearColor(0.13f, 0.68f, 0.78f);
}

TSoftClassPtr<UObject> UAssetDefinition_AidDocument::GetAssetClass() const
{
    return UAidDocument::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_AidDocument::GetAssetCategories() const
{
    static const TArray<FAssetCategoryPath> Categories = { FAssetCategoryPath(LOCTEXT("AidCategory", "Aid")) };
    return Categories;
}

EAssetCommandResult UAssetDefinition_AidDocument::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
    for (UAidDocument* Document : OpenArgs.LoadObjects<UAidDocument>())
    {
        if (Document && GEditor)
        {
            if (UAidEditorSubsystem* State = GEditor->GetEditorSubsystem<UAidEditorSubsystem>())
            {
                State->SetActiveDocument(Document);
                State->SetViewportVisible(true);
                FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("AidBrowser")));
                return EAssetCommandResult::Handled;
            }
        }
    }
    return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
