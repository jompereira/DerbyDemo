// Fill out your copyright notice in the Description page of Project Settings.

#include "MorphTargetsComponent.h"

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

	MeshComponent = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	
}


// Called every frame
void UMorphTargetsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

TArray<FString> UMorphTargetsComponent::GetMorphTargetSocketOptions() const
{
	TArray<FString> Options;
	if (MeshComponent)
	{
		for (const FName& Name : MeshComponent->GetAllSocketNames())
		{
			if (Name.ToString().StartsWith(MorphTargetSocketPrefix))
			{
				Options.Add(Name.ToString());
			}
		}
	}
	return Options;

}

FName UMorphTargetsComponent::GetClosestMTSocket(FVector WorldHitLocation, const float MaxDistance) const
{
	FName ClosestSocket = NAME_None;
	float ClosestDistSq = MAX_FLT;
	const float MaxDistanceSquared = MaxDistance * MaxDistance;

	for (const FName& SocketName : MeshComponent->GetAllSocketNames())
	{
		if (!SocketName.ToString().StartsWith(MorphTargetSocketPrefix))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MeshComponent->GetSocketLocation(SocketName), WorldHitLocation);
		if (DistSq < ClosestDistSq && DistSq < MaxDistanceSquared)
		{
			ClosestDistSq = DistSq;
			ClosestSocket = SocketName;
		}
	}

	return ClosestSocket;
}

