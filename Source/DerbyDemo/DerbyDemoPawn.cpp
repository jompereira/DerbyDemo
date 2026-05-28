// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyDemoPawn.h"
#include "DerbyDemoWheelFront.h"
#include "DerbyDemoWheelRear.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "DerbyDemo.h"
#include "TimerManager.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

#define LOCTEXT_NAMESPACE "VehiclePawn"

ADerbyDemoPawn::ADerbyDemoPawn()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

}

void ADerbyDemoPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ADerbyDemoPawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ADerbyDemoPawn::Throttle);

		// break 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ADerbyDemoPawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ADerbyDemoPawn::StopBrake);

		// handbrake 
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ADerbyDemoPawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ADerbyDemoPawn::StopHandbrake);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::ToggleCamera);

		// reset the vehicle 
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &ADerbyDemoPawn::ResetVehicle);
	}
	else
	{
		UE_LOG(LogDerbyDemo, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADerbyDemoPawn::BeginPlay()
{
	Super::BeginPlay();

	// Initialise health
	CurrentHealth = MaxHealth;

	// set up the flipped check timer
	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &ADerbyDemoPawn::FlippedCheck, FlipCheckTime, true);

	// prevent geometry collection pieces from colliding with this vehicle's skeletal mesh
	for (UGeometryCollectionComponent* GC : TInlineComponentArray<UGeometryCollectionComponent*>(this))
	{
		GC->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	}

}

void ADerbyDemoPawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the flipped check timer
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	Super::EndPlay(EndPlayReason);
}

void ADerbyDemoPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// add some angular damping if the vehicle is in midair
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);

	// realign the camera yaw to face front
	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
}

void ADerbyDemoPawn::Steering(const FInputActionValue& Value)
{
	// route the input
	DoSteering(Value.Get<float>());
}

void ADerbyDemoPawn::Throttle(const FInputActionValue& Value)
{
	// route the input
	DoThrottle(Value.Get<float>());
}

void ADerbyDemoPawn::Brake(const FInputActionValue& Value)
{
	// route the input
	DoBrake(Value.Get<float>());
}

void ADerbyDemoPawn::StartBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStart();
}

void ADerbyDemoPawn::StopBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStop();
}

void ADerbyDemoPawn::StartHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStart();
}

void ADerbyDemoPawn::StopHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStop();
}

void ADerbyDemoPawn::LookAround(const FInputActionValue& Value)
{
	// route the input
	DoLookAround(Value.Get<float>());
}

void ADerbyDemoPawn::ToggleCamera(const FInputActionValue& Value)
{
	// route the input
	DoToggleCamera();
}

void ADerbyDemoPawn::ResetVehicle(const FInputActionValue& Value)
{
	// route the input
	DoResetVehicle();
}

void ADerbyDemoPawn::DoSteering(float SteeringValue)
{
	// add the input
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);
}

void ADerbyDemoPawn::DoThrottle(float ThrottleValue)
{
	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);

	// reset the brake input
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ADerbyDemoPawn::DoBrake(float BrakeValue)
{
	// add the input
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);

	// reset the throttle input
	ChaosVehicleMovement->SetThrottleInput(0.0f);
}

void ADerbyDemoPawn::DoBrakeStart()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(true);
}

void ADerbyDemoPawn::DoBrakeStop()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ADerbyDemoPawn::DoHandbrakeStart()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);

	// call the Blueprint hook for the break lights
	BrakeLights(true);
}

void ADerbyDemoPawn::DoHandbrakeStop()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// call the Blueprint hook for the break lights
	BrakeLights(false);
}

void ADerbyDemoPawn::DoLookAround(float YawDelta)
{
	// rotate the spring arm
	BackSpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void ADerbyDemoPawn::DoToggleCamera()
{
	// toggle the active camera flag
	bFrontCameraActive = !bFrontCameraActive;

	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void ADerbyDemoPawn::DoResetVehicle()
{
	// reset to a location slightly above our current one
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	// reset to our yaw. Ignore pitch and roll
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	// teleport the actor to the reset spot and reset physics
	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
}



void ADerbyDemoPawn::TakeDerbyDamage(float Amount)
{
	if (bIsEliminated || Amount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Amount);

	if (CurrentHealth <= 0.f)
	{
		bIsEliminated = true;

		// Stop the vehicle immediately — brake hard and hold the handbrake.
		ChaosVehicleMovement->SetThrottleInput(0.f);
		ChaosVehicleMovement->SetBrakeInput(0.f);
		ChaosVehicleMovement->SetHandbrakeInput(true);
		ChaosVehicleMovement->SetSteeringInput(0.f);

		// Cancel the flip-check timer so an eliminated wreck doesn't self-reset.
		GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

		// Blueprint hook for VFX / SFX (explosion, smoke, etc.).
		OnEliminated();

		// Notify game mode and controllers.
		OnVehicleEliminated.Broadcast(this);

		UE_LOG(LogDerbyDemo, Log, TEXT("%s eliminated."), *GetNameSafe(this));
	}
}

float ADerbyDemoPawn::GetHealthRatio() const
{
	return MaxHealth > 0.f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f;
}

void ADerbyDemoPawn::DoCameraShake(const FVector& NormalImpulse)
{
	if (!CollisionCameraShake)
	{
		return;
	}

	APlayerController* PC = GetController<APlayerController>();
	if (!PC)
	{
		return;
	}

	const float ImpulseMag = NormalImpulse.Size();

	// Only shake if the impulse exceeds the minimum threshold.
	// Scale linearly from 0 at CollisionShakeMinImpulse to 1 at CollisionShakeMaxImpulse.
	if (ImpulseMag >= CollisionShakeMinImpulse)
	{
		const float Scale = FMath::Clamp(
			(ImpulseMag - CollisionShakeMinImpulse) / (CollisionShakeMaxImpulse - CollisionShakeMinImpulse),
			0.0f, 1.0f);
		if (Scale > 0.0f)
		{
			PC->ClientStartCameraShake(CollisionCameraShake, Scale);
		}
	}
}

void ADerbyDemoPawn::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// The Chaos contact point lands on the convex hull (usually a corner).
	// Trace back along the hit normal against the actual mesh body to find the
	// true surface intersection, giving accurate panel-level hit location.
	FVector RefinedLocation = HitLocation;
	FHitResult SurfaceHit;
	const FVector TraceStart = HitLocation + HitNormal * 50.f;
	const FVector TraceEnd   = HitLocation - HitNormal * 50.f;
	if (MyComp->LineTraceComponent(SurfaceHit, TraceStart, TraceEnd, FCollisionQueryParams::DefaultQueryParam))
	{
		RefinedLocation = SurfaceHit.ImpactPoint;
	}
	
	// DrawDebugSphere(GetWorld(), HitLocation, 16.f, 16, FColor::Blue, false, 10.0f);
	// DrawDebugSphere(GetWorld(), RefinedLocation, 16.f, 16, FColor::Red, false, 10.0f);
	

	const float ImpulseMag = NormalImpulse.Size();
	DoCameraShake(NormalImpulse);
	OnImpact(RefinedLocation, HitNormal, ImpulseMag);
}

void ADerbyDemoPawn::FlippedCheck()
{
	// check the difference in angle between the mesh's up vector and world up
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		
		// is this the second time we've checked that the vehicle is still flipped?
		if (bPreviousFlipCheck)
		{
			// reset the vehicle to upright
			DoResetVehicle();
		}
		
		// set the flipped check flag so the next check resets the car
		bPreviousFlipCheck = true;

	} else {

		// we're upright. reset the flipped check flag
		bPreviousFlipCheck = false;
	}
}

void ADerbyDemoPawn::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	if (!DebugDisplay.IsDisplayOn(TEXT("Vehicle")))
	{
		return;
	}

	FDisplayDebugManager& DDM = Canvas->DisplayDebugManager;
	DDM.SetFont(GEngine->GetSmallFont());
	DDM.SetDrawColor(FColor::Cyan);
	DDM.DrawString(TEXT("=== DerbyDemo Wheel Slip ==="));

	const int32 NumWheels = ChaosVehicleMovement->GetNumWheels();
	for (int32 i = 0; i < NumWheels; i++)
	{
		const FWheelStatus& S = ChaosVehicleMovement->GetWheelState(i);

		// Sample LateralSlipGraph at the current slip angle to show where on the curve we are
		float CurveFriction = 0.f;
		if (ChaosVehicleMovement->Wheels.IsValidIndex(i))
		{
			CurveFriction = ChaosVehicleMovement->Wheels[i]->LateralSlipGraph.GetRichCurveConst()->Eval(S.SlipAngle);
		}

		// Red = skidding, Yellow = slipping, White = nominal
		DDM.SetDrawColor(S.bIsSkidding ? FColor::Red : (S.bIsSlipping ? FColor::Yellow : FColor::White));

		DDM.DrawString(FString::Printf(
			TEXT("[W%d] SlipAngle: %6.2f deg  CurveFriction: %.3f  SlipMag: %5.2f  SkidMag: %5.2f  Spring: %7.1f N  Drive: %7.1f Nm"),
			i,
			S.SlipAngle,
			CurveFriction,
			S.SlipMagnitude,
			S.SkidMagnitude,
			S.SpringForce,
			S.DriveTorque
		));
	}
}

#undef LOCTEXT_NAMESPACE