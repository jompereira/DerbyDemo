// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DerbyDemoGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStartRoundDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCountdownTickDelegate, int32, SecondsRemaining);

UENUM(BlueprintType)
enum class ERoundPhase : uint8
{
	PreRound   UMETA(DisplayName = "Pre Round"),
	InProgress UMETA(DisplayName = "In Progress"),
};

/**
 * Holds authoritative round state readable by HUD, controllers, and AI.
 * GameMode::StartRound() drives transitions; everything else observes.
 */
UCLASS()
class DERBYDEMO_API ADerbyDemoGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** Current phase — read from HUD, controllers, AI to gate behavior. */
	UPROPERTY(BlueprintReadOnly, Category = "Derby|Round")
	ERoundPhase RoundPhase = ERoundPhase::PreRound;

	/** Seconds remaining before the round starts. Counts down from StartRoundSeconds to 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Derby|Round")
	int32 CountdownSecondsRemaining = 0;

	/** Broadcast every second during the countdown with the new remaining value. */
	UPROPERTY(BlueprintAssignable, Category = "Derby|Round")
	FCountdownTickDelegate CountdownTickDelegate;

	/** Broadcast when the round begins. Bind from Blueprint or C++ (AddDynamic). */
	UPROPERTY(BlueprintAssignable, Category = "Derby|Round")
	FStartRoundDelegate StartRoundDelegate;

	/** Called by ADerbyGameMode::StartRound() — sets phase to InProgress and broadcasts. */
	void BeginRound();
};
