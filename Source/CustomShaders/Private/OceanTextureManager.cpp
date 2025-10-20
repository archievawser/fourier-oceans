#include "OceanTextureManager.h"

#include "ButterflyTextureComputeShader.h"
#include "FFTComputeShader.h"
#include "FoamComputeShader.h"
#include "FourierComponentsComputeShader.h"
#include "NoiseComputeShader.h"
#include "RenderGraphUtils.h"
#include "InitialSpectraComputeShader.h"
#include "InversionComputeShader.h"
#include "NormalsComputeShader.h"
#include "DSP/AudioFFT.h"
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


TArray<int> OceanTextureManager::PrecomputeBitReversedIndices(int N)
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


void OceanTextureManager::SetSpectrumParameters(const FSpectrumParameters& spectrumParameters)
{
	mSpectrumParameters = spectrumParameters;

	ComputeInitialSpectra(FOnInitialSpectraTexturesReady(), false);
}


void OceanTextureManager::ComputeButterfly(FOnButterflyTextureReady onComplete)
{
	if (mButterflyTextureCache.Contains(mSpectrumParameters.N))
		return (void) onComplete.ExecuteIfBound(mButterflyTextureCache[mSpectrumParameters.N]);
	
	ENQUEUE_RENDER_COMMAND(WaveComputeCmd)([this, onComplete](FRHICommandListImmediate& rhiCmdList) mutable
	{
		FRDGBuilder rdgBuilder(rhiCmdList);
		
		FButterflyTextureComputeShader::FParameters params;
		params.N = mSpectrumParameters.N;

		// Create butterfly texture on GPU
		FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
			FIntPoint(log2(mSpectrumParameters.N), mSpectrumParameters.N),
			PF_A32B32G32R32F,
			FClearValueBinding(),
			TexCreate_UAV
		);
		FRDGTextureRef outTextureRef = rdgBuilder.CreateTexture(textureDesc, TEXT("Butterfly_Compute_Out"));
        FRDGTextureUAVRef outTextureUAV = rdgBuilder.CreateUAV({ outTextureRef });
		params.ButterflyTexture = outTextureUAV;

		// Upload bit-reversed indices to GPU
		FRDGBufferDesc bitReversedIndicesBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(int), mSpectrumParameters.N);
		FRDGBufferRef bitReversedIndicesBufferRef = rdgBuilder.CreateBuffer(bitReversedIndicesBufferDesc, TEXT("Butterfly_Compute_BRI_Buffer"));
		params.BitReversedIndices = rdgBuilder.CreateUAV({ bitReversedIndicesBufferRef, PF_R32_SINT });

		TArray<int> bri = PrecomputeBitReversedIndices(mSpectrumParameters.N);
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
				FMath::DivideAndRoundUp((int)(log2(mSpectrumParameters.N)), NUM_THREADS_PER_GROUP_DIMENSION),
				FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
				1)
			);
		});

		TRefCountPtr<IPooledRenderTarget> output;
		rdgBuilder.QueueTextureExtraction(outTextureRef, &output);
		rdgBuilder.Execute();
		
		mButterflyTextureCache.Add(mSpectrumParameters.N, output);
		onComplete.ExecuteIfBound(output);
	});
}


void OceanTextureManager::ComputeInitialSpectra(FOnInitialSpectraTexturesReady onComplete, bool useCache)
{
	if (useCache && mInitialSpectraCache.Contains(mSpectrumParameters.N))
		return (void) onComplete.ExecuteIfBound(mInitialSpectraCache[mSpectrumParameters.N].first, mInitialSpectraCache[mSpectrumParameters.N].second);
	
	ENQUEUE_RENDER_COMMAND(SpectraComputeCmd)([this, onComplete](FRHICommandListImmediate& rhiCmdList) mutable 
	{
		TShaderMapRef<FNoiseComputeShader> noiseComputeShader (GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FRDGBuilder rdgBuilder(rhiCmdList);

		// Compute noise textures (R,G,B, and A are each individual random 0-1 floats)
		FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
			FIntPoint(mSpectrumParameters.N, mSpectrumParameters.N),
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
            	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
            	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
            	1)
            );
        });
		
		// Compute initial spectra
		TShaderMapRef<FInitialSpectraComputeShader> spectraComputeShader (GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FInitialSpectraComputeShader::FParameters spectraComputeParams;
		spectraComputeParams.N = mSpectrumParameters.N;
		spectraComputeParams.L = mSpectrumParameters.L;
		spectraComputeParams.A = mSpectrumParameters.A;
		spectraComputeParams.WindDirection = mSpectrumParameters.WindDirection;
		spectraComputeParams.WindSpeed = mSpectrumParameters.WindSpeed;
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
				FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
				FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
				1)
			);
		});
		
		TRefCountPtr<IPooledRenderTarget> negativeSpectrumOut;
		TRefCountPtr<IPooledRenderTarget> positiveSpectrumOut;
		rdgBuilder.QueueTextureExtraction(outNegativeSpectrum, &negativeSpectrumOut);
		rdgBuilder.QueueTextureExtraction(outPositiveSpectrum, &positiveSpectrumOut);
		rdgBuilder.Execute();

		mInitialSpectraCache.Add(mSpectrumParameters.N, { positiveSpectrumOut, negativeSpectrumOut });
		onComplete.ExecuteIfBound(positiveSpectrumOut, negativeSpectrumOut);
	});
}


void OceanTextureManager::ComputeFourierComponents(float time, FOnFourierComponentsReady onComplete)
{
	FOnInitialSpectraTexturesReady onInitialSpectraDrawn;

	onInitialSpectraDrawn.BindLambda([this, onComplete, time](TRefCountPtr<IPooledRenderTarget> positiveSpectrum, TRefCountPtr<IPooledRenderTarget> negativeSpectrum) 
	{
		ENQUEUE_RENDER_COMMAND(HeightComputeCmd)([this, positiveSpectrum, negativeSpectrum, onComplete, time](FRHICommandListImmediate& rhiCmdList) mutable
        {
            FRDGBuilder rdgBuilder(rhiCmdList);
            	
            FFourierComponentsComputeShader::FParameters params;
            params.N = mSpectrumParameters.N;
            params.L = mSpectrumParameters.L;

			static float t = 0.03f;
			t += 0.008f;
            params.t = time;
    
            // Create height texture on GPU
            FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
            	FIntPoint(mSpectrumParameters.N, mSpectrumParameters.N),
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
            		FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
            		FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
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

	ComputeInitialSpectra(onInitialSpectraDrawn);
}


void OceanTextureManager::ComputeDisplacement(float time, FOnDisplacementFieldReady onComplete, UTextureRenderTarget2D* displacementOutXTarget, UTextureRenderTarget2D* displacementOutYTarget, UTextureRenderTarget2D* displacementOutZTarget, UTextureRenderTarget2D* foamOutTarget)
{
	FOnFourierComponentsReady onFourierComponentsReady;

	onFourierComponentsReady.BindLambda([this, onComplete, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget, foamOutTarget](FFourierComponents fourierComponents)
	{
		FOnButterflyTextureReady onButterflyTextureReady;

		onButterflyTextureReady.BindLambda([this, fourierComponents, onComplete, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget, foamOutTarget](TRefCountPtr<IPooledRenderTarget> butterflyTexture)
		{
			ENQUEUE_RENDER_COMMAND(HeightComputeCmd)([this, fourierComponents, onComplete, butterflyTexture, displacementOutXTarget, displacementOutYTarget, displacementOutZTarget, foamOutTarget](FRHICommandListImmediate& rhiCmdList) mutable
            {
                FRDGBuilder rdgBuilder(rhiCmdList);
				TShaderMapRef<FFFTComputeShader> fftCompute(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        
                // Create height texture on GPU
                FRDGTextureDesc textureDesc = FRDGTextureDesc::Create2D(
                    FIntPoint(mSpectrumParameters.N, mSpectrumParameters.N),
                    PF_A32B32G32R32F,
                    FClearValueBinding(),
                    TexCreate_UAV
                );
        		
                FRDGTextureRef butterflyRef = rdgBuilder.RegisterExternalTexture(butterflyTexture);
                FRDGTextureUAVRef butterflyTextureUAV = rdgBuilder.CreateUAV({butterflyRef});

				TRefCountPtr<IPooledRenderTarget> displacementTexturesInternal[] { nullptr, nullptr, nullptr };
				TRefCountPtr<IPooledRenderTarget> foamTextureInternal;
				UTextureRenderTarget2D* displacementTargets[] { displacementOutXTarget, displacementOutYTarget, displacementOutZTarget };
				FRDGTextureRef displacementTextures[] { 
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutX")),
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutY")),
					rdgBuilder.CreateTexture(textureDesc, TEXT("Displacement_OutZ"))
				};

				FRDGTextureUAVRef displacementTexturesUAV[] { nullptr, nullptr, nullptr };

				for (int axis = 0; axis < 3; axis++)
				{
	                FRDGTextureRef fourierComponentsRef = rdgBuilder.RegisterExternalTexture(fourierComponents.Components[axis]);
                    FRDGTextureUAVRef pingpong0UAV = rdgBuilder.CreateUAV({fourierComponentsRef});
                    
                    FRDGTextureRef pingPong1Texture = rdgBuilder.CreateTexture(textureDesc, TEXT("FFT_PingPong1_Out"));
                    FRDGTextureUAVRef pingPong1Texture_UAV = rdgBuilder.CreateUAV({ pingPong1Texture });
                    FRDGTextureUAVRef pingpong1UAV = pingPong1Texture_UAV;
    
                    enum class FFTDirection { Horizontal, Vertical };
                    const int numStages = log2(mSpectrumParameters.N);
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
                                [fftCompute, params, this](FRHICommandListImmediate& passRhiCmdList)
                            {	
                                FComputeShaderUtils::Dispatch(passRhiCmdList, fftCompute, *params,
                                FIntVector(
                                    FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                                    FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                                    1)
                                );
                            });
                        }
                    }
					
                    FInversionComputeShader::FParameters* inversionParams = rdgBuilder.AllocParameters<FInversionComputeShader::FParameters>();
                    inversionParams->pingpong0 = pingpong0UAV;
                    inversionParams->pingpong1 = pingpong1UAV;
                    inversionParams->N = mSpectrumParameters.N;
                    inversionParams->pingpong = pingpong % 2;
                    inversionParams->displacement = displacementTexturesUAV[axis] = rdgBuilder.CreateUAV({ displacementTextures[axis] });
                    
                    TShaderMapRef<FInversionComputeShader> inversionCompute (GetGlobalShaderMap(GMaxRHIFeatureLevel));
                    rdgBuilder.AddPass(
                    	RDG_EVENT_NAME("InversionComputePass"),
                    	inversionParams,
                    	ERDGPassFlags::Compute,
                    	[inversionParams, inversionCompute, this](FRHICommandListImmediate& passRhiCmdList)
                    {	
                    	FComputeShaderUtils::Dispatch(passRhiCmdList, inversionCompute, *inversionParams,
                    	FIntVector(
                    		FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    		FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    		1)
                    	);
                    });
				}

				FRDGTextureRef normals = rdgBuilder.CreateTexture(textureDesc, TEXT("Normals_Out"));
				FNormalsComputeShader::FParameters* normalsParams = rdgBuilder.AllocParameters<FNormalsComputeShader::FParameters>();
				normalsParams->displacementX = displacementTexturesUAV[0];
				normalsParams->displacementY = displacementTexturesUAV[2];
				normalsParams->normals = rdgBuilder.CreateUAV({ normals });
				
				TShaderMapRef<FNormalsComputeShader> normalsCompute (GetGlobalShaderMap(GMaxRHIFeatureLevel));
                rdgBuilder.AddPass(
                    RDG_EVENT_NAME("NormalsComputePass"),
                    normalsParams,
                    ERDGPassFlags::Compute,
                    [normalsParams, normalsCompute, this](FRHICommandListImmediate& passRhiCmdList)
                {	
                    FComputeShaderUtils::Dispatch(passRhiCmdList, normalsCompute, *normalsParams,
                    FIntVector(
                    	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    	1)
                    );
                });
				
				FRDGTextureRef foamTexture = rdgBuilder.CreateTexture(textureDesc, TEXT("Foam_Out"));
				FFoamComputeShader::FParameters* foamParams = rdgBuilder.AllocParameters<FFoamComputeShader::FParameters>();
				foamParams->normals = normalsParams->normals;
				foamParams->foam = rdgBuilder.CreateUAV({ foamTexture });
				this->mLastFoamTexture = foamTextureInternal;
				
				TShaderMapRef<FFoamComputeShader> foamCompute (GetGlobalShaderMap(GMaxRHIFeatureLevel));
                rdgBuilder.AddPass(
                    RDG_EVENT_NAME("FoamComputePass"),
                    foamParams,
                    ERDGPassFlags::Compute,
                    [foamParams, foamCompute, this](FRHICommandListImmediate& passRhiCmdList)
                {	
                    FComputeShaderUtils::Dispatch(passRhiCmdList, foamCompute, *foamParams,
                    FIntVector(
                    	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    	FMath::DivideAndRoundUp(mSpectrumParameters.N, NUM_THREADS_PER_GROUP_DIMENSION),
                    	1)
                    );
                });
				
                rdgBuilder.QueueTextureExtraction(displacementTextures[0], &displacementTexturesInternal[0]);
                rdgBuilder.QueueTextureExtraction(displacementTextures[1], &displacementTexturesInternal[1]);
				rdgBuilder.QueueTextureExtraction(displacementTextures[2], &displacementTexturesInternal[2]);
				rdgBuilder.QueueTextureExtraction(foamTexture, &foamTextureInternal);
                rdgBuilder.Execute();
				
				rhiCmdList.CopyTexture(displacementTexturesInternal[0]->GetRHI(), displacementTargets[0]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
				rhiCmdList.CopyTexture(displacementTexturesInternal[1]->GetRHI(), displacementTargets[1]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
				rhiCmdList.CopyTexture(displacementTexturesInternal[2]->GetRHI(), displacementTargets[2]->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
				rhiCmdList.CopyTexture(foamTextureInternal->GetRHI(), foamOutTarget->GetRenderTargetResource()->GetTextureRHI(), FRHICopyTextureInfo());
            });	
		});
		
		ComputeButterfly(onButterflyTextureReady);
	});

	ComputeFourierComponents(time, onFourierComponentsReady);
}


OceanTextureManager* OceanTextureManager::mSingleton;
TRefCountPtr<IPooledRenderTarget> OceanTextureManager::mLastFoamTexture;
