// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev6MultiPGameMode.h"
#include "Dev6MultiPCharacter.h"
#include "UObject/ConstructorHelpers.h"

ADev6MultiPGameMode::ADev6MultiPGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
