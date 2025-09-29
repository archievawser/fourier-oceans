#include "OceanComputeShaderDispatcher.h"

#include "ButterflyTextureComputeShader.h"
#include "FourierComponentsComputeShader.h"
#include "NoiseComputeShader.h"
#include "RenderGraphUtils.h"
#include "SpectrumComputeShader.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"


template <typename T>
T reverse(T n, size_t b = sizeof(T) * CHAR_BIT)
{
	T rv = 0;

	for (size_t i = 0; i < b; ++i, n >>= 1) {
		rv = (rv << 1) | (n & 0x01);
	}

	return rv;
}


TArray<int> OceanComputeShaderDispatcher::PrecomputeBitReversedIndices(int N)
{
	TArray<int> reversedIndices;
	reversedIndices.SetNumUninitialized(N);
	
	const unsigned int power = log2(N);
	
	for (unsigned int i = 0; i < (unsigned int)N; i++)
	{
		reversedIndices[i] = reverse(i, power);
	}

	return reversedIndices;
}


void OceanComputeShaderDispatcher::DrawButterfly(int N, FOnButterflyTextureDrawn onComplete)
{
	ENQUEUE_RENDER_COMMAND(WaveComputeCmd)([N, onComplete](FRHICommandListImmediate& rhiCmdList) mutable
	{
		FRDGBuilder rdgBuilder(rhiCmdList);
		
		FButterflyTextureComputeShader::FParameters params;
		params.N = N;

		// Create butterfly texture on GPU
		FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
			FIntPoint(log2(N), N),
			PF_A32B32G32R32F,
			FClearValueBinding(),
			TexCreate_UAV
		);
		FRDGTextureRef outTextureRef = rdgBuilder.CreateTexture(textureDesc, TEXT("Butterfly_Compute_Out"));
        FRDGTextureUAVRef outTextureUAV = rdgBuilder.CreateUAV({ outTextureRef });
		params.ButterflyTexture = outTextureUAV;

		// Upload bit-reversed indices to GPU
		FRDGBufferDesc bitReversedIndicesBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(int), N);
		FRDGBufferRef bitReversedIndicesBufferRef = rdgBuilder.CreateBuffer(bitReversedIndicesBufferDesc, TEXT("Butterfly_Compute_BRI_Buffer"));
		params.BitReversedIndices = rdgBuilder.CreateUAV({ bitReversedIndicesBufferRef, PF_R32_SINT });

		TArray<int> bri = PrecomputeBitReversedIndices(N);
		rdgBuilder.QueueBufferUpload(bitReversedIndicesBufferRef, bri.GetData(), bri.Num() * sizeof(int));

		// Add compute execution step
		TShaderMapRef<FButterflyTextureComputeShader> butterflyCompute(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		
		rdgBuilder.AddPass(
			RDG_EVENT_NAME("ButterflyComputePass"),
			&params,
			ERDGPassFlags::Compute,
			[&](FRHICommandListImmediate& passRhiCmdList)
		{	
			FComputeShaderUtils::Dispatch(passRhiCmdList, butterflyCompute, params,
			FIntVector(
				FMath::DivideAndRoundUp((int)(log2(N)), NUM_THREADS_PER_GROUP_DIMENSION),
				FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
				1)
			);
		});

		TRefCountPtr<IPooledRenderTarget> output;
		rdgBuilder.QueueTextureExtraction(outTextureRef, &output);
		rdgBuilder.Execute();

		onComplete.ExecuteIfBound(output);
	});
}


void OceanComputeShaderDispatcher::DrawIndependentSpectra(int N, FOnInitialSpectraDrawn onComplete)
{
	ENQUEUE_RENDER_COMMAND(SpectraComputeCmd)([N, onComplete](FRHICommandListImmediate& rhiCmdList) mutable 
	{
		TShaderMapRef<FNoiseComputeShader> noiseComputeShader (GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FRDGBuilder rdgBuilder(rhiCmdList);

		// Compute noise textures (R,G,B, and A are each individual random 0-1 floats)
		FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
			FIntPoint(N, N),
			PF_A32B32G32R32F,
			FClearValueBinding(),
			TexCreate_UAV
		);
		
		FRDGTextureRef outNoiseRef = rdgBuilder.CreateTexture(textureDesc, TEXT("Noise_Compute_Out"));
        FRDGTextureUAVRef outNoiseUAV = rdgBuilder.CreateUAV({ outNoiseRef });
		FNoiseComputeShader::FParameters noiseComputeParams;
		noiseComputeParams.Output = outNoiseUAV;

		rdgBuilder.AddPass(
            RDG_EVENT_NAME("NoiseComputePass"),
            &noiseComputeParams,
            ERDGPassFlags::Compute,
            [&](FRHICommandListImmediate& passRhiCmdList)
        {	
            FComputeShaderUtils::Dispatch(passRhiCmdList, noiseComputeShader, noiseComputeParams,
            FIntVector(
            	FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
            	FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
            	1)
            );
        });
		
		// Compute initial spectra
		TShaderMapRef<FSpectrumComputeShader> spectraComputeShader (GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSpectrumComputeShader::FParameters spectraComputeParams;
		spectraComputeParams.N = N;
		spectraComputeParams.L = 1000;
		spectraComputeParams.A = 4;
		spectraComputeParams.WindDirection = FVector2f(1.0f, 1.0f);
		spectraComputeParams.WindSpeed = 12.0;
		spectraComputeParams.Noise = outNoiseUAV;

		FRDGTextureRef outNegativeSpectrum = rdgBuilder.CreateTexture(textureDesc, TEXT("NegativeSpectrum_Compute_Out"));
		FRDGTextureUAVRef outNegativeSpectrumUAV = rdgBuilder.CreateUAV({ outNegativeSpectrum });
		spectraComputeParams.NegativeSpectrum = outNegativeSpectrumUAV;

		FRDGTextureRef outPositiveSpectrum = rdgBuilder.CreateTexture(textureDesc, TEXT("PositiveSpectrum_Compute_Out"));
		FRDGTextureUAVRef outPositiveSpectrumUAV = rdgBuilder.CreateUAV({ outPositiveSpectrum });
		spectraComputeParams.PositiveSpectrum = outPositiveSpectrumUAV;

		rdgBuilder.AddPass(
			RDG_EVENT_NAME("SpectraComputePass"),
			&spectraComputeParams,
			ERDGPassFlags::Compute,
			[&](FRHICommandListImmediate& passRhiCmdList)
		{	
			FComputeShaderUtils::Dispatch(passRhiCmdList, spectraComputeShader, spectraComputeParams,
			FIntVector(
				FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
				FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
				1)
			);
		});
		
		TRefCountPtr<IPooledRenderTarget> negativeSpectrumOut;
		TRefCountPtr<IPooledRenderTarget> positiveSpectrumOut;
		rdgBuilder.QueueTextureExtraction(outNegativeSpectrum, &negativeSpectrumOut);
		rdgBuilder.QueueTextureExtraction(outPositiveSpectrum, &positiveSpectrumOut);
		rdgBuilder.Execute();

		onComplete.ExecuteIfBound(positiveSpectrumOut, negativeSpectrumOut);
	});
}


void OceanComputeShaderDispatcher::DrawHeightField(int N, UTextureRenderTarget2D* target)
{
	FOnInitialSpectraDrawn onInitialSpectraDrawn;

	onInitialSpectraDrawn.BindLambda([this, N, target](TRefCountPtr<IPooledRenderTarget> positiveSpectrum, TRefCountPtr<IPooledRenderTarget> negativeSpectrum) 
	{
		FOnButterflyTextureDrawn onButterflyDrawn;

		onButterflyDrawn.BindLambda([N, target, positiveSpectrum, negativeSpectrum](TRefCountPtr<IPooledRenderTarget> butterfly)
		{
			ENQUEUE_RENDER_COMMAND(HeightComputeCmd)([N, target, positiveSpectrum, negativeSpectrum, butterfly](FRHICommandListImmediate& rhiCmdList) mutable
            {
            	FRDGBuilder rdgBuilder(rhiCmdList);
            		
            	FFourierComponentsComputeShader::FParameters params;
            	params.N = N;
            	params.L = 1000;
            	params.t = 0.1;
        
            	// Create height texture on GPU
            	FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
            		FIntPoint(N, N),
            		PF_A32B32G32R32F,
            		FClearValueBinding(),
            		TexCreate_UAV
            	);
            	
            	FRDGTextureRef fourierComponentsOut_Y = rdgBuilder.CreateTexture(textureDesc, TEXT("FourierComponents_Y_Out"));
            	FRDGTextureUAVRef fourierComponentsOut_Y_UAV = rdgBuilder.CreateUAV({ fourierComponentsOut_Y });
            	params.FourierComponentsY = fourierComponentsOut_Y_UAV;
            	
            	FRDGTextureRef fourierComponentsOut_X = rdgBuilder.CreateTexture(textureDesc, TEXT("FourierComponents_X_Out"));
            	FRDGTextureUAVRef fourierComponentsOut_X_UAV = rdgBuilder.CreateUAV({ fourierComponentsOut_X });
            	params.FourierComponentsX = fourierComponentsOut_X_UAV;
            	
            	FRDGTextureRef fourierComponentsOut_Z = rdgBuilder.CreateTexture(textureDesc, TEXT("FourierComponents_Z_Out"));
            	FRDGTextureUAVRef fourierComponentsOut_Z_UAV = rdgBuilder.CreateUAV({ fourierComponentsOut_Z });
            	params.FourierComponentsZ = fourierComponentsOut_Z_UAV;
        
            	FRDGTextureRef positiveSpectrumRef = rdgBuilder.RegisterExternalTexture(positiveSpectrum);
            	params.PositiveInitialSpectrum = rdgBuilder.CreateUAV({ positiveSpectrumRef });
            	
            	FRDGTextureRef negativeSpectrumRef = rdgBuilder.RegisterExternalTexture(negativeSpectrum);
            	params.NegativeInitialSpectrum = rdgBuilder.CreateUAV({ negativeSpectrumRef });
        
            	// Add compute execution step
            	TShaderMapRef<FFourierComponentsComputeShader> fourierComponentsCompute(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            		
            	rdgBuilder.AddPass(
            		RDG_EVENT_NAME("FourierComponentsComputePass"),
            		&params,
            		ERDGPassFlags::Compute,
            		[&](FRHICommandListImmediate& passRhiCmdList)
            	{	
            		FComputeShaderUtils::Dispatch(passRhiCmdList, fourierComponentsCompute, params,
            		FIntVector(
            			FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
            			FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
            			1)
            		);
            	});
            	
            	TRefCountPtr<IPooledRenderTarget> output;
            	rdgBuilder.QueueTextureExtraction(fourierComponentsOut_Y, &output);
            	rdgBuilder.Execute();
        
            	rhiCmdList.CopyTexture(output->GetRHI(), target->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
            });
		});
		
		DrawButterfly(N, onButterflyDrawn);
	});

	DrawIndependentSpectra(N, onInitialSpectraDrawn);
}


OceanComputeShaderDispatcher* OceanComputeShaderDispatcher::mSingleton;
