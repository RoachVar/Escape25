// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// DefaultPawns are simple pawns that can fly around the world.
//
//=============================================================================

// HACK: This class is essentially a slightly modified clone of the Default Pawn class. It would probably be strongly preferable to just work on the source version of the engine and modify to be allowed to modify that default class.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Character.h"
#include "DefaultEscapePawn.generated.h"

class UInputComponent;
class UPawnMovementComponent;
class UParkourMovementComponent;

/**
 * DefaultPawn implements a simple Pawn with spherical collision and built-in flying movement.
 * @see UFloatingPawnMovement
 */
UCLASS(config = Game, Blueprintable, BlueprintType)
class BUILDING_ESCAPE_API ADefaultEscapePawn : public ACharacter
{
	GENERATED_UCLASS_BODY()

	// Begin Pawn overrides
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;
	virtual void UpdateNavigationRelevance() override;
	// End Pawn overrides

	// Begin Character overrides
	// End Character overrides

	UParkourMovementComponent* GetParkourMovementComponent() const;

	/**
	 * Input callback to move forward in local space (or backward if Val is negative).
	 * @param Val Amount of movement in the forward direction (or backward if negative).
	 * @see APawn::AddMovementInput()
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
		virtual void MoveForward(float Val);

	/**
	 * Input callback to strafe right in local space (or left if Val is negative).
	 * @param Val Amount of movement in the right direction (or left if negative).
	 * @see APawn::AddMovementInput()
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
		virtual void MoveRight(float Val);

	/**
	 * Input callback to move up in world space (or down if Val is negative).
	 * @param Val Amount of movement in the world up direction (or down if negative).
	 * @see APawn::AddMovementInput()
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
		virtual void MoveUp_World(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
		virtual void TurnAtRate(float Rate);

	/**
	* Called via input to look up at a given rate (or down if Rate is negative).
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
		virtual void LookUpAtRate(float Rate);

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pawn")
		float BaseTurnRate;

	/** Base lookup rate, in deg/sec. Other scaling may affect final lookup rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pawn")
		float BaseLookUpRate;

//public:
//	/** Name of the MovementComponent.  Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
//	static FName MovementComponentName;
//
//protected:
//	/** DefaultPawn movement component */
//	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
//		UPawnMovementComponent* MovementComponent;

public:

	/** If true, adds default input bindings for movement and camera look. */
	UPROPERTY(Category = Pawn, EditAnywhere, BlueprintReadOnly)
		uint32 bAddDefaultMovementBindings : 1;

};

