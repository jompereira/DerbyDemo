// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerbyResultsUI.h"

void UDerbyResultsUI::NotifyEliminated()
{
	BP_OnEliminated();
}

void UDerbyResultsUI::NotifyRoundEnd(bool bLocalPlayerWon)
{
	BP_OnRoundEnd(bLocalPlayerWon);
}
