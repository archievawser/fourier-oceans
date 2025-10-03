// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <functional>

#include "CoreMinimal.h"


class CUSTOMSHADERS_API OceanTextureManager
{
public:
	struct FFourierComponents
	{
		TRefCountPtr<IPooledRenderTarget> Components[3];
	};
	
	struct FSpectrumParameters
	{
		int N = 512;
		float L = 1000;
		float A = 4;
		FVector2f WindDirection = FVector2f(1.0f, 1.0f);
		float WindSpeed = 20;
	};
	
	static OceanTextureManager* Get()
	{
		if (!mSingleton) mSingleton = new OceanTextureManager;
		return mSingleton;
	}

	void SetSpectrumParameters(const FSpectrumParameters& spectrumParameters);

	DECLARE_DELEGATE_OneParam(FOnButterflyTextureReady, TRefCountPtr<IPooledRenderTarget> butterflyTexture);
	void ComputeButterfly(FOnButterflyTextureReady onComplete);
	
	DECLARE_DELEGATE_TwoParams(FOnInitialSpectraTexturesReady, TRefCountPtr<IPooledRenderTarget> positiveSpectrumTexture, TRefCountPtr<IPooledRenderTarget> negativeSpectrumTexture);
	void ComputeInitialSpectra(FOnInitialSpectraTexturesReady onComplete, bool useCache = true);
	
	DECLARE_DELEGATE_OneParam(FOnFourierComponentsReady, FFourierComponents fourierComponentsTexture);
	void ComputeFourierComponents(float time, FOnFourierComponentsReady onComplete);
	
	DECLARE_DELEGATE_OneParam(FOnDisplacementFieldReady, TRefCountPtr<IPooledRenderTarget> fourierComponentsTexture);
	void ComputeDisplacement(float time, FOnDisplacementFieldReady onComplete, UTextureRenderTarget2D* displacementOutX, UTextureRenderTarget2D* displacementOutY, UTextureRenderTarget2D* displacementOutZ);

private:
	OceanTextureManager() = default;
	
	FSpectrumParameters mSpectrumParameters;
	
	TMap<int, TRefCountPtr<IPooledRenderTarget>> mButterflyTextureCache;
	
	TMap<int, std::pair<TRefCountPtr<IPooledRenderTarget>, TRefCountPtr<IPooledRenderTarget>>> mInitialSpectraCache;
	
	static TArray<int> PrecomputeBitReversedIndices(int N);
	
	static OceanTextureManager* mSingleton;
};