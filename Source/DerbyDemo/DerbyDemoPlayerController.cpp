// Copyright Epic Games, Inc. All Rights Reserved.


#include "DerbyDemoPlayerController.h"
#include "DerbyDemoPawn.h"
#include "DerbyDemoUI.h"
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
}

void ADerbyDemoPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	VehiclePawn = Cast<ADerbyDemoPawn>(P);
}

void ADerbyDemoPlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
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
