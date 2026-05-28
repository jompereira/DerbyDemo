// Copyright Epic Games, Inc. All Rights Reserved.


#include "DerbyDemoPlayerController.h"
#include "DerbyDemoPawn.h"
#include "DerbyDemoGameState.h"
#include "DerbyDemoUI.h"
#include "DerbyCountdownUI.h"
#include "DerbyResultsUI.h"
#include "EnhancedInputSubsystems.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "DerbyDemo.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Widgets/Input/SVirtualJoystick.h"

void ADerbyDemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ensure we're attached to the vehicle pawn so that World Partition streaming works correctly
	bAttachToPawn = true;

	// Subscribe to round-state signals.
	if (ADerbyDemoGameState* GS = GetWorld()->GetGameState<ADerbyDemoGameState>())
	{
		GS->StartRoundDelegate.AddDynamic(this, &ADerbyDemoPlayerController::OnRoundStarted);
		GS->RoundEndDelegate.AddDynamic(this, &ADerbyDemoPlayerController::OnRoundEnded);
	}

	// only spawn UI on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogDerbyDemo, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}
		

		// spawn the UI widget and add it to the viewport
		VehicleUI = CreateWidget<UDerbyDemoUI>(this, VehicleUIClass);

		if (VehicleUI)
		{
			VehicleUI->AddToViewport();

		} else {

			UE_LOG(LogDerbyDemo, Error, TEXT("Could not spawn vehicle UI widget."));

		}

		// spawn the pre-round countdown widget (sits above the vehicle HUD)
		if (CountdownUIClass)
		{
			CountdownUI = CreateWidget<UDerbyCountdownUI>(this, CountdownUIClass);

			if (CountdownUI)
			{
				CountdownUI->AddToViewport(1);

			} else {

				UE_LOG(LogDerbyDemo, Error, TEXT("Could not spawn countdown UI widget."));

			}
		}
	}
}

void ADerbyDemoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void ADerbyDemoPlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	{
		VehicleUI->UpdateSpeed(VehiclePawn->GetChaosVehicleMovement()->GetForwardSpeed());
		VehicleUI->UpdateGear(VehiclePawn->GetChaosVehicleMovement()->GetCurrentGear());
	}
}

void ADerbyDemoPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	VehiclePawn = CastChecked<ADerbyDemoPawn>(InPawn);
	VehiclePawn->OnDestroyed.AddDynamic(this, &ADerbyDemoPlayerController::OnPawnDestroyed);
	VehiclePawn->OnVehicleEliminated.AddDynamic(this, &ADerbyDemoPlayerController::OnPawnEliminatedDelegate);

	// Lock input on the newly possessed pawn if the round hasn't started yet.
	// This also covers respawns: a replacement pawn starts locked if still in PreRound.
	if (ADerbyDemoGameState* GS = GetWorld()->GetGameState<ADerbyDemoGameState>())
	{
		if (GS->RoundPhase == ERoundPhase::PreRound)
		{
			InPawn->DisableInput(this);
		}
	}
}

void ADerbyDemoPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (ADerbyDemoGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADerbyDemoGameState>() : nullptr)
	{
		GS->StartRoundDelegate.RemoveDynamic(this, &ADerbyDemoPlayerController::OnRoundStarted);
		GS->RoundEndDelegate.RemoveDynamic(this, &ADerbyDemoPlayerController::OnRoundEnded);
	}
	Super::EndPlay(EndPlayReason);
}

void ADerbyDemoPlayerController::OnRoundStarted()
{
	if (APawn* P = GetPawn())
	{
		P->EnableInput(this);
	}
}

void ADerbyDemoPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	VehiclePawn = Cast<ADerbyDemoPawn>(P);
}

void ADerbyDemoPlayerController::OnPawnEliminatedDelegate(ADerbyDemoPawn* EliminatedPawn)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	// Lock the player out of driving
	if (APawn* P = GetPawn())
	{
		P->DisableInput(this);
	}

	// Spawn (or reuse) the results widget and notify it of the elimination
	if (ResultsUIClass && !ResultsUI)
	{
		ResultsUI = CreateWidget<UDerbyResultsUI>(this, ResultsUIClass);
		if (ResultsUI)
		{
			ResultsUI->AddToViewport(2);
		}
		else
		{
			UE_LOG(LogDerbyDemo, Error, TEXT("Could not spawn results UI widget."));
		}
	}

	if (ResultsUI)
	{
		ResultsUI->NotifyEliminated();
	}
}

void ADerbyDemoPlayerController::OnRoundEnded(AActor* Winner)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const bool bWon = (GetPawn() != nullptr && GetPawn() == Winner);

	// Spawn results widget if we haven't already (player was never eliminated)
	if (ResultsUIClass && !ResultsUI)
	{
		ResultsUI = CreateWidget<UDerbyResultsUI>(this, ResultsUIClass);
		if (ResultsUI)
		{
			ResultsUI->AddToViewport(2);
		}
		else
		{
			UE_LOG(LogDerbyDemo, Error, TEXT("Could not spawn results UI widget."));
		}
	}

	if (ResultsUI)
	{
		ResultsUI->NotifyRoundEnd(bWon);
	}

	// Disable input — the round is over for everyone
	if (APawn* P = GetPawn())
	{
		P->DisableInput(this);
	}
}

void ADerbyDemoPlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	// If the pawn was eliminated by the health system it was never Destroy()'d,
	// so this should not fire for that path. Guard anyway in case something else
	// destroys the pawn so we don't respawn mid-round or after it ends.
	if (const ADerbyDemoPawn* DV = Cast<ADerbyDemoPawn>(DestroyedPawn))
	{
		if (DV->bIsEliminated)
		{
			return;
		}
	}

	if (const ADerbyDemoGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADerbyDemoGameState>() : nullptr)
	{
		if (GS->RoundPhase == ERoundPhase::PostRound)
		{
			return;
		}
	}

	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// spawn a vehicle at the player start
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ADerbyDemoPawn* RespawnedVehicle = GetWorld()->SpawnActor<ADerbyDemoPawn>(VehiclePawnClass, SpawnTransform))
		{
			// possess the vehicle
			Possess(RespawnedVehicle);
		}
	}
}

bool ADerbyDemoPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
