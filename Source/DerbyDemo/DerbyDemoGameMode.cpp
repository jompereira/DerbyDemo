// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyDemoGameMode.h"
#include "DerbyDemoPlayerController.h"

ADerbyDemoGameMode::ADerbyDemoGameMode()
{
	PlayerControllerClass = ADerbyDemoPlayerController::StaticClass();
}
