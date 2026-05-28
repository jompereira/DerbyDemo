// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DerbyDemoPlayerController.generated.h"

class UInputMappingContext;
class ADerbyDemoPawn;
class UDerbyDemoUI;
class UDerbyCountdownUI;
class UDerbyResultsUI;

/**
 *  Vehicle Player Controller class
 *  Handles input mapping and user interface
 */
UCLASS(abstract, Config="Game")
class ADerbyDemoPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** If true, the optional steering wheel input mapping context will be registered */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls")
	bool bUseSteeringWheelControls = false;

	/** Optional Input Mapping Context to be used for steering wheel input.
	 *  This is added alongside the default Input Mapping Context and does not block other forms of input.
	 */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls", meta = (EditCondition = "bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	/** Type of vehicle to automatically respawn when it's destroyed */
	UPROPERTY(EditAnywhere, Category="Vehicle|Respawn")
	TSubclassOf<ADerbyDemoPawn> VehiclePawnClass;

	/** Pointer to the controlled vehicle pawn */
	TObjectPtr<ADerbyDemoPawn> VehiclePawn;

	/** Type of the UI to spawn */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UDerbyDemoUI> VehicleUIClass;

	/** Pointer to the UI widget */
	UPROPERTY()
	TObjectPtr<UDerbyDemoUI> VehicleUI;

	/** Pre-round countdown widget class to spawn */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UDerbyCountdownUI> CountdownUIClass;

	/** Pointer to the countdown widget */
	UPROPERTY()
	TObjectPtr<UDerbyCountdownUI> CountdownUI;

	/** Results widget class (shown on elimination and when the round ends). */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UDerbyResultsUI> ResultsUIClass;

	/** Pointer to the results widget. */
	UPROPERTY()
	TObjectPtr<UDerbyResultsUI> ResultsUI;
		
protected:

	/** Gameplay initialization — subscribes to round-start delegate */
	virtual void BeginPlay() override;

	/** Cleanup — unsubscribes from round-start delegate */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Input setup */
	virtual void SetupInputComponent() override;

public:

	/** Update vehicle UI on tick */
	virtual void Tick(float Delta) override;

protected:

	/** Pawn setup (server) */
	virtual void OnPossess(APawn* InPawn) override;

	/** Pawn setup (owning client) */
	virtual void AcknowledgePossession(APawn* P) override;

	/** Handles pawn destruction and respawning */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedPawn);

	/** Bound to ADerbyDemoGameState::StartRoundDelegate — enables input on the possessed pawn */
	UFUNCTION()
	void OnRoundStarted();

	/** Bound to the possessed pawn's OnVehicleEliminated — disables input, shows results widget */
	UFUNCTION()
	void OnPawnEliminatedDelegate(ADerbyDemoPawn* EliminatedPawn);

	/** Bound to ADerbyDemoGameState::RoundEndDelegate — shows win/loss result */
	UFUNCTION()
	void OnRoundEnded(AActor* Winner);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
