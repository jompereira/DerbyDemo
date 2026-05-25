// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DecalPoolSubsystem.generated.h"

class ADecalActor;
class UDecalComponent;
class UMaterialInterface;
class USceneComponent;

/**
 * World subsystem that maintains a fixed-size ring-buffer pool of ADecalActors.
 *
 * Auto-created with every world (all three maps) — no level placement required.
 * Decals are cosmetic and client-side only; no replication.
 *
 * Usage from Blueprint (e.g. inside OnImpact):
 *   Get World Subsystem (DecalPoolSubsystem)
 *     → SpawnPooledDecal(Material, Location, Rotation, Size, ..., AttachTo: GetMesh)
 *
 * Pass the vehicle's skeletal mesh as AttachTo so the decal moves with the car.
 * Omit AttachTo (or pass nullptr) for world-space decals (e.g. ground skids).
 *
 * Rotation tip: use the same rotation you would pass to SpawnDecalAttached.
 *   For vehicle body impacts: RotateVector(HitNormal, 90°,0,0) → RotationFromXVector.
 */
UCLASS()
class DERBYDEMO_API UDecalPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Number of decal actors to pre-allocate.
	 * When all slots are in use the oldest one is silently recycled.
	 * Must be set before the first SpawnPooledDecal call; changes afterwards have no effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decal Pool")
	int32 PoolSize = 32;

	/**
	 * Grab the next slot from the pool, configure and place the decal, and return the component.
	 *
	 * @param Material      Decal material (must use the Deferred Decal blend mode)
	 * @param Location      World-space position of the decal (hit point on the surface)
	 * @param Rotation      Projection direction. Decals project along their local +X axis.
	 *                      The correct rotation depends on the surface being hit — use the
	 *                      SAME rotation you would pass to SpawnDecalAttached for the same
	 *                      surface. For vehicle body impacts the working convention (matching
	 *                      SpawnDecalAttached) is: RotateVector(HitNormal, 90°,0,0) → RotationFromXVector.
	 * @param Size          Half-extents of the decal box in cm: X = depth, Y = width, Z = height
	 * @param LifeSpan      Seconds before the decal begins fading out (default 8 s).
	 *                      Pass 0 to disable fade; the decal stays visible until the slot is recycled.
	 * @param FadeDuration  Seconds the decal takes to fade to invisible (default 1.5 s).
	 *                      Ignored when LifeSpan is 0.
	 * @param AttachTo      If set, the decal component is attached to this component so it moves
	 *                      with it (e.g. pass GetMesh() for vehicle body decals). Pass nullptr
	 *                      for world-space decals such as ground skid marks.
	 * @return              The live UDecalComponent, or nullptr if the pool could not be initialised
	 */
	UFUNCTION(BlueprintCallable, Category="Decal Pool")
	UDecalComponent* SpawnPooledDecal(
		UMaterialInterface* Material,
		FVector             Location,
		FRotator            Rotation,
		FVector             Size,
		float               LifeSpan     = 8.0f,
		float               FadeDuration = 1.5f,
		USceneComponent*    AttachTo     = nullptr
	);

private:

	/** Pre-allocated ring-buffer of decal actors. */
	UPROPERTY()
	TArray<TObjectPtr<ADecalActor>> Pool;

	/** Index of the next slot to hand out. */
	int32 NextSlot = 0;

	bool bPoolInitialized = false;

	/** Spawns PoolSize ADecalActors and hides them. Called lazily on first use. */
	void InitPool();
};
