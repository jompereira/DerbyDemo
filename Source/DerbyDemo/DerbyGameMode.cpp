// Fill out your copyright notice in the Description page of Project Settings.

#include "DerbyGameMode.h"
#include "DerbyDemoGameState.h"
#include "DerbyDemoPawn.h"
#include "DerbyDemo.h"
#include "Kismet/GameplayStatics.h"

void ADerbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Seed the countdown counter on the GameState so Blueprint HUD can read it immediately.
	if (ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>())
	{
		GS->CountdownSecondsRemaining = StartRoundSeconds;
	}

	// Fire once per second; CountdownTick handles its own termination.
	GetWorldTimerManager().SetTimer(CountdownTickHandle, this, &ADerbyGameMode::CountdownTick, 1.0f, true, 1.0f);

	// Subscribe to every pawn's elimination event so we can detect last-car-standing.
	RegisterVehicles();
}

void ADerbyGameMode::RegisterVehicles()
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADerbyDemoPawn::StaticClass(), Actors);

	TrackedVehicles.Reset();
	for (AActor* Actor : Actors)
	{
		if (ADerbyDemoPawn* Pawn = Cast<ADerbyDemoPawn>(Actor))
		{
			TrackedVehicles.Add(Pawn);
			Pawn->OnVehicleEliminated.AddDynamic(this, &ADerbyGameMode::OnVehicleEliminated);
		}
	}

	if (ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>())
	{
		GS->VehiclesRemaining = TrackedVehicles.Num();
	}

	UE_LOG(LogDerbyDemo, Log, TEXT("DerbyGameMode: registered %d vehicles."), TrackedVehicles.Num());
}

void ADerbyGameMode::OnVehicleEliminated(ADerbyDemoPawn* Eliminated)
{
	TrackedVehicles.Remove(Eliminated);

	// Count vehicles that are still alive.
	int32 AliveCount = 0;
	ADerbyDemoPawn* LastAlive = nullptr;
	for (ADerbyDemoPawn* V : TrackedVehicles)
	{
		if (IsValid(V) && !V->bIsEliminated)
		{
			++AliveCount;
			LastAlive = V;
		}
	}

	if (ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>())
	{
		GS->VehiclesRemaining = AliveCount;
	}

	UE_LOG(LogDerbyDemo, Log, TEXT("DerbyGameMode: %s eliminated. %d vehicle(s) remaining."),
		*GetNameSafe(Eliminated), AliveCount);

	// One (or zero) vehicles left — the round is over.
	if (AliveCount <= 1)
	{
		EndRound(LastAlive);
	}
}

void ADerbyGameMode::EndRound(ADerbyDemoPawn* Winner)
{
	if (ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>())
	{
		GS->EndRound(Winner);
	}

	UE_LOG(LogDerbyDemo, Log, TEXT("DerbyGameMode: round ended. Winner: %s"),
		Winner ? *GetNameSafe(Winner) : TEXT("none"));
}

void ADerbyGameMode::CountdownTick()
{
	ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>();
	if (!GS)
	{
		return;
	}

	GS->CountdownSecondsRemaining = FMath::Max(0, GS->CountdownSecondsRemaining - 1);
	GS->CountdownTickDelegate.Broadcast(GS->CountdownSecondsRemaining);

	if (GS->CountdownSecondsRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTickHandle);
		StartRound();
	}
}

void ADerbyGameMode::StartRound()
{
	if (ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>())
	{
		GS->BeginRound();
	}
}
