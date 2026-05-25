// Fill out your copyright notice in the Description page of Project Settings.

#include "DecalPoolSubsystem.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "DerbyDemo.h"

// ---------------------------------------------------------------------------
// USubsystem interface
// ---------------------------------------------------------------------------

void UDecalPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Pool creation is deferred to first use so the world is guaranteed to be
	// ready to spawn actors when InitPool() is called.
	
	InitPool();
}

void UDecalPoolSubsystem::Deinitialize()
{
	// The world tears down all its actors anyway; just clear our references.
	Pool.Empty();
	bPoolInitialized = false;
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Pool initialisation (lazy, first use)
// ---------------------------------------------------------------------------

void UDecalPoolSubsystem::InitPool()
{
	if (bPoolInitialized) return;

	UWorld* World = GetWorld();
	if (!ensureMsgf(World, TEXT("UDecalPoolSubsystem::InitPool — no valid world")))
	{
		return;
	}

	Pool.Reserve(PoolSize);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient; // never saved into the level

	for (int32 i = 0; i < PoolSize; ++i)
	{
		ADecalActor* Actor = World->SpawnActor<ADecalActor>(
			ADecalActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params
		);

		if (!Actor) continue;

		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);

		if (UDecalComponent* DC = Actor->GetDecal())
		{
			// Never let the engine destroy the actor after the fade ends —
			// the whole point of the pool is to keep these actors alive for reuse.
			DC->bDestroyOwnerAfterFade = false;
		}

		Pool.Add(Actor);
	}

	bPoolInitialized = true;
	UE_LOG(LogDerbyDemo, Log, TEXT("UDecalPoolSubsystem: pool ready with %d slots"), Pool.Num());
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

UDecalComponent* UDecalPoolSubsystem::SpawnPooledDecal(
	UMaterialInterface* Material,
	FVector             Location,
	FRotator            Rotation,
	FVector             Size,
	float               LifeSpan,
	float               FadeDuration,
	USceneComponent*    AttachTo)
{
	if (!bPoolInitialized)
	{
		InitPool();
	}

	if (Pool.IsEmpty())
	{
		UE_LOG(LogDerbyDemo, Warning, TEXT("UDecalPoolSubsystem::SpawnPooledDecal — pool is empty"));
		return nullptr;
	}

	// Take the next slot. If it is still fading this silently recycles it,
	// which is intentional: in a heavy crash the oldest decal is the one least
	// likely to still be visible.
	ADecalActor* Slot = Pool[NextSlot];
	NextSlot = (NextSlot + 1) % Pool.Num();

	if (!IsValid(Slot)) return nullptr;

	UDecalComponent* Decal = Slot->GetDecal();
	if (!IsValid(Decal)) return nullptr;

	// --- Detach from any previous parent ---
	// A recycled slot may still be attached to a different vehicle.
	Slot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// --- Configure material and size while still hidden ---
	// Setting these before the proxy is (re)built avoids intermediate dirty
	// proxy recreations and matches the order used in CreateDecalComponent
	// (the internal helper called by SpawnDecalAttached).
	Decal->SetDecalMaterial(Material);
	Decal->DecalSize = Size;
	// SetUsingAbsoluteScale must be in place before attachment so the proxy
	// reads the correct scale when it is constructed. This matches
	// CreateDecalComponent which also sets it before RegisterComponentWithWorld.
	Decal->SetUsingAbsoluteScale(true);

	// --- Attach then set world position ---
	// SpawnDecalAttached (CreateDecalComponent) attaches with KeepRelativeTransform
	// and then calls SetWorldLocationAndRotation — use the same sequence so the
	// proxy's transform is computed identically.
	if (AttachTo)
	{
		Decal->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform);
		Decal->SetWorldLocationAndRotation(Location, Rotation);
	}
	else
	{
		Decal->SetWorldLocationAndRotation(Location, Rotation);
	}

	// --- Fade / lifetime ---
	// WARNING: SetFadeOut internally calls SetLifeSpan(FadeStart+FadeDuration),
	// which schedules LifeSpanCallback → DestroyComponent().  Cancel that timer
	// immediately with SetLifeSpan(0); the visual fade in the render proxy is
	// driven by the render-thread parameters set by SetFadeOut and is unaffected.
	if (FadeDuration > 0.0f)
	{
		Decal->SetFadeOut(LifeSpan, FadeDuration, /*bDestroyOwnerAfterFade=*/false);
		Decal->SetLifeSpan(0.0f);
	}
	else
	{
		// No fade animation requested — clear any fade params left by a previous
		// use so the decal renders at full opacity for its full lifetime.
		Decal->FadeStartDelay = 0.0f;
		Decal->FadeDuration   = 0.0f;
		Decal->SetLifeSpan(0.0f);
	}

	// --- Unhide last ---
	// SetActorHiddenInGame(false) triggers the final MarkRenderStateDirty so
	// the proxy is rebuilt exactly once, after all configuration is in place,
	// with DrawInGame=true.
	Slot->SetActorHiddenInGame(false);

	return Decal;
}
