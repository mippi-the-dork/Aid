#pragma once

#include "CoreMinimal.h"
#include "Misc/AssetCategoryPath.h"
#include "Factories/Factory.h"
#include "AidDocumentFactory.generated.h"

/** Dedicated Content Browser asset creation for Aid Documents. */
UCLASS()
class UAidDocumentFactory : public UFactory
{
    GENERATED_BODY()
public:
    UAidDocumentFactory();
    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
        EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
    virtual FText GetDisplayName() const override;
    virtual FString GetDefaultNewAssetName() const override;
    virtual TArray<FAssetCategoryPath> GetAssetMenuPathsForCategory(FName InCategory) const override;
};
