// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DerbyGameMode.generated.h"

/**
 * Base game mode for derby maps.
 * Owns the StartRound() decision; state and the delegate live in ADerbyDemoGameState.
 *
 * On BeginPlay the countdown timer fires once per second, decrementing
 * GameState::CountdownSecondsRemaining until it reaches zero, then calls StartRound().
 */
UCLASS()
class DERBYDEMO_API ADerbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Countdown duration before the round begins (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derby|Round")
	int32 StartRoundSeconds = 3;

	/** Transitions the GameState to InProgress and broadcasts StartRoundDelegate. */
	UFUNCTION(BlueprintCallable, Category = "Derby|Round")
	void StartRound();

protected:
	virtual void BeginPlay() override;

private:
	/** Fires once per second during the pre-round countdown. */
	UFUNCTION()
	void CountdownTick();

	FTimerHandle CountdownTickHandle;
};
