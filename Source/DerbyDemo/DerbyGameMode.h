// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DerbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class DERBYDEMO_API ADerbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DerbyGameMode")
	int  StartRoundSeconds = 3;
	
	UFUNCTION(BlueprintCallable, Category = "DerbyGameMode")
	void StartRound();
	
};
