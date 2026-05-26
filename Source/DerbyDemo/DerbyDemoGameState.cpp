// Fill out your copyright notice in the Description page of Project Settings.

#include "DerbyDemoGameState.h"

void ADerbyDemoGameState::BeginRound()
{
	RoundPhase = ERoundPhase::InProgress;
	StartRoundDelegate.Broadcast();
}
