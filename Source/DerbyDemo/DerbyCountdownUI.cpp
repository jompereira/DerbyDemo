// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyCountdownUI.h"
#include "DerbyDemoGameState.h"

void UDerbyCountdownUI::NativeConstruct()
{
	Super::NativeConstruct();

	ADerbyDemoGameState* GS = GetWorld()->GetGameState<ADerbyDemoGameState>();
	if (!GS)
	{
		return;
	}

	GS->CountdownTickDelegate.AddDynamic(this, &UDerbyCountdownUI::OnCountdownTick);
	GS->StartRoundDelegate.AddDynamic(this, &UDerbyCountdownUI::OnRoundStarted);

	// Seed the display immediately so the player sees the starting number right away
	// rather than waiting for the first tick one second later.
	BP_OnCountdownTick(GS->CountdownSecondsRemaining);
}

void UDerbyCountdownUI::NativeDestruct()
{
	if (ADerbyDemoGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADerbyDemoGameState>() : nullptr)
	{
		GS->CountdownTickDelegate.RemoveDynamic(this, &UDerbyCountdownUI::OnCountdownTick);
		GS->StartRoundDelegate.RemoveDynamic(this, &UDerbyCountdownUI::OnRoundStarted);
	}

	Super::NativeDestruct();
}

void UDerbyCountdownUI::OnCountdownTick(int32 SecondsRemaining)
{
	BP_OnCountdownTick(SecondsRemaining);
}

void UDerbyCountdownUI::OnRoundStarted()
{
	BP_OnRoundStarted();
}
