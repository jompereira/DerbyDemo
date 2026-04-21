// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyDemoWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UDerbyDemoWheelFront::UDerbyDemoWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}