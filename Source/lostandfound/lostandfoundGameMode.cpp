// Copyright Epic Games, Inc. All Rights Reserved.

#include "lostandfoundGameMode.h"
#include "lostandfoundCharacter.h"
#include "UObject/ConstructorHelpers.h"

AlostandfoundGameMode::AlostandfoundGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
