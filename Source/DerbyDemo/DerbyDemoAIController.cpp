#include "DerbyDemoAIController.h"
#include "DerbyDemoPawn.h"
#include "DerbyDemo.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ADerbyDemoAIController::ADerbyDemoAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADerbyDemoAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	VehiclePawn = Cast<ADerbyDemoPawn>(InPawn);
}

void ADerbyDemoAIController::OnUnPossess()
{
	Super::OnUnPossess();
	VehiclePawn = nullptr;
}

void ADerbyDemoAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || !VehiclePawn)
	{
		return;
	}

	// -------------------------------------------------------------------------
	// Stuck detection — suppressed while Ramming (we're actively hitting something),
	// while already Reversing (avoid re-triggering mid-maneuver), and while Fleeing
	// (the vehicle intentionally slows during its breakaway turn).
	// -------------------------------------------------------------------------
	if (CurrentState != EAIState::Reversing)
	{
		if (VehiclePawn->GetVelocity().Size() < StuckSpeedThreshold)
		{
			TimeAtLowSpeed += DeltaTime;
		}
		else
		{
			TimeAtLowSpeed = 0.0f;
		}

		if (TimeAtLowSpeed >= StuckTime)
		{
			TimeAtLowSpeed = 0.0f;
			TransitionToState(EAIState::Reversing);
		}
	}

	// -------------------------------------------------------------------------
	// Reversing state — handled separately; no target needed.
	// -------------------------------------------------------------------------
	if (CurrentState == EAIState::Reversing)
	{
		VehiclePawn->DoSteering(0.0f);
		UChaosWheeledVehicleMovementComponent* MoveComp = VehiclePawn->GetChaosVehicleMovement();
		MoveComp->SetThrottleInput(0.0f);
		MoveComp->SetBrakeInput(1.0f);

		ReverseTimeRemaining -= DeltaTime;
		if (ReverseTimeRemaining <= 0.0f)
		{
			MoveComp->SetBrakeInput(0.0f);
			TransitionToState(EAIState::Seeking);
		}
		return;
	}

	// -------------------------------------------------------------------------
	// StartingRound state — drive to arena centre before engaging; no target needed.
	// -------------------------------------------------------------------------
	if (CurrentState == EAIState::StartingRound)
	{
		const FVector MyLocation  = VehiclePawn->GetActorLocation();
		const FVector ToCenter    = ArenaCenter - MyLocation;
		const float   DistToCenter = ToCenter.Size();

		if (DistToCenter <= StartingRoundRadius)
		{
			// Arrived — begin engaging enemies.
			TransitionToState(EAIState::Seeking);
			// Fall through to normal target-seeking behavior this tick.
		}
		else
		{
			const FVector ToCenterDir  = ToCenter.GetSafeNormal();
			const float   HeadingFactor = FMath::Clamp(
				FVector::DotProduct(VehiclePawn->GetActorForwardVector(), ToCenterDir),
				0.0f, 1.0f);
			VehiclePawn->DoSteering(ComputeSteering(ToCenterDir));
			VehiclePawn->DoThrottle(FMath::Lerp(MinThrottle, 1.0f, HeadingFactor));
			return;
		}
	}

	// -------------------------------------------------------------------------
	// Fleeing state — drive away from the nearest enemy; no lock-on needed.
	// -------------------------------------------------------------------------
	if (CurrentState == EAIState::Fleeing)
	{
		FleeTimeRemaining -= DeltaTime;
		if (FleeTimeRemaining <= 0.0f)
		{
			// Flee complete — re-engage.
			TransitionToState(EAIState::Seeking);
			// Fall through to normal target-seeking behavior this tick.
		}
		else
		{
			const FVector MyLocation     = VehiclePawn->GetActorLocation();
			ADerbyDemoPawn* NearestEnemy = FindNearestEnemy();

			if (NearestEnemy)
			{
				// Steer directly away; use the same heading-factor throttle as Seeking
				// so the vehicle accelerates once it has turned to face the exit direction.
				const FVector AwayFromEnemy = (MyLocation - NearestEnemy->GetActorLocation()).GetSafeNormal();
				const float   HeadingFactor = FMath::Clamp(
					FVector::DotProduct(VehiclePawn->GetActorForwardVector(), AwayFromEnemy),
					0.0f, 1.0f);
				VehiclePawn->DoSteering(ComputeSteering(AwayFromEnemy));
				VehiclePawn->DoThrottle(FMath::Lerp(MinThrottle, 1.0f, HeadingFactor));
			}
			else
			{
				// Nothing to flee from — transition immediately.
				TransitionToState(EAIState::Seeking);
			}
			return;
		}
	}

	// -------------------------------------------------------------------------
	// Target acquisition
	// -------------------------------------------------------------------------
	ADerbyDemoPawn* Target = FindNearestEnemy();
	if (!Target)
	{
		VehiclePawn->DoThrottle(0.0f);
		VehiclePawn->DoSteering(0.0f);
		return;
	}

	// Shared geometry computed once and reused by transitions and behavior below.
	const FVector MyLocation     = VehiclePawn->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float   DistToTarget   = FVector::Dist(MyLocation, TargetLocation);

	// ActualToTarget is always based on the real position — never the predicted one.
	// ForwardDot must reflect where the target actually is so U-turn detection cannot
	// be fooled by a chasing target whose predicted position projects ahead of us.
	const FVector ActualToTarget = (TargetLocation - MyLocation).GetSafeNormal();
	const float   ForwardDot     = FVector::DotProduct(VehiclePawn->GetActorForwardVector(), ActualToTarget);

	// -------------------------------------------------------------------------
	// State transitions
	// -------------------------------------------------------------------------
	switch (CurrentState)
	{
	case EAIState::Seeking:
		SeekTimeElapsed += DeltaTime;
		if (DistToTarget < RamDistance)
			TransitionToState(EAIState::Ramming);
		else if (SeekTimeElapsed >= SeekTimeLimit)
			TransitionToState(EAIState::Fleeing);    // chased too long without contact — take a breath
		else if (ForwardDot < 0.0f)
			TransitionToState(EAIState::UTurning);
		break;

	case EAIState::Ramming:
		RamTimeElapsed += DeltaTime;
		if (RamTimeElapsed >= RamTimeLimit)
			TransitionToState(EAIState::Fleeing);    // been ramming too long — break off
		else if (ForwardDot < 0.0f)
			TransitionToState(EAIState::UTurning);   // overshot — turn around
		else if (DistToTarget >= RamDistance)
			TransitionToState(EAIState::Seeking);    // target escaped
		break;

	case EAIState::UTurning:
		if (ForwardDot >= 0.0f)
			TransitionToState(EAIState::Seeking);    // turned around successfully
		break;

	default:
		break;
	}

	// -------------------------------------------------------------------------
	// Per-state behavior
	// -------------------------------------------------------------------------
	switch (CurrentState)
	{
	case EAIState::Seeking:
	{
		// Lead the aim point by a small amount proportional to how fast the target
		// is moving; stopped targets get zero lead so the AI aims exactly at them.
		const float PredictionTime = FMath::Clamp(Target->GetVelocity().Size() / 3000.0f, 0.0f, MaxPredictionTime);
		const FVector AimLocation  = TargetLocation + Target->GetVelocity() * PredictionTime;
		const FVector ToAim        = (AimLocation - MyLocation).GetSafeNormal();

		const float HeadingFactor = FMath::Clamp(ForwardDot, 0.0f, 1.0f);
		VehiclePawn->DoSteering(ComputeSteering(ToAim));
		VehiclePawn->DoThrottle(FMath::Lerp(MinThrottle, 1.0f, HeadingFactor));
		break;
	}

	case EAIState::Ramming:
		// No prediction — aim at exactly where the target is and commit.
		VehiclePawn->DoSteering(ComputeSteering(ActualToTarget));
		VehiclePawn->DoThrottle(1.0f);
		break;

	case EAIState::UTurning:
	{
		// Pick the shorter turn direction based on which side the target is on.
		const float RightDot = FVector::DotProduct(VehiclePawn->GetActorRightVector(), ActualToTarget);
		VehiclePawn->DoSteering((RightDot >= 0.0f) ? 1.0f : -1.0f);
		VehiclePawn->DoThrottle(UTurnThrottle);
		break;
	}

	default:
		break;
	}
}

// -----------------------------------------------------------------------------

void ADerbyDemoAIController::TransitionToState(EAIState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	UE_LOG(LogDerbyDemo, Log, TEXT("AI %s: %s -> %s"),
		*GetName(),
		*UEnum::GetValueAsString(CurrentState),
		*UEnum::GetValueAsString(NewState));

	if (NewState == EAIState::Reversing) ReverseTimeRemaining = ReverseTime;
	if (NewState == EAIState::Ramming)   RamTimeElapsed        = 0.0f;
	if (NewState == EAIState::Seeking)   SeekTimeElapsed       = 0.0f;
	if (NewState == EAIState::Fleeing)   FleeTimeRemaining     = FleeTime;

	CurrentState = NewState;
}

float ADerbyDemoAIController::ComputeSteering(FVector ToAim) const
{
	const float RightComp = FVector::DotProduct(VehiclePawn->GetActorRightVector(), ToAim);
	const float FwdComp   = FVector::DotProduct(VehiclePawn->GetActorForwardVector(), ToAim);
	return FMath::Clamp(
		FMath::Atan2(RightComp, FwdComp) / FMath::DegreesToRadians(FullLockAngleDeg),
		-1.0f, 1.0f);
}

ADerbyDemoPawn* ADerbyDemoAIController::FindNearestEnemy() const
{
	TArray<AActor*> AllPawns;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADerbyDemoPawn::StaticClass(), AllPawns);

	ADerbyDemoPawn* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	const FVector MyLocation = VehiclePawn->GetActorLocation();

	for (AActor* Actor : AllPawns)
	{
		ADerbyDemoPawn* Other = Cast<ADerbyDemoPawn>(Actor);
		if (!Other || Other == VehiclePawn)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Other;
		}
	}

	return Nearest;
}
