// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InteractionSystemLibrary.generated.h"

/**
 * 
 */
UENUM(BlueprintType)
enum EInteractionType
{
	Use    UMETA(DisplayName = "Use"),
	Hold   UMETA(DisplayName = "Hold"),
};

UENUM(BlueprintType)
enum EFocusState
{
	FocusedState    UMETA(DisplayName = "Focused"),
	MarkedState  UMETA(DisplayName = "Marked"),
	InactiveState UMETA(DisplayName = "Inactive"),
};
