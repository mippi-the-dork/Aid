#include "AidProjectSettings.h"

UAidProjectSettings::UAidProjectSettings()
{
    FAidCategoryPreset Design; Design.Name = TEXT("Design"); Categories.Add(Design);
    FAidCategoryPreset Logic; Logic.Name = TEXT("Logic"); Logic.CategoryColor = FLinearColor(0.31f, 0.66f, 0.98f);
    Logic.CardStyle.HeaderBackgroundColor = FLinearColor(0.06f, 0.23f, 0.48f); Categories.Add(Logic);
    FAidCategoryPreset Flow; Flow.Name = TEXT("Flow"); Flow.CategoryColor = FLinearColor(0.38f, 0.91f, 0.59f);
    Flow.CardStyle.HeaderBackgroundColor = FLinearColor(0.06f, 0.32f, 0.19f); Categories.Add(Flow);
    FAidCategoryPreset Narrative; Narrative.Name = TEXT("Narrative"); Narrative.CategoryColor = FLinearColor(0.81f, 0.52f, 0.95f);
    Narrative.CardStyle.HeaderBackgroundColor = FLinearColor(0.31f, 0.14f, 0.43f); Categories.Add(Narrative);

    FAidRelationshipPreset Reference; Relationships.Add(Reference);
    FAidRelationshipPreset Activates; Activates.Name = TEXT("Activates"); Activates.LineColor = FLinearColor(0.45f, 0.9f, 0.63f); Relationships.Add(Activates);
    FAidRelationshipPreset Requires; Requires.Name = TEXT("Requires"); Requires.LineColor = FLinearColor(1.f, 0.7f, 0.35f);
    Requires.LineStyle = EAidLineStyle::Dashed; Relationships.Add(Requires);
    FAidRelationshipPreset PlayerFlow; PlayerFlow.Name = TEXT("Player Flow"); PlayerFlow.LineColor = FLinearColor(0.3f, 0.9f, 1.f);
    PlayerFlow.Thickness = 3.f; Relationships.Add(PlayerFlow);
}

const FAidCategoryPreset* UAidProjectSettings::FindCategory(FName Name) const
{
    return Categories.FindByPredicate([Name](const FAidCategoryPreset& Preset) { return Preset.Name == Name; });
}

const FAidRelationshipPreset* UAidProjectSettings::FindRelationship(FName Name) const
{
    return Relationships.FindByPredicate([Name](const FAidRelationshipPreset& Preset) { return Preset.Name == Name; });
}
