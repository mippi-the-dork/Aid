#include "AidDocumentFactory.h"
#include "AidDocument.h"
#include "Misc/AssetCategoryPath.h"

#define LOCTEXT_NAMESPACE "AidDocumentFactory"

UAidDocumentFactory::UAidDocumentFactory()
{
    SupportedClass = UAidDocument::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UAidDocumentFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
    EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<UAidDocument>(InParent, InClass, InName, Flags | RF_Transactional);
}

FText UAidDocumentFactory::GetDisplayName() const
{
    return LOCTEXT("AidDocumentName", "Aid Document");
}

FString UAidDocumentFactory::GetDefaultNewAssetName() const
{
    return TEXT("AD_NewDocument");
}

TArray<FAssetCategoryPath> UAidDocumentFactory::GetAssetMenuPathsForCategory(FName InCategory) const
{
    return { FAssetCategoryPath(LOCTEXT("AidMenu", "Aid")) };
}

#undef LOCTEXT_NAMESPACE
