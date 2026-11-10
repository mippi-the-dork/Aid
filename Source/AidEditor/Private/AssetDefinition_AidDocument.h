#pragma once

#include "CoreMinimal.h"
#include "Misc/AssetCategoryPath.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_AidDocument.generated.h"

/** Dedicated Aid Document identity in Content Browser; opening routes to visual authoring. */
UCLASS()
class UAssetDefinition_AidDocument : public UAssetDefinitionDefault
{
    GENERATED_BODY()
public:
    virtual FText GetAssetDisplayName() const override;
    virtual FLinearColor GetAssetColor() const override;
    virtual TSoftClassPtr<UObject> GetAssetClass() const override;
    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
