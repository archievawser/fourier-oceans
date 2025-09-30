#include "OceanComputeShaderDispatcher.h"

#include "ButterflyTextureComputeShader.h"
#include "FFTComputeShader.h"
#include "FourierComponentsComputeShader.h"
#include "NoiseComputeShader.h"
#include "RenderGraphUtils.h"
#include "InitialSpectraComputeShader.h"
#include "InversionComputeShader.h"
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


void OceanComputeShaderDispatcher::ComputeButterfly(int N, FOnButterflyTextureReady onComplete)
{
	if (mButterflyTextureCache.Contains(N))
		return (void) onComplete.ExecuteIfBound(mButterflyTextureCache[N]);
	
	ENQUEUE_RENDER_COMMAND(WaveComputeCmd)([this, N, onComplete](FRHICommandListImmediate& rhiCmdList) mutable
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
		
		mButterflyTextureCache.Add(N, output);
		onComplete.ExecuteIfBound(output);
	});
}


void OceanComputeShaderDispatcher::ComputeInitialSpectra(int N, FOnInitialSpectraReady onComplete)
{
	if (mButterflyTextureCache.Contains(N))
		return (void) onComplete.ExecuteIfBound(mInitialSpectraCache[N].first, mInitialSpectraCache[N].second);
	
	ENQUEUE_RENDER_COMMAND(SpectraComputeCmd)([this, N, onComplete](FRHICommandListImmediate& rhiCmdList) mutable 
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
		TShaderMapRef<FInitialSpectraComputeShader> spectraComputeShader (GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FInitialSpectraComputeShader::FParameters spectraComputeParams;
		spectraComputeParams.N = N;
		spectraComputeParams.L = 1000;
		spectraComputeParams.A = 4;
		spectraComputeParams.WindDirection = FVector2f(1.0f, 1.0f);
		spectraComputeParams.WindSpeed = 40.0;
		spectraComputeParams.Noise = outNoiseUAV;

		FRDGTextureRef outNegativeSpectrum = rdgBuilder.CreateTexture(textureDesc, TEXT("NegativeSpectrum_Compute_Out"));
		FRDGTextureUAVRef outNegativeSpectrumUAV = rdgBuilder.CreateUAV({ outNegativeSpectrum });
		spectraComputeParams.NegativeSpectrum = outNegativeSpectrumUAV;

		FRDGTextureRef outPositiveSpectrum = rdgBuilder.CreateTexture(textureDesc, TEXT("PositiveSpectrum_Compute_Out"));
		FRDGTextureUAVRef outPositiveSpectrumUAV = rdgBuilder.CreateUAV({ outPositiveSpectrum });
		spectraComputeParams.PositiveSpectrum = outPositiveSpectrumUAV;

		rdgBuilder.AddPass(
			RDG_EVENT_NAME("InitialSpectraComputePass"),
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

		mInitialSpectraCache.Add(N, { positiveSpectrumOut, negativeSpectrumOut });
		onComplete.ExecuteIfBound(positiveSpectrumOut, negativeSpectrumOut);
	});
}


void OceanComputeShaderDispatcher::ComputeFourierComponents(int N, FOnFourierComponentsReady onComplete)
{
	FOnInitialSpectraReady onInitialSpectraDrawn;

	onInitialSpectraDrawn.BindLambda([this, N, onComplete](TRefCountPtr<IPooledRenderTarget> positiveSpectrum, TRefCountPtr<IPooledRenderTarget> negativeSpectrum) 
	{
		ENQUEUE_RENDER_COMMAND(HeightComputeCmd)([N, positiveSpectrum, negativeSpectrum, onComplete](FRHICommandListImmediate& rhiCmdList) mutable
        {
            FRDGBuilder rdgBuilder(rhiCmdList);
            	
            FFourierComponentsComputeShader::FParameters params;
            params.N = N;
            params.L = 100;

			static float t = 0.0f;
			t += 0.003f;
            params.t = t;
    
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

			FFourierComponents output;
            rdgBuilder.QueueTextureExtraction(fourierComponentsOut_X, &output.Components[0]);
            rdgBuilder.QueueTextureExtraction(fourierComponentsOut_Y, &output.Components[1]);
            rdgBuilder.QueueTextureExtraction(fourierComponentsOut_Z, &output.Components[2]);
			
            rdgBuilder.Execute();
			onComplete.ExecuteIfBound(output);
        });
	});

	ComputeInitialSpectra(N, onInitialSpectraDrawn);
}


void OceanComputeShaderDispatcher::ComputeDisplacement(int N, FOnDisplacementFieldReady onComplete, UTextureRenderTarget2D* displacementOutXTarget, UTextureRenderTarget2D* displacementOutYTarget, UTextureRenderTarget2D* displacementOutZTarget)
{
	FOnFourierComponentsReady onFourierComponentsReady;

	onFourierComponentsReady.BindLambda([this, N, onComplete, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget](FFourierComponents fourierComponents)
	{
		FOnButterflyTextureReady onButterflyTextureReady;

		onButterflyTextureReady.BindLambda([N, fourierComponents, onComplete, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget](TRefCountPtr<IPooledRenderTarget> butterflyTexture)
		{
			ENQUEUE_RENDER_COMMAND(HeightComputeCmd)([N, fourierComponents, onComplete, butterflyTexture, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget](FRHICommandListImmediate& rhiCmdList) mutable
            {
                FRDGBuilder rdgBuilder(rhiCmdList);
				TShaderMapRef<FFFTComputeShader> fftCompute(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        
                // Create height texture on GPU
                FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
                    FIntPoint(N, N),
                    PF_A32B32G32R32F,
                    FClearValueBinding(),
                    TexCreate_UAV
                );
        		
                FRDGTextureRef butterflyRef = rdgBuilder.RegisterExternalTexture(butterflyTexture);
                FRDGTextureUAVRef butterflyTextureUAV = rdgBuilder.CreateUAV({butterflyRef});

				TRefCountPtr<IPooledRenderTarget> displacementTexturesInternal[] { nullptr, nullptr, nullptr };
				UTextureRenderTarget2D* displacementTargets[] { displacementOutXTarget, displacementOutYTarget, displacementOutZTarget };
				FRDGTextureRef displacementTextures[] { 
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutX")),
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutY")),
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutZ"))
				};

				for (int axis = 0; axis < 3; axis++)
				{
	                FRDGTextureRef fourierComponentsRef = rdgBuilder.RegisterExternalTexture(fourierComponents.Components[axis]);
                    FRDGTextureUAVRef pingpong0UAV = rdgBuilder.CreateUAV({fourierComponentsRef});
                    
                    FRDGTextureRef pingPong1Texture = rdgBuilder.CreateTexture(textureDesc, TEXT("FFT_PingPong1_Out"));
                    FRDGTextureUAVRef pingPong1Texture_UAV = rdgBuilder.CreateUAV({ pingPong1Texture });
                    FRDGTextureUAVRef pingpong1UAV = pingPong1Texture_UAV;
    
                    enum class FFTDirection { Horizontal, Vertical };
                    const int numStages = log2(N);
                    int pingpong = 0;
    
                    for (auto direction: { FFTDirection::Horizontal, FFTDirection::Vertical })
                    {
                        for (int i = 0; i < numStages; i++)
                        {
                            FFFTComputeShader::FParameters* params = rdgBuilder.AllocParameters<FFFTComputeShader::FParameters>();
                            params->direction = (int)direction;
                            params->pingpong0 = pingpong0UAV;
                            params->pingpong1 = pingpong1UAV;
                            params->stage = i;
                            params->pingpong = pingpong % 2;
                            params->butterflyTexture = butterflyTextureUAV;
                            pingpong++;
                            
                            // Add compute execution step
                            rdgBuilder.AddPass(
                                RDG_EVENT_NAME("FFTComputePass"),
                                params,
                                ERDGPassFlags::Compute,
                                [fftCompute, params, N](FRHICommandListImmediate& passRhiCmdList)
                            {	
                                FComputeShaderUtils::Dispatch(passRhiCmdList, fftCompute, *params,
                                FIntVector(
                                    FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
                                    FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
                                    1)
                                );
                            });
                        }
                    }
					
                    FInversionComputeShader::FParameters* inversionParams = rdgBuilder.AllocParameters<FInversionComputeShader::FParameters>();
                    inversionParams->pingpong0 = pingpong0UAV;
                    inversionParams->pingpong1 = pingpong1UAV;
                    inversionParams->N = N;
                    inversionParams->pingpong = pingpong % 2;
                    inversionParams->displacement = rdgBuilder.CreateUAV({ displacementTextures[axis] });
                    
                    TShaderMapRef<FInversionComputeShader> inversionCompute (GetGlobalShaderMap(GMaxRHIFeatureLevel));
                    rdgBuilder.AddPass(
                    	RDG_EVENT_NAME("InversionComputePass"),
                    	inversionParams,
                    	ERDGPassFlags::Compute,
                    	[inversionParams, inversionCompute, N](FRHICommandListImmediate& passRhiCmdList)
                    {	
                    	FComputeShaderUtils::Dispatch(passRhiCmdList, inversionCompute, *inversionParams,
                    	FIntVector(
                    		FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
                    		FMath::DivideAndRoundUp(N, NUM_THREADS_PER_GROUP_DIMENSION),
                    		1)
                    	);
                    });
				}
				
                rdgBuilder.QueueTextureExtraction(displacementTextures[0], &displacementTexturesInternal[0]);
                rdgBuilder.QueueTextureExtraction(displacementTextures[1], &displacementTexturesInternal[1]);
				rdgBuilder.QueueTextureExtraction(displacementTextures[2], &displacementTexturesInternal[2]);
                rdgBuilder.Execute();
				
				rhiCmdList.CopyTexture(displacementTexturesInternal[0]->GetRHI(), displacementTargets[0]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
				rhiCmdList.CopyTexture(displacementTexturesInternal[1]->GetRHI(), displacementTargets[1]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
				rhiCmdList.CopyTexture(displacementTexturesInternal[2]->GetRHI(), displacementTargets[2]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
            });	
		});
		
		ComputeButterfly(N, onButterflyTextureReady);
	});

	ComputeFourierComponents(N, onFourierComponentsReady);
}


OceanComputeShaderDispatcher* OceanComputeShaderDispatcher::mSingleton;
