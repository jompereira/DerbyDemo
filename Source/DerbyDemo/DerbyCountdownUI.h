// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DerbyCountdownUI.generated.h"

/**
 * Pre-round countdown HUD widget.
 *
 * Subscribes to ADerbyDemoGameState::CountdownTickDelegate (fires every second)
 * and StartRoundDelegate (fires when the round begins). All animation and visual
 * logic is implemented in Blueprint via the BP_ events below.
 *
 * Blueprint responsibilities:
 *   BP_OnCountdownTick  — update the number display; play a per-tick animation
 *   BP_OnRoundStarted   — show "GO!", play outro, then call RemoveFromParent
 */
UCLASS(abstract)
class DERBYDEMO_API UDerbyCountdownUI : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Subscribes to GameState delegates and seeds the initial display. */
	virtual void NativeConstruct() override;

	/** Unsubscribes from GameState delegates. */
	virtual void NativeDestruct() override;

	/** Called every second with the updated seconds remaining (3 → 2 → 1 → 0). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Derby|Countdown")
	void BP_OnCountdownTick(int32 SecondsRemaining);

	/** Called once when the round transitions to InProgress. Show "GO!" then RemoveFromParent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Derby|Countdown")
	void BP_OnRoundStarted();

private:

	UFUNCTION()
	void OnCountdownTick(int32 SecondsRemaining);

	UFUNCTION()
	void OnRoundStarted();
};
