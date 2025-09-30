// Fill out your copyright notice in the Description page of Project Settings.


#include "ComputeTester.h"

#include "ButterflyTextureComputeShader.h"
#include "OceanComputeShaderDispatcher.h"


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

}

void AComputeTester::Fire(UTextureRenderTarget2D* t)
{
	//OceanComputeShaderDispatcher::Get()->DrawButterfly(256, Target);
	//OceanComputeShaderDispatcher::Get()->ComputeFourierComponents(256, Target);
	OceanComputeShaderDispatcher::Get()->ComputeDisplacement(256, OceanComputeShaderDispatcher::FOnFourierComponentsReady(), Target);
}

