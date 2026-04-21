// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyDemoWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UDerbyDemoWheelRear::UDerbyDemoWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}