// Fill out your copyright notice in the Description page of Project Settings.


#include "ComputeTester.h"

#include "ButterflyTextureComputeShader.h"
#include "OceanTextureManager.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AComputeTester::AComputeTester()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;		

}

// Called when the game starts or when spawned
void AComputeTester::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AComputeTester::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	static auto lastA = A;
	static auto lastL = L;
	static auto lastWindDirection = WindDirection;
	static auto lastWindSpeed = WindSpeed;
	
	if (A != lastA || L != lastL || lastWindDirection != WindDirection || lastWindSpeed != WindSpeed)
	{

	}
}

void AComputeTester::Fire(UTextureRenderTarget2D* t)
{
	//OceanComputeShaderDispatcher::Get()->DrawButterfly(256, Target);
	//OceanComputeShaderDispatcher::Get()->ComputeFourierComponents(256, Target);

	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 10.f, FColor::Red, FString::FromInt(FDateTime::Now().GetMillisecond() / 1000.f));
	OceanTextureManager::Get()->ComputeDisplacement(UGameplayStatics::GetRealTimeSeconds(GetWorld()) + 10000.f, OceanTextureManager::FOnDisplacementFieldReady(), X, Y, Z, Foam);
}


void AComputeTester::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 10.f, FColor::Red, "Recalculating initial spectra");
                                         		
    OceanTextureManager::FSpectrumParameters spectrumParameters;
    spectrumParameters.A = A;
    spectrumParameters.L = L;
    spectrumParameters.WindDirection = WindDirection;
    spectrumParameters.WindSpeed = WindSpeed;

    OceanTextureManager::Get()->SetSpectrumParameters(spectrumParameters);
}