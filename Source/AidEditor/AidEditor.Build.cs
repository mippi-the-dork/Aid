using UnrealBuildTool;

public class AidEditor : ModuleRules
{
    public AidEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "Aid", "UnrealEd", "LevelEditor",
            "Slate", "SlateCore", "PropertyEditor", "AssetRegistry", "EditorSubsystem", "ToolMenus", "Projects"
        });
    }
}
