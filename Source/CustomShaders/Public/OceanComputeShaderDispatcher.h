// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <functional>

#include "CoreMinimal.h"


class CUSTOMSHADERS_API OceanComputeShaderDispatcher
{
public:
	static OceanComputeShaderDispatcher* Get()
	{
		if (!mSingleton) mSingleton = new OceanComputeShaderDispatcher;
		return mSingleton;
	}

	DECLARE_DELEGATE_OneParam(FOnButterflyTextureReady, TRefCountPtr<IPooledRenderTarget> butterflyTexture);
	void ComputeButterfly(int N, FOnButterflyTextureReady onComplete);
	
	DECLARE_DELEGATE_TwoParams(FOnInitialSpectraReady, TRefCountPtr<IPooledRenderTarget> positiveSpectrumTexture, TRefCountPtr<IPooledRenderTarget> negativeSpectrumTexture);
	void ComputeInitialSpectra(int N, FOnInitialSpectraReady onComplete);
	
	DECLARE_DELEGATE_OneParam(FOnFourierComponentsReady, TRefCountPtr<IPooledRenderTarget> fourierComponentsTexture);
	void ComputeFourierComponents(int N, FOnFourierComponentsReady onComplete);
	
	DECLARE_DELEGATE_OneParam(FOnDisplacementFieldReady, TRefCountPtr<IPooledRenderTarget> fourierComponentsTexture);
	void ComputeDisplacement(int N, FOnFourierComponentsReady onComplete, UTextureRenderTarget2D* debugOutput);

private:
	OceanComputeShaderDispatcher() = default;

	TMap<int, TRefCountPtr<IPooledRenderTarget>> mButterflyTextureCache;
	TMap<int, std::pair<TRefCountPtr<IPooledRenderTarget>, TRefCountPtr<IPooledRenderTarget>>> mInitialSpectraCache;
	static TArray<int> PrecomputeBitReversedIndices(int N);
	static OceanComputeShaderDispatcher* mSingleton;
};