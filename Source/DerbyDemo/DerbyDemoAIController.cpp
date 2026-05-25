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
	// Stuck detection — suppressed while already Reversing (avoid re-triggering
	// mid-maneuver) and while Ramming (wall braking can drop speed below the
	// threshold while the car is still actively pressing a target; RamTimeLimit
	// already handles the "can't make progress" escape → Fleeing).
	// NOT suppressed during Fleeing: if the car is genuinely wedged in a corner
	// during its breakaway it must still be able to trigger a rescue reverse.
	// -------------------------------------------------------------------------
	if (CurrentState != EAIState::Reversing
		&& CurrentState != EAIState::Ramming)
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
			const float WallAvoid    = ComputeWallAvoidanceSteering(WallDanger);
			// Decouple direction from blend weight: sign gives a decisive ±1 steer
			// recommendation; WallDanger (proximity) drives how strongly it overrides
			// the target-seek direction — prevents quadratic scale-down near walls.
			const float WallSteerDir = (FMath::Abs(WallAvoid) > KINDA_SMALL_NUMBER) ? FMath::Sign(WallAvoid) : 0.0f;
			const float BlendWeight  = FMath::Clamp(WallDanger * WhiskerAvoidanceStrength, 0.0f, 1.0f);
			VehiclePawn->DoSteering(FMath::Clamp(
				FMath::Lerp(ComputeSteering(ToCenterDir), WallSteerDir, BlendWeight), -1.0f, 1.0f));
			const float Throttle = FMath::Lerp(MinThrottle, 1.0f, HeadingFactor)
			                       * FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
			VehiclePawn->DoThrottle(Throttle);
			VehiclePawn->GetChaosVehicleMovement()->SetBrakeInput(FMath::Clamp(
				(WallDanger - WhiskerBrakeDangerThreshold) / FMath::Max(1.0f - WhiskerBrakeDangerThreshold, KINDA_SMALL_NUMBER),
				0.0f, 1.0f));
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
				const float WallAvoid    = ComputeWallAvoidanceSteering(WallDanger);
				const float WallSteerDir = (FMath::Abs(WallAvoid) > KINDA_SMALL_NUMBER) ? FMath::Sign(WallAvoid) : 0.0f;
				const float BlendWeight  = FMath::Clamp(WallDanger * WhiskerAvoidanceStrength, 0.0f, 1.0f);

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
						FMath::Lerp(ComputeSteering(AwayFromEnemy), WallSteerDir, BlendWeight), -1.0f, 1.0f));
					const float Throttle = FMath::Lerp(MinThrottle, 1.0f, HeadingFactor)
					                       * FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
					VehiclePawn->DoThrottle(Throttle);
				}
				// Active braking proportional to wall proximity (same formula as other states).
				VehiclePawn->GetChaosVehicleMovement()->SetBrakeInput(FMath::Clamp(
					(WallDanger - WhiskerBrakeDangerThreshold)
					/ FMath::Max(1.0f - WhiskerBrakeDangerThreshold, KINDA_SMALL_NUMBER),
					0.0f, 1.0f));
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
	// Wall avoidance blend — WallDanger (proximity) drives blend weight so even
	// small clearance asymmetries produce decisive corrections near walls.
	// Decoupling the weight from |WallAvoid| prevents the old quadratic scale-down
	// where a weak ClearanceBias produced a weak WallAvoid → weak SteerDanger →
	// almost no avoidance even when the wall was at bumper distance.
	// -------------------------------------------------------------------------
	const float AvoidScale = (CurrentState == EAIState::Ramming) ? WhiskerRammingAvoidanceScale : 1.0f;
	float WallDanger = 0.0f;
	const float WallAvoid    = ComputeWallAvoidanceSteering(WallDanger) * AvoidScale;
	// Direction: decisive ±1 signal from any clearance advantage; 0 when clear.
	const float WallSteerDir = (FMath::Abs(WallAvoid) > KINDA_SMALL_NUMBER) ? FMath::Sign(WallAvoid) : 0.0f;
	// Weight: wall proximity × user strength × Ramming scale (0.25 during Ramming to preserve charge).
	const float BlendWeight  = FMath::Clamp(WallDanger * WhiskerAvoidanceStrength * AvoidScale, 0.0f, 1.0f);

	VehiclePawn->DoSteering(
		FMath::Clamp(FMath::Lerp(DesiredSteering, WallSteerDir, BlendWeight), -1.0f, 1.0f));

	// Throttle uses raw WallDanger (peak proximity, pre-scale) so head-on walls
	// still slow the vehicle even when the clearance bias is near zero.
	// Ramming is exempt — don't kill the approach speed.
	if (CurrentState != EAIState::Ramming)
	{
		DesiredThrottle *= FMath::Lerp(1.0f, WhiskerMinThrottleOnWall, WallDanger);
	}
	VehiclePawn->DoThrottle(DesiredThrottle);

	// Apply active braking when danger exceeds the threshold; exempt Ramming so
	// the vehicle commits fully to its charge without the brakes fighting it.
	const float WallBrake = (CurrentState != EAIState::Ramming)
		? FMath::Clamp((WallDanger - WhiskerBrakeDangerThreshold) / FMath::Max(1.0f - WhiskerBrakeDangerThreshold, KINDA_SMALL_NUMBER), 0.0f, 1.0f)
		: 0.0f;
	VehiclePawn->GetChaosVehicleMovement()->SetBrakeInput(WallBrake);
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
