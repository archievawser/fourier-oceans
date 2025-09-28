using UnrealBuildTool;

public class CustomShaders : ModuleRules
{
    public CustomShaders(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Renderer",
            "RenderCore",
            "RHI",
            "Projects"
        });
    }
}