// Copyright Epic Games, Inc. All Rights Reserved.


#include "DerbyDemoSportsWheelRear.h"

UDerbyDemoSportsWheelRear::UDerbyDemoSportsWheelRear()
{
	WheelRadius = 40.f;
	WheelWidth = 40.0f;
	FrictionForceMultiplier = 2.2f;
	SlipThreshold = 20.0f;
	SkidThreshold = 20.0f;
	MaxSteerAngle = 0.0f;
	MaxHandBrakeTorque = 8000.0f;

	FRichCurve* Curve = LateralSlipGraph.GetRichCurve();
	Curve->Reset();
	Curve->AddKey(0.0f,  0.0f);
	Curve->AddKey(6.0f,  1.0f);   // peak grip
	Curve->AddKey(20.0f, 0.8f);   // gradual drop — rear can self-correct
	Curve->AddKey(40.0f, 0.65f);
}