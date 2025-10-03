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

	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* X;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* Y;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	UTextureRenderTarget2D* Z;
	
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	float L = 1000;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	float A = 4;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	FVector2f WindDirection = FVector2f(1, 1);

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = Ocean)
	float WindSpeed = 40.f;
};
