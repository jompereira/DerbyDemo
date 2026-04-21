// Copyright Epic Games, Inc. All Rights Reserved.


#include "DerbyDemoSportsWheelRear.h"

UDerbyDemoSportsWheelRear::UDerbyDemoSportsWheelRear()
{
	WheelRadius = 40.f;
	WheelWidth = 40.0f;
	FrictionForceMultiplier = 2.7f;
	SlipThreshold = 20.0f;
	SkidThreshold = 20.0f;
	MaxSteerAngle = 0.0f;
	MaxHandBrakeTorque = 8000.0f;

	// Leave LateralSlipGraph empty — engine uses CorneringStiffness fallback path,
	// which scales force linearly with slip angle. Tune CorneringStiffness instead.
	LateralSlipGraph.GetRichCurve()->Reset();
}