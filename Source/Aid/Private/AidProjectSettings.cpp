#include "AidProjectSettings.h"

UAidProjectSettings::UAidProjectSettings()
{
    DocumentCategories = { TEXT("Design"), TEXT("Narrative"), TEXT("QA") };
    FAidCategoryPreset Design; Design.Name = TEXT("Design"); Categories.Add(Design);
    FAidCategoryPreset Logic; Logic.Name = TEXT("Logic"); Logic.BaseColor = FLinearColor(.06f,.23f,.48f); Categories.Add(Logic);
    FAidCategoryPreset Flow; Flow.Name = TEXT("Flow"); Flow.BaseColor = FLinearColor(.06f,.32f,.19f); Categories.Add(Flow);
    FAidCategoryPreset Narrative; Narrative.Name = TEXT("Narrative"); Narrative.BaseColor = FLinearColor(.31f,.14f,.43f); Categories.Add(Narrative);

    FAidRelationshipPreset Reference; Relationships.Add(Reference);
    FAidRelationshipPreset Activates; Activates.Name = TEXT("Activates"); Activates.BaseColor = FLinearColor(.45f,.9f,.63f); Relationships.Add(Activates);
    FAidRelationshipPreset Requires; Requires.Name = TEXT("Requires"); Requires.BaseColor = FLinearColor(1.f,.7f,.35f);
    Requires.LineStyle = EAidLineStyle::Dashed; Relationships.Add(Requires);
    FAidRelationshipPreset PlayerFlow; PlayerFlow.Name = TEXT("Player Flow"); PlayerFlow.BaseColor = FLinearColor(.3f,.9f,1.f);
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

FLinearColor UAidProjectSettings::ResolveNoteColor(const FAidNode& Node) const
{
    if (Node.bCustomAppearance) return Node.Style.HeaderBackgroundColor;
    if (const FAidCategoryPreset* Preset = FindCategory(Node.Category)) return Preset->BaseColor;
    return DefaultNoteColor;
}

FLinearColor UAidProjectSettings::ResolveConnectionColor(const FAidConnection& Edge) const
{
    if (Edge.bCustomAppearance) return Edge.Color;
    if (const FAidRelationshipPreset* Preset = FindRelationship(Edge.RelationshipType)) return Preset->BaseColor;
    return DefaultConnectionColor;
}

FString UAidProjectSettings::GetNotePresetLabel(const FAidNode& Node) const
{
    if (Node.bCustomAppearance) return TEXT("Custom");
    if (FindCategory(Node.Category)) return Node.Category.ToString();
    return TEXT("Default");
}

FString UAidProjectSettings::GetConnectionPresetLabel(const FAidConnection& Edge) const
{
    if (Edge.bCustomAppearance) return TEXT("Custom");
    if (FindRelationship(Edge.RelationshipType)) return Edge.RelationshipType.ToString();
    return TEXT("Default");
}

FString UAidProjectSettings::GetNoteFooterLabel(const FAidNode& Node) const
{
    if (Node.bCustomAppearance && !Node.CustomFooterText.IsEmpty()) return Node.CustomFooterText.ToString();
    return GetNotePresetLabel(Node);
}

FString UAidProjectSettings::GetConnectionFooterLabel(const FAidConnection& Edge) const
{
    if (Edge.bCustomAppearance && !Edge.CustomFooterText.IsEmpty()) return Edge.CustomFooterText.ToString();
    return GetConnectionPresetLabel(Edge);
}
