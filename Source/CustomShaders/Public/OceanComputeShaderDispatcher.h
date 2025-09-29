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

	DECLARE_DELEGATE_OneParam(FOnButterflyTextureDrawn, TRefCountPtr<IPooledRenderTarget>);
	void DrawButterfly(int N, FOnButterflyTextureDrawn onComplete);
	
	DECLARE_DELEGATE_TwoParams(FOnInitialSpectraDrawn, TRefCountPtr<IPooledRenderTarget>, TRefCountPtr<IPooledRenderTarget>);
	void DrawIndependentSpectra(int N, FOnInitialSpectraDrawn onComplete);
	
	void DrawHeightField(int N, UTextureRenderTarget2D* target);

private:
	OceanComputeShaderDispatcher() = default;

	static TArray<int> PrecomputeBitReversedIndices(int N);
	static OceanComputeShaderDispatcher* mSingleton;
};