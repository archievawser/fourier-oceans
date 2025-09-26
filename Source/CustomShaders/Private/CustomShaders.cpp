#include "CustomShaders.h"

#define LOCTEXT_NAMESPACE "FCustomShadersModule"

void FCustomShadersModule::StartupModule()
{
    FString shaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders/"));
    AddShaderSourceDirectoryMapping("/CustomShaders", shaderDirectory);
}

void FCustomShadersModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FCustomShadersModule, CustomShaders)