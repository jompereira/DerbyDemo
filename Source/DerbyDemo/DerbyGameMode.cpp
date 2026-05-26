// Fill out your copyright notice in the Description page of Project Settings.

#include "DerbyGameMode.h"
#include "DerbyDemoGameState.h"

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
}

void ADerbyGameMode::CountdownTick()
{
	ADerbyDemoGameState* GS = GetGameState<ADerbyDemoGameState>();
	if (!GS)
	{
		return;
	}

	GS->CountdownSecondsRemaining = FMath::Max(0, GS->CountdownSecondsRemaining - 1);

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
