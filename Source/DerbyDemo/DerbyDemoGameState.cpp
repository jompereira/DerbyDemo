// Fill out your copyright notice in the Description page of Project Settings.

#include "DerbyDemoGameState.h"

void ADerbyDemoGameState::BeginRound()
{
	RoundPhase = ERoundPhase::InProgress;
	StartRoundDelegate.Broadcast();
}

void ADerbyDemoGameState::EndRound(AActor* Winner)
{
	RoundPhase = ERoundPhase::PostRound;
	RoundEndDelegate.Broadcast(Winner);
}
