#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"

#define NUM_THREADS_PER_GROUP_DIMENSION 32


struct FNormalsComputeShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FNormalsComputeShader);

	SHADER_USE_PARAMETER_STRUCT(FNormalsComputeShader, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	// check that x and y are choppiness axis and not height displacement
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, displacementX)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, displacementY)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, normals)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};