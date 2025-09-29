#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"


#define NUM_THREADS_PER_GROUP_DIMENSION 32


struct FSpectrumComputeShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpectrumComputeShader);

	SHADER_USE_PARAMETER_STRUCT(FSpectrumComputeShader, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, PositiveSpectrum)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, NegativeSpectrum)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, Noise)
		SHADER_PARAMETER(float, N)
		SHADER_PARAMETER(float, L)
		SHADER_PARAMETER(float, A)
		SHADER_PARAMETER(FVector2f, WindDirection)
		SHADER_PARAMETER(float, WindSpeed)
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