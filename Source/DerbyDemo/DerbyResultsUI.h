// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DerbyResultsUI.generated.h"

/**
 * Post-round results widget.
 * Spawned by ADerbyDemoPlayerController when the player is eliminated or the round ends.
 *
 * Blueprint responsibilities:
 *   BP_OnEliminated   — show "Eliminated!" state while waiting for the round to finish
 *   BP_OnRoundEnd     — show the final result ("You Win!" / "Defeated")
 *
 * Both events may fire in sequence (eliminated → round ends) or only BP_OnRoundEnd
 * (player survived to the end). Handle both gracefully in Blueprint.
 */
UCLASS(abstract)
class DERBYDEMO_API UDerbyResultsUI : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the player controller when the local player's vehicle is eliminated mid-round. */
	UFUNCTION(BlueprintCallable, Category="Derby|Results")
	void NotifyEliminated();

	/** Called by the player controller when the round ends. */
	UFUNCTION(BlueprintCallable, Category="Derby|Results")
	void NotifyRoundEnd(bool bLocalPlayerWon);

protected:
	/** Override in Blueprint: show "Eliminated!" and wait for round to finish. */
	UFUNCTION(BlueprintImplementableEvent, Category="Derby|Results")
	void BP_OnEliminated();

	/** Override in Blueprint: show the final result ("You Win!" or "Defeated"). */
	UFUNCTION(BlueprintImplementableEvent, Category="Derby|Results")
	void BP_OnRoundEnd(bool bLocalPlayerWon);
};
