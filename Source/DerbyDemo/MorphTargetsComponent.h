// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MorphTargetsComponent.generated.h"

USTRUCT(BlueprintType)
struct FMorphTargetData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
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
	
	UPROPERTY(EditAnywhere)
	bool bDebugDraw;

	/**
	 * Maximum fractional reduction in the physics body scale when all panels are fully deformed.
	 * 0 = no physics body update; 0.15 = shrink 15% at full damage.
	 * All MT_ sockets share the root body, so this scales the entire chassis hull.
	 */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="0.5"))
	float MaxBodyShrink = 0.0f;

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

public:
	UFUNCTION(BlueprintCallable)
	TArray<FString> GetMorphTargetSocketOptions() const;

	/**
	 * Returns the socket closest to WorldHitLocation in the plane of WorldHitNormal.
	 * Depth along the normal is ignored so that e.g. a straight frontal hit maps to the
	 * center bumper socket rather than a corner socket displaced in depth.
	 */
	UFUNCTION(BlueprintCallable)
	FName GetClosestMTSocket(FVector WorldHitLocation, FVector WorldHitNormal, float MaxDistance) const;

	/** Accumulates damage on a named socket and updates its morph target blend weight (0–1 mapped to 0–Durability). */
	UFUNCTION(BlueprintCallable)
	void ApplyDamage(FName SocketName, float DamageAmount);

	/** Finds the closest MT socket and applies DamageAmount to it. Pass the hit normal for accurate panel selection. */
	UFUNCTION(BlueprintCallable)
	void ApplyDamageAtLocation(FVector WorldHitLocation, FVector WorldHitNormal, float MaxDistance, float DamageAmount);

private:

	/** Accumulated damage per socket name; drives morph target blend weights at runtime. */
	TMap<FName, float> DamageCache;

	/** Returns the cached mesh component, or finds it via the outer actor if the cache is cold.
	 *  Uses GetTypedOuter<AActor>() as a fallback because Blueprint-added component archetypes
	 *  have a null OwnerPrivate while their outer chain still leads to the actor CDO. */
	USkeletalMeshComponent* FindMeshComponent() const;

	/** Recomputes the root physics body scale from the average panel damage ratio.
	 *  Called after every ApplyDamage so the single root body stays in sync with all panels. */
	void RefreshBodyScale(USkeletalMeshComponent* Mesh);
};
