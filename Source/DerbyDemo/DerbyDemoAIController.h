#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DerbyDemoAIController.generated.h"

class ADerbyDemoPawn;

UENUM(BlueprintType)
enum class EAIState : uint8
{
	PreStartRound UMETA(DisplayName="Pre Start Round"),
	StartingRound UMETA(DisplayName="Starting Round"),
	Seeking       UMETA(DisplayName="Seeking"),
	Ramming       UMETA(DisplayName="Ramming"),
	Fleeing       UMETA(DisplayName="Fleeing"),
	UTurning      UMETA(DisplayName="U-Turning"),
	Reversing     UMETA(DisplayName="Reversing"),
};

UCLASS()
class DERBYDEMO_API ADerbyDemoAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADerbyDemoAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

protected:
	/** Speed below which the car is considered stuck (cm/s) */
	UPROPERTY(EditAnywhere, Category="AI|Stuck Detection")
	float StuckSpeedThreshold = 50.0f;

	/** Time at low speed before triggering a reverse maneuver (seconds) */
	UPROPERTY(EditAnywhere, Category="AI|Stuck Detection")
	float StuckTime = 3.0f;

	/** Duration of the reverse maneuver (seconds) */
	UPROPERTY(EditAnywhere, Category="AI|Stuck Detection")
	float ReverseTime = 2.0f;

	/** Angle at which full steering lock is applied; smaller = more aggressive correction */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="1.0", ClampMax="90.0"))
	float FullLockAngleDeg = 20.0f;

	/** Minimum throttle when turning hard toward a target */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinThrottle = 0.35f;

	/** Throttle during a U-turn; keep low for a tight turning radius */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="0.0", ClampMax="1.0"))
	float UTurnThrottle = 0.1f;

	/** Distance (cm) at which the AI transitions from Seeking to Ramming */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="0.0"))
	float RamDistance = 700.0f;

	/** Max seconds ahead to predict a moving target's position */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="0.0"))
	float MaxPredictionTime = 0.3f;

	/** Maximum time (seconds) the AI continuously rams before breaking off.
	 *  Prevents vehicles gluing to one another indefinitely. */
	UPROPERTY(EditAnywhere, Category="AI|Ramming", meta=(ClampMin="0.0"))
	float RamTimeLimit = 2.0f;

	/** Maximum time (seconds) the AI spends Seeking before giving up and fleeing.
	 *  Breaks endless pursuit loops and gives the arena room to breathe. */
	UPROPERTY(EditAnywhere, Category="AI|Seek", meta=(ClampMin="0.0"))
	float SeekTimeLimit = 6.0f;

	/** How long (seconds) the AI spends in the Fleeing state before re-engaging. */
	UPROPERTY(EditAnywhere, Category="AI|Fleeing", meta=(ClampMin="0.0"))
	float FleeTime = 3.0f;

	/** Distance (cm) beyond which the fleeing AI ignores the enemy direction and
	 *  navigates using wall avoidance only. Prevents the flee vector from pointing
	 *  straight into a wall when the enemy is far away and poses no immediate threat. */
	UPROPERTY(EditAnywhere, Category="AI|Fleeing", meta=(ClampMin="0.0"))
	float FleeWallOnlyDistance = 1500.0f;

	/** World-space position of the arena centre; AI drives here at round start before engaging */
	UPROPERTY(EditAnywhere, Category="AI|Starting Round")
	FVector ArenaCenter = FVector::ZeroVector;

	/** Radius (cm) within which the AI considers itself to have reached the arena centre */
	UPROPERTY(EditAnywhere, Category="AI|Starting Round", meta=(ClampMin="0.0"))
	FVector2D StartingRoundRadius = FVector2D(500.f, 1000.f);

	// -------------------------------------------------------------------------
	// Wall Avoidance — whisker line traces
	// -------------------------------------------------------------------------

	/** Length (cm) of the center whisker fired straight ahead */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0"))
	float WhiskerCenterLength = 500.0f;

	/** Length (cm) of each angled side whisker */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0"))
	float WhiskerSideLength = 350.0f;

	/** Angle (degrees) the inner side whiskers diverge from the vehicle's forward vector */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="90.0"))
	float WhiskerSideAngleDeg = 45.0f;

	/** Angle (degrees) of the outer whisker pair — wider for detecting near-perpendicular walls */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="90.0"))
	float WhiskerFarSideAngleDeg = 75.0f;

	/** Length (cm) of the outer whiskers — shorter since they cover close-range flanks */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0"))
	float WhiskerFarSideLength = 200.0f;

	/** Forward offset (cm) from the actor origin to the whisker start point — place near the front bumper */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0"))
	float WhiskerOriginForwardOffset = 150.0f;

	/** Maximum allowed Z component of a hit surface's normal to be treated as a wall.
	 *  Walls have near-horizontal normals (Z ≈ 0); terrain and slopes have near-vertical
	 *  normals (Z ≈ 1). Hits above this threshold are silently ignored.
	 *  0.7 ≈ 45° — surfaces steeper than that are walls; shallower surfaces are terrain. */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WhiskerMaxTerrainNormalZ = 0.7f;

	/** Look-ahead time (seconds) added to all whisker lengths based on current speed.
	 *  EffectiveLength = BaseLength + Speed * WhiskerLookAheadTime.
	 *  At 0.3 s and 2000 cm/s the whiskers extend an extra 600 cm beyond their base lengths. */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0"))
	float WhiskerLookAheadTime = 0.3f;

	/** Avoidance correction multiplier applied to all whisker hits; 0 = disabled, 1 = normal, 2 = aggressive */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="5.0"))
	float WhiskerAvoidanceStrength = 1.0f;

	/** Scales wall avoidance during Ramming so the vehicle commits to the target
	 *  rather than veering off. 0 = full commitment (no avoidance), 1 = normal strength.
	 *  Keep low — the target geometry usually overrides any wall concern at ram distance. */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WhiskerRammingAvoidanceScale = 0.25f;

	/** Throttle multiplier applied when wall danger is at maximum (proximity = 1).
	 *  0 = full stop, 0.3 = 30 % throttle, 1 = no change. Not applied during Ramming. */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WhiskerMinThrottleOnWall = 0.3f;

	/** Wall danger (0–1) above which the AI actively brakes in addition to reducing throttle.
	 *  Brake intensity ramps linearly from 0 at this threshold to 1 at full proximity (WallDanger=1).
	 *  Not applied during Ramming so the vehicle commits fully to its charge. */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WhiskerBrakeDangerThreshold = 0.75f;

	/** Draw green/red whisker debug lines in the viewport during PIE for tuning */
	UPROPERTY(EditAnywhere, Category="AI|Wall Avoidance")
	bool bDebugDrawWhiskers = false;

private:
	UPROPERTY()
	TObjectPtr<ADerbyDemoPawn> VehiclePawn;

	/** Current state — visible in the Details panel during PIE for easy debugging */
	UPROPERTY(VisibleInstanceOnly, Category="AI|Debug")
	EAIState CurrentState = EAIState::PreStartRound;

	float TimeAtLowSpeed = 0.0f;
	float ReverseTimeRemaining = 0.0f;
	float RamTimeElapsed = 0.0f;
	float SeekTimeElapsed = 0.0f;
	float FleeTimeRemaining = 0.0f;
	float CachedStartingRoundRadius = 0.f;

	void TransitionToState(EAIState NewState);
	float ComputeSteering(FVector ToAim) const;
	/** Fires five forward whisker traces against WorldStatic.
	 *  Returns a steering correction in [-1, 1] derived from left/right clearance comparison.
	 *  OutWallDanger is set to the peak proximity across all hits (0 = clear, 1 = wall at bumper)
	 *  and is used for throttle reduction independent of steering direction. */
	float ComputeWallAvoidanceSteering(float& OutWallDanger) const;
	ADerbyDemoPawn* FindNearestEnemy() const;

	/** Bound to ADerbyDemoGameState::StartRoundDelegate. Transitions from PreStartRound to StartingRound. */
	UFUNCTION()
	void OnRoundStarted();

protected:
	virtual void BeginPlay() override;
};
