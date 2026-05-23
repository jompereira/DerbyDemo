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
	// IsValid guards against slots whose UDecalComponent was destroyed by a
	// previous SetFadeOut (see below for why that was happening).
	if (!IsValid(Decal)) return nullptr;

	// --- Detach from any previous parent ---
	// A recycled slot may still be attached to a different vehicle.
	Slot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// --- Place at the world-space hit point, then attach ---
	// Set world transform first, then attach with KeepWorldTransform so the
	// engine computes the correct local offset relative to the new parent.
	// This is the same order SpawnDecalAttached uses internally.
	Slot->SetActorLocationAndRotation(Location, Rotation);
	Slot->SetActorHiddenInGame(false);

	if (AttachTo)
	{
		Slot->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepWorldTransform);
	}

	// --- Configure ---
	Decal->SetDecalMaterial(Material);
	Decal->DecalSize = Size;
	// Match SpawnDecalAttached: make size immune to parent component scale.
	Decal->SetUsingAbsoluteScale(true);

	// Restart the visual fade clock from now.
	//
	// WARNING: UDecalComponent::SetFadeOut always calls SetLifeSpan(FadeStart+FadeDuration)
	// internally, which sets a timer that fires LifeSpanCallback → DestroyComponent().
	// For a pooled actor that must stay alive, this destroys the root UDecalComponent
	// and breaks the slot permanently.  Immediately call SetLifeSpan(0) to cancel
	// that timer; the visual fade in the render proxy is unaffected.
	Decal->SetFadeOut(LifeSpan, FadeDuration, /*bDestroyOwnerAfterFade=*/false);
	Decal->SetLifeSpan(0.0f); // Cancel the self-destruction timer; pool owns the lifetime.

	Decal->MarkRenderStateDirty();

	return Decal;
}
