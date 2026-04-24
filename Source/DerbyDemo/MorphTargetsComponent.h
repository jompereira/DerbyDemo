// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MorphTargetsComponent.generated.h"

USTRUCT(BlueprintType)
struct FMorphTargetData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta=(GetOptions="GetMorphTargetSocketOptions"))
	FName SocketName;
	
	UPROPERTY(EditAnywhere)
	float Durability;
	
};

UCLASS(ClassGroup=(Destruction), meta=(BlueprintSpawnableComponent) )
class DERBYDEMO_API UMorphTargetsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMorphTargetsComponent();

protected:
	
	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> MeshComponent;
	
	UPROPERTY(EditAnywhere)
	FString MorphTargetSocketPrefix = TEXT("MT_");
	
	UPROPERTY(EditAnywhere)
	TArray<FMorphTargetData> MorphTargets;
	
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	UFUNCTION(BlueprintCallable)
	TArray<FString> GetMorphTargetSocketOptions() const;
	
	UFUNCTION(BlueprintCallable)
	FName GetClosestMTSocket(FVector WorldHitLocation, float MaxDistance) const;
};
