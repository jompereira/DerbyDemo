// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DerbyGameMode.generated.h"

class ADerbyDemoPawn;

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

	// -------------------------------------------------------------------------
	// Elimination tracking — last car standing ends the round.
	// -------------------------------------------------------------------------

	/** All vehicles that were alive at round start; shrinks as they are eliminated. */
	UPROPERTY()
	TArray<TObjectPtr<ADerbyDemoPawn>> TrackedVehicles;

public:
	/** Read-only access to the live vehicle list — use this instead of GetAllActorsOfClass. */
	const TArray<TObjectPtr<ADerbyDemoPawn>>& GetTrackedVehicles() const { return TrackedVehicles; }

private:

	/** Finds all ADerbyDemoPawn actors in the level and subscribes to their elimination delegates. */
	void RegisterVehicles();

	/** Bound to each tracked vehicle's OnVehicleEliminated delegate. */
	UFUNCTION()
	void OnVehicleEliminated(ADerbyDemoPawn* Eliminated);

	/** Transitions the game state to PostRound and broadcasts the result. */
	void EndRound(ADerbyDemoPawn* Winner);
};
