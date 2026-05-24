#include "DerbyDemoAIController.h"
#include "DerbyDemoPawn.h"
#include "DerbyDemo.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

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
			float WallDanger = 0.0f;
			const float WallAvoid   = ComputeWallAvoidanceSteering(WallDanger);
			const float SteerDanger = FMath::Abs(WallAvoid);
			VehiclePawn->DoSteering(FMath::Clamp(
				FMath::Lerp(ComputeSteering(ToCenterDir), WallAvoid, SteerDanger), -1.0f, 1.0f));
			const float Throttle = FMath::Lerp(MinThrottle, 1.0f, HeadingFactor)
			                       * FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
			VehiclePawn->DoThrottle(Throttle);
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
				float WallDanger = 0.0f;
				const float WallAvoid   = ComputeWallAvoidanceSteering(WallDanger);
				const float SteerDanger = FMath::Abs(WallAvoid);

				const float DistToEnemy = FVector::Dist(MyLocation, NearestEnemy->GetActorLocation());

				if (DistToEnemy > FleeWallOnlyDistance)
				{
					// Enemy is far — the flee vector is irrelevant and often points
					// straight into a wall. Navigate purely by wall avoidance so the
					// vehicle finds clear space; full throttle when nothing is nearby.
					VehiclePawn->DoSteering(WallAvoid);
					VehiclePawn->DoThrottle(FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger));
				}
				else
				{
					// Enemy is close — blend flee direction with wall avoidance.
					const FVector AwayFromEnemy = (MyLocation - NearestEnemy->GetActorLocation()).GetSafeNormal();
					const float   HeadingFactor = FMath::Clamp(
						FVector::DotProduct(VehiclePawn->GetActorForwardVector(), AwayFromEnemy),
						0.0f, 1.0f);
					VehiclePawn->DoSteering(FMath::Clamp(
						FMath::Lerp(ComputeSteering(AwayFromEnemy), WallAvoid, SteerDanger), -1.0f, 1.0f));
					const float Throttle = FMath::Lerp(MinThrottle, 1.0f, HeadingFactor)
					                       * FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
					VehiclePawn->DoThrottle(Throttle);
				}
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
	// Per-state behavior — desired steering and throttle stored as locals so
	// wall avoidance can be blended in one place after the switch.
	// -------------------------------------------------------------------------
	float DesiredSteering = 0.0f;
	float DesiredThrottle = 0.0f;

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
		DesiredSteering = ComputeSteering(ToAim);
		DesiredThrottle = FMath::Lerp(MinThrottle, 1.0f, HeadingFactor);
		break;
	}

	case EAIState::Ramming:
		// No prediction — aim at exactly where the target is and commit.
		DesiredSteering = ComputeSteering(ActualToTarget);
		DesiredThrottle = 1.0f;
		break;

	case EAIState::UTurning:
	{
		// Pick the shorter turn direction based on which side the target is on.
		const float RightDot = FVector::DotProduct(VehiclePawn->GetActorRightVector(), ActualToTarget);
		DesiredSteering = (RightDot >= 0.0f) ? 1.0f : -1.0f;
		DesiredThrottle = UTurnThrottle;
		break;
	}

	default:
		break;
	}

	// -------------------------------------------------------------------------
	// Wall avoidance blend — priority lerp overrides target steering as danger
	// increases, so opposing target+wall forces can no longer cancel each other.
	// -------------------------------------------------------------------------
	const float AvoidScale = (CurrentState == EAIState::Ramming) ? WhiskerRammingAvoidanceScale : 1.0f;
	float WallDanger = 0.0f;
	const float WallAvoid   = ComputeWallAvoidanceSteering(WallDanger) * AvoidScale;
	const float SteerDanger = FMath::Abs(WallAvoid);  // post-scale, drives the lerp

	// Priority lerp: SteerDanger=0 → pure target steering; SteerDanger=1 → pure avoidance.
	VehiclePawn->DoSteering(
		FMath::Clamp(FMath::Lerp(DesiredSteering, WallAvoid, SteerDanger), -1.0f, 1.0f));

	// Throttle uses raw WallDanger (peak proximity, pre-scale) so head-on walls
	// still slow the vehicle even when the clearance bias is near zero.
	// Ramming is exempt — don't kill the approach speed.
	if (CurrentState != EAIState::Ramming)
	{
		DesiredThrottle *= FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
	}
	VehiclePawn->DoThrottle(DesiredThrottle);
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
	if (NewState == EAIState::Fleeing)   FleeTimeRemaining     = FleeTime;

	// Only reset the seek timer when returning from a genuine break state.
	// Cycling back from a missed ram or UTurn must keep the clock running so
	// SeekTimeLimit can fire and force a detour — without this the
	// Seek → Ram (miss) → UTurn → Seek loop runs indefinitely.
	if (NewState == EAIState::Seeking)
	{
		const bool bFromBreakState = (CurrentState == EAIState::Fleeing      ||
		                              CurrentState == EAIState::Reversing    ||
		                              CurrentState == EAIState::StartingRound);
		if (bFromBreakState)
		{
			SeekTimeElapsed = 0.0f;
		}
	}

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

// -----------------------------------------------------------------------------

float ADerbyDemoAIController::ComputeWallAvoidanceSteering(float& OutWallDanger) const
{
	OutWallDanger = 0.0f;

	if (!VehiclePawn) return 0.0f;

	UWorld* World = GetWorld();
	if (!World) return 0.0f;

	// Project forward and right onto the world XY plane so whiskers always fire
	// horizontally regardless of vehicle pitch.
	FVector Forward = VehiclePawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize()) { Forward = FVector::ForwardVector; }

	FVector Right = VehiclePawn->GetActorRightVector();
	Right.Z = 0.0f;
	if (!Right.Normalize()) { Right = FVector::RightVector; }

	const FVector Origin = VehiclePawn->GetActorLocation()
	                       + Forward * WhiskerOriginForwardOffset;

	const float SideRad    = FMath::DegreesToRadians(WhiskerSideAngleDeg);
	const float CosSide    = FMath::Cos(SideRad);
	const float SinSide    = FMath::Sin(SideRad);

	const float FarSideRad = FMath::DegreesToRadians(WhiskerFarSideAngleDeg);
	const float CosFarSide = FMath::Cos(FarSideRad);
	const float SinFarSide = FMath::Sin(FarSideRad);

	// [0] center  [1] inner-left  [2] inner-right  [3] outer-left  [4] outer-right
	const FVector Dirs[5] =
	{
		Forward,
		Forward * CosSide    - Right * SinSide,
		Forward * CosSide    + Right * SinSide,
		Forward * CosFarSide - Right * SinFarSide,
		Forward * CosFarSide + Right * SinFarSide,
	};

	const float SpeedExtension = VehiclePawn->GetVelocity().Size() * WhiskerLookAheadTime;
	const float Lengths[5] =
	{
		WhiskerCenterLength  + SpeedExtension,
		WhiskerSideLength    + SpeedExtension,
		WhiskerSideLength    + SpeedExtension,
		WhiskerFarSideLength + SpeedExtension,
		WhiskerFarSideLength + SpeedExtension,
	};

	FCollisionQueryParams QueryParams(TEXT("WhiskerTrace"), false, VehiclePawn);

	// -----------------------------------------------------------------------
	// Trace all whiskers and record clearance distances.
	// Clearance = distance to the nearest valid wall hit, or full length if clear.
	// -----------------------------------------------------------------------
	float Clearance[5];
	float MaxProximity = 0.0f;

	for (int32 i = 0; i < 5; ++i)
	{
		const FVector End = Origin + Dirs[i] * Lengths[i];
		FHitResult Hit;
		bool bValidHit = World->LineTraceSingleByChannel(Hit, Origin, End, ECC_WorldStatic, QueryParams);

		// Reject terrain: walls have near-horizontal normals, landscape is near-vertical.
		if (bValidHit && FMath::Abs(Hit.ImpactNormal.Z) > WhiskerMaxTerrainNormalZ)
		{
			bValidHit = false;
		}

		if (bDebugDrawWhiskers)
		{
			DrawDebugLine(World, Origin, bValidHit ? Hit.ImpactPoint : End,
				bValidHit ? FColor::Red : FColor::Green,
				false, -1.0f, 0, 3.0f);
		}

		Clearance[i] = bValidHit ? Hit.Distance : Lengths[i];

		if (bValidHit)
		{
			const float Proximity = 1.0f - (Hit.Distance / Lengths[i]);
			MaxProximity = FMath::Max(MaxProximity, Proximity);
		}
	}

	// OutWallDanger = peak proximity across ALL whiskers, used for throttle
	// reduction even when there is no clear lateral avoidance direction.
	OutWallDanger = MaxProximity;

	if (MaxProximity <= 0.0f)
	{
		return 0.0f;
	}

	// -----------------------------------------------------------------------
	// Clearance comparison — steer toward whichever side has more free space.
	//
	// The old normal-based approach failed for near-perpendicular walls because
	// DotProduct(Right, WallNormal) ≈ 0, producing near-zero correction for the
	// most dangerous (head-on) approach angle.
	//
	// Comparing clearances is robust at every angle: a wall that blocks the left
	// whiskers and not the right ones always produces a rightward correction,
	// regardless of what the wall's normal direction happens to be.
	//
	// Inner whiskers (±WhiskerSideAngleDeg) are weighted 2× over outer whiskers
	// because they cover the zone directly ahead where walls are most dangerous.
	// -----------------------------------------------------------------------
	const float InnerW = 2.0f;
	const float OuterW = 1.0f;

	const float LeftClearance     = Clearance[1] * InnerW + Clearance[3] * OuterW;
	const float RightClearance    = Clearance[2] * InnerW + Clearance[4] * OuterW;
	const float MaxLeftClearance  = Lengths[1]   * InnerW + Lengths[3]   * OuterW;
	const float MaxRightClearance = Lengths[2]   * InnerW + Lengths[4]   * OuterW;

	// Normalize to [0, 1] so absolute length differences don't skew the result.
	const float NormLeft  = LeftClearance  / MaxLeftClearance;
	const float NormRight = RightClearance / MaxRightClearance;

	// Positive → steer right (more space right), negative → steer left (more space left).
	// Scale by MaxProximity so the correction is proportional to actual danger.
	const float ClearanceBias = NormRight - NormLeft;
	return FMath::Clamp(ClearanceBias * WhiskerAvoidanceStrength * MaxProximity, -1.0f, 1.0f);
}

// -----------------------------------------------------------------------------

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
