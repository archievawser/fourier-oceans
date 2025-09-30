// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ComputeTester.generated.h"

UCLASS()
class OCEAN_API AComputeTester : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AComputeTester();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION( BlueprintCallable )
	void Fire(UTextureRenderTarget2D* t);

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* X;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* Y;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* Z;
};
