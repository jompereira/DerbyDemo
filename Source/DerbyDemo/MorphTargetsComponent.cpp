// Fill out your copyright notice in the Description page of Project Settings.

#include "MorphTargetsComponent.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

UMorphTargetsComponent::UMorphTargetsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UMorphTargetsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMorphTargetsComponent, DamageState);
}

// Called when the game starts
void UMorphTargetsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (DamageMaterialParameter != NAME_None)
	{
		if (USkeletalMeshComponent* Mesh = FindMeshComponent())
		{
			DamageMaterialInstance = Mesh->CreateDynamicMaterialInstance(DamageMaterialSlot);
		}
	}
}

void UMorphTargetsComponent::OnRegister()
{
	Super::OnRegister();

	if (!MeshComponent)
	{
		MeshComponent = FindMeshComponent();
	}
}

// Called every frame
void UMorphTargetsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDebugDraw)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = FindMeshComponent();
	if (!Mesh)
	{
		return;
	}

	UWorld* World = GetWorld();
	for (const FMorphTargetData& Data : MorphTargets)
	{
		if (!Mesh->DoesSocketExist(Data.SocketName))
		{
			continue;
		}

		const FDamageEntry* Entry = DamageState.FindByPredicate([&](const FDamageEntry& E) { return E.SocketName == Data.SocketName; });
		const float Cached = Entry ? Entry->Accumulated : 0.f;
		const float BlendWeight = Data.Durability > 0.f ? Cached / Data.Durability : 0.f;
		const FVector Location = Mesh->GetSocketLocation(Data.SocketName);
		const FString Label = FString::Printf(TEXT("%s: %.2f"), *Data.SocketName.ToString(), BlendWeight);
		DrawDebugString(World, Location, Label, nullptr, FColor::Orange, 0.f, true);
	}
}

USkeletalMeshComponent* UMorphTargetsComponent::FindMeshComponent() const
{
	if (MeshComponent)
	{
		return MeshComponent;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		Owner = GetTypedOuter<AActor>();
	}
	if (Owner)
	{
		return Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	return nullptr;
}

TArray<FString> UMorphTargetsComponent::GetMorphTargetSocketOptions() const
{
	TArray<FString> Options;

	if (USkeletalMeshComponent* Mesh = FindMeshComponent())
	{
		for (const FName& Name : Mesh->GetAllSocketNames())
		{
			if (Name.ToString().StartsWith(MorphTargetSocketPrefix))
			{
				Options.Add(Name.ToString());
			}
		}
	}
	return Options;
}

void UMorphTargetsComponent::ApplyDamage(FName SocketName, float DamageAmount)
{
	// Damage is server-authoritative; clients receive the result via OnRep_DamageState.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	USkeletalMeshComponent* Mesh = FindMeshComponent();
	if (SocketName == NAME_None || !Mesh)
	{
		return;
	}

	const FMorphTargetData* Data = MorphTargets.FindByPredicate([&](const FMorphTargetData& D)
	{
		return D.SocketName == SocketName;
	});

	if (!Data || Data->Durability <= 0.f || !Mesh->DoesSocketExist(SocketName))
	{
		return;
	}

	FDamageEntry* Entry = DamageState.FindByPredicate([&](const FDamageEntry& E) { return E.SocketName == SocketName; });
	if (!Entry)
	{
		Entry = &DamageState.AddDefaulted_GetRef();
		Entry->SocketName = SocketName;
	}
	Entry->Accumulated = FMath::Min(Entry->Accumulated + DamageAmount, Data->Durability);

	Mesh->SetMorphTarget(SocketName, Entry->Accumulated / Data->Durability);

	RefreshBodyScale(Mesh);
	RefreshDamageMaterial();
}

void UMorphTargetsComponent::ApplyDamageAtLocation(FVector WorldHitLocation, FVector WorldHitNormal, float MaxDistance, float DamageAmount)
{
	const FName Socket = GetClosestMTSocket(WorldHitLocation, WorldHitNormal, MaxDistance);
	if (Socket != NAME_None)
	{
		ApplyDamage(Socket, DamageAmount);
	}
}

void UMorphTargetsComponent::OnRep_DamageState()
{
	USkeletalMeshComponent* Mesh = FindMeshComponent();
	if (!Mesh)
	{
		return;
	}

	for (const FDamageEntry& Entry : DamageState)
	{
		const FMorphTargetData* Data = MorphTargets.FindByPredicate([&](const FMorphTargetData& D) { return D.SocketName == Entry.SocketName; });
		if (Data && Data->Durability > 0.f)
		{
			Mesh->SetMorphTarget(Entry.SocketName, Entry.Accumulated / Data->Durability);
		}
	}

	RefreshBodyScale(Mesh);
	RefreshDamageMaterial();
}

FName UMorphTargetsComponent::GetClosestMTSocket(FVector WorldHitLocation, FVector WorldHitNormal, const float MaxDistance) const
{
	USkeletalMeshComponent* Mesh = FindMeshComponent();
	if (!Mesh)
	{
		return NAME_None;
	}

	// Project into local space and mask out the dominant normal axis so that sockets
	// are compared in the plane of impact rather than in 3D. This prevents corner
	// sockets from winning over a center socket purely because the physics contact
	// point is offset in depth along the hull surface.
	const FTransform MeshTransform = Mesh->GetComponentTransform();
	const FVector LocalHit = MeshTransform.InverseTransformPosition(WorldHitLocation);
	const FVector LocalNormal = MeshTransform.InverseTransformVectorNoScale(WorldHitNormal).GetSafeNormal();
	const FVector AbsNormal = LocalNormal.GetAbs();

	// Build a mask that zeroes the dominant axis (depth direction)
	FVector PlaneMask = FVector::OneVector;
	if (AbsNormal.X >= AbsNormal.Y && AbsNormal.X >= AbsNormal.Z)      PlaneMask.X = 0.f;
	else if (AbsNormal.Y >= AbsNormal.X && AbsNormal.Y >= AbsNormal.Z) PlaneMask.Y = 0.f;
	else                                                                 PlaneMask.Z = 0.f;

	FName ClosestSocket = NAME_None;
	float ClosestDistSq = MAX_FLT;
	const float MaxDistanceSquared = MaxDistance * MaxDistance;

	for (const FMorphTargetData& Data : MorphTargets)
	{
		if (!Mesh->DoesSocketExist(Data.SocketName))
		{
			continue;
		}

		const FVector WorldSocketPos = Mesh->GetSocketLocation(Data.SocketName);
		if (FVector::DistSquared(WorldSocketPos, WorldHitLocation) >= MaxDistanceSquared)
		{
			continue;
		}

		const FVector LocalSocket = MeshTransform.InverseTransformPosition(WorldSocketPos);
		const FVector Delta = (LocalHit - LocalSocket) * PlaneMask;
		const float DistSq = Delta.SizeSquared();
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			ClosestSocket = Data.SocketName;
		}
	}

	return ClosestSocket;
}

void UMorphTargetsComponent::RefreshDamageMaterial()
{
	if (!DamageMaterialInstance || DamageMaterialParameter == NAME_None)
	{
		return;
	}

	float TotalRatio = 0.f;
	for (const FMorphTargetData& D : MorphTargets)
	{
		if (D.Durability > 0.f)
		{
			const FDamageEntry* Entry = DamageState.FindByPredicate([&](const FDamageEntry& E) { return E.SocketName == D.SocketName; });
			TotalRatio += Entry ? Entry->Accumulated / D.Durability : 0.f;
		}
	}
	const float AvgRatio = MorphTargets.Num() > 0 ? TotalRatio / MorphTargets.Num() : 0.f;
	DamageMaterialInstance->SetScalarParameterValue(DamageMaterialParameter, AvgRatio);
}

void UMorphTargetsComponent::RefreshBodyScale(USkeletalMeshComponent* Mesh)
{
	if (MaxBodyShrink <= 0.f || MorphTargets.IsEmpty())
	{
		return;
	}

	float TotalRatio = 0.f;
	for (const FMorphTargetData& D : MorphTargets)
	{
		if (D.Durability > 0.f)
		{
			const FDamageEntry* Entry = DamageState.FindByPredicate([&](const FDamageEntry& E) { return E.SocketName == D.SocketName; });
			TotalRatio += Entry ? Entry->Accumulated / D.Durability : 0.f;
		}
	}
	const float AvgRatio = TotalRatio / MorphTargets.Num();

	// All MT_ sockets sit on the root bone, so any socket name resolves to the same body
	FBodyInstance* BI = Mesh->GetBodyInstance();
	if (BI)
	{
		const float Scale = 1.0f - MaxBodyShrink * AvgRatio;
		BI->UpdateBodyScale(FVector(Scale), true);
	}
}

