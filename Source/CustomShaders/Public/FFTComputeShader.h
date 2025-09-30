#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"

#define NUM_THREADS_PER_GROUP_DIMENSION 32


struct FFFTComputeShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFFTComputeShader);

	SHADER_USE_PARAMETER_STRUCT(FFFTComputeShader, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, butterflyTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, pingpong0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, pingpong1)
		SHADER_PARAMETER(int, stage)
		SHADER_PARAMETER(int, pingpong)
		SHADER_PARAMETER(int, direction)
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