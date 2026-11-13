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
    /** Single authorable color for note title; body, footer, outlines and text are derived. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor BaseColor = FLinearColor(.07f, .27f, .38f);
};

USTRUCT(BlueprintType)
struct AID_API FAidRelationshipPreset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name = TEXT("References");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor BaseColor = FLinearColor(.25f, .8f, .95f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Thickness = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidLineStyle LineStyle = EAidLineStyle::Solid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidCapStyle StartCap = EAidCapStyle::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAidCapStyle EndCap = EAidCapStyle::None;
};

/** Shared project-wide Aid authoring presets; individual nodes and edges can be overridden. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Aid"))
class AID_API UAidProjectSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UAidProjectSettings();
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FName GetSectionName() const override { return TEXT("Aid"); }
    virtual FText GetSectionText() const override { return NSLOCTEXT("Aid", "ProjectSettings", "Aid"); }

    /** Permanent Default preset, independent of the editable named preset arrays. */
    UPROPERTY(EditAnywhere, Config, Category="Default Presets")
    FLinearColor DefaultNoteColor = FLinearColor(.07f, .27f, .38f);

    UPROPERTY(EditAnywhere, Config, Category="Default Presets")
    FLinearColor DefaultConnectionColor = FLinearColor(.25f, .8f, .95f);

    UPROPERTY(EditAnywhere, Config, Category="Viewport", meta=(DisplayName="Auto-show Aid documents for the open level"))
    bool bAutoShowAssociatedDocuments = true;

    UPROPERTY(EditAnywhere, Config, Category="Authoring", meta=(DisplayName="Auto-collapse unselected notes"))
    bool bAutoCollapseUnselectedNotes = true;

    UPROPERTY(EditAnywhere, Config, Category="Viewport", meta=(ClampMin="8", ClampMax="96", UIMin="8", UIMax="48"))
    int32 NoteFontSize = 16;

    UPROPERTY(EditAnywhere, Config, Category="Viewport", meta=(ClampMin="1.0", ClampMax="12.0"))
    float NoteBorderThickness = 1.5f;

    UPROPERTY(EditAnywhere, Config, Category="Presets", meta=(TitleProperty="Name"))
    TArray<FAidCategoryPreset> Categories;

    /** Folder-like organization for documents; None keeps documents at the root. */
    UPROPERTY(EditAnywhere, Config, Category="Presets")
    TArray<FName> DocumentCategories;

    UPROPERTY(EditAnywhere, Config, Category="Presets", meta=(TitleProperty="Name"))
    TArray<FAidRelationshipPreset> Relationships;

    const FAidCategoryPreset* FindCategory(FName Name) const;
    const FAidRelationshipPreset* FindRelationship(FName Name) const;
    FLinearColor ResolveNoteColor(const FAidNode& Node) const;
    FLinearColor ResolveConnectionColor(const FAidConnection& Connection) const;
    FString GetNotePresetLabel(const FAidNode& Node) const;
    FString GetConnectionPresetLabel(const FAidConnection& Connection) const;
    FString GetNoteFooterLabel(const FAidNode& Node) const;
    FString GetConnectionFooterLabel(const FAidConnection& Connection) const;
};
