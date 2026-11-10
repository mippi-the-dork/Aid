#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AidDocument.h"
#include "AidProjectSettings.generated.h"

USTRUCT(BlueprintType)
struct AID_API FAidCategoryPreset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name = TEXT("Design");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor CategoryColor = FLinearColor(0.25f, 0.8f, 0.95f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FAidCardStyle CardStyle;
};

USTRUCT(BlueprintType)
struct AID_API FAidRelationshipPreset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name = TEXT("References");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor LineColor = FLinearColor(0.25f, 0.8f, 0.95f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Thickness = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidLineStyle LineStyle = EAidLineStyle::Solid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidCapStyle StartCap = EAidCapStyle::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidCapStyle EndCap = EAidCapStyle::Arrowhead;
};

/** Shared project-wide Aid authoring presets; individual nodes and edges can be overridden. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Aid"))
class AID_API UAidProjectSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UAidProjectSettings();
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FText GetSectionText() const override { return NSLOCTEXT("Aid", "ProjectSettings", "Aid"); }

    UPROPERTY(EditAnywhere, Config, Category="Presets", meta=(TitleProperty="Name"))
    TArray<FAidCategoryPreset> Categories;

    UPROPERTY(EditAnywhere, Config, Category="Presets", meta=(TitleProperty="Name"))
    TArray<FAidRelationshipPreset> Relationships;

    const FAidCategoryPreset* FindCategory(FName Name) const;
    const FAidRelationshipPreset* FindRelationship(FName Name) const;
};
