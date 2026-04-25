// Fill out your copyright notice in the Description page of Project Settings.

#include "MorphTargetsComponent.h"
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UMorphTargetsComponent::UMorphTargetsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void UMorphTargetsComponent::BeginPlay()
{
	Super::BeginPlay();
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

		const float Cached = DamageCache.FindRef(Data.SocketName);
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
	USkeletalMeshComponent* Mesh = FindMeshComponent();
	if (SocketName == NAME_None || !Mesh)
	{
		return;
	}

	const FMorphTargetData* Data = MorphTargets.FindByPredicate([&](const FMorphTargetData& D)
	{
		return D.SocketName == SocketName;
	});

	if (!Data || Data->Durability <= 0.f)
	{
		return;
	}

	if (!Mesh->DoesSocketExist(SocketName))
	{
		return;
	}

	float& Cached = DamageCache.FindOrAdd(SocketName);
	Cached = FMath::Min(Cached + DamageAmount, Data->Durability);

	Mesh->SetMorphTarget(SocketName, Cached / Data->Durability);
}

void UMorphTargetsComponent::ApplyDamageAtLocation(FVector WorldHitLocation, FVector WorldHitNormal, float MaxDistance, float DamageAmount)
{
	const FName Socket = GetClosestMTSocket(WorldHitLocation, WorldHitNormal, MaxDistance);
	if (Socket != NAME_None)
	{
		ApplyDamage(Socket, DamageAmount);
	}
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

