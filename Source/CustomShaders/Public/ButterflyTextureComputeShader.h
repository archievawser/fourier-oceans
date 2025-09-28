#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"


#define NUM_THREADS_PER_GROUP_DIMENSION 32


struct FButterflyTextureComputeShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FButterflyTextureComputeShader);

	SHADER_USE_PARAMETER_STRUCT(FButterflyTextureComputeShader, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4>, ButterflyTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, BitReversedIndices)
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


IMPLEMENT_GLOBAL_SHADER(FButterflyTextureComputeShader, "/CustomShaders/ButterflyTextureComputeShader.usf", "MainComputeShader", SF_Compute);


class CUSTOMSHADERS_API ButterflyTextureComputeShaderManager
{
	struct FParams
 	{
 		UTextureRenderTarget2D* RenderTarget;
 	};
	
public:
	static ButterflyTextureComputeShaderManager* Get()
	{
		if (!mSingleton) mSingleton = new ButterflyTextureComputeShaderManager;
		return mSingleton;
	}
	
	void Dispatch(UTextureRenderTarget2D* target);
	void UpdateParameters(FParams& drawParameters);

private:
	ButterflyTextureComputeShaderManager() = default;
	
	static ButterflyTextureComputeShaderManager* mSingleton;
	TArray<int> BitReversedIndices;
};
