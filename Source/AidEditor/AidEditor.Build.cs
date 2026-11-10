using UnrealBuildTool;

public class AidEditor : ModuleRules
{
    public AidEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "AppFramework", "Aid", "UnrealEd", "EditorFramework", "LevelEditor",
            "Slate", "SlateCore", "PropertyEditor", "AssetRegistry", "AssetDefinition", "EditorSubsystem", "ToolMenus", "Projects"
        });
    }
}
