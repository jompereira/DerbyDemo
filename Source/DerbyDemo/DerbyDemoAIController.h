#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DerbyDemoAIController.generated.h"

class ADerbyDemoPawn;

UENUM(BlueprintType)
enum class EAIState : uint8
{
	StartingRound UMETA(DisplayName="Starting Round"),
	Seeking       UMETA(DisplayName="Seeking"),
	Ramming       UMETA(DisplayName="Ramming"),
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

	/** World-space position of the arena centre; AI drives here at round start before engaging */
	UPROPERTY(EditAnywhere, Category="AI|Starting Round")
	FVector ArenaCenter = FVector::ZeroVector;

	/** Radius (cm) within which the AI considers itself to have reached the arena centre */
	UPROPERTY(EditAnywhere, Category="AI|Starting Round", meta=(ClampMin="0.0"))
	float StartingRoundRadius = 500.0f;

private:
	UPROPERTY()
	TObjectPtr<ADerbyDemoPawn> VehiclePawn;

	/** Current state — visible in the Details panel during PIE for easy debugging */
	UPROPERTY(VisibleInstanceOnly, Category="AI|Debug")
	EAIState CurrentState = EAIState::StartingRound;

	float TimeAtLowSpeed = 0.0f;
	float ReverseTimeRemaining = 0.0f;

	void TransitionToState(EAIState NewState);
	float ComputeSteering(FVector ToAim) const;
	ADerbyDemoPawn* FindNearestEnemy() const;
};
