// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// DefaultPawns are simple pawns that can fly around the world.
//
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Pawn.h"
#include "DefaultEscapePawn.generated.h"

class UInputComponent;
class UPawnMovementComponent;
class UCapsuleComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum EPawnMovementState
{
	Walk    UMETA(DisplayName = "Walk"),
	Crawl   UMETA(DisplayName = "Crawl"),
	Slide   UMETA(DisplayName = "Slide"),
	Jump   UMETA(DisplayName = "Jump"),
	Kong   UMETA(DisplayName = "Kong"),
	Hang   UMETA(DisplayName = "Hang"),
	Wallrun   UMETA(DisplayName = "Wallrun"),
};
/**
 * DefaultPawn implements a simple Pawn with spherical collision and built-in flying movement.
 * @see UFloatingPawnMovement
 */
UCLASS(config = Game, Blueprintable, BlueprintType)
class BUILDING_ESCAPE_API ADefaultEscapePawn : public APawn
{
	GENERATED_UCLASS_BODY()

		// Begin Pawn overrides
		virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;
	virtual void UpdateNavigationRelevance() override;
	// End Pawn overrides

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

	UPROPERTY()
		bool bIsAirborn = false;

	UFUNCTION(BlueprintCallable)
	void SetIsAirborn(bool bNewIsAirborn);
	
	UFUNCTION(BlueprintPure)
	bool GetIsAirborn();

	UPROPERTY()
	TEnumAsByte<EPawnMovementState> CurrentMovementState;
	
	UFUNCTION(BlueprintCallable)
	void SetMovementState(TEnumAsByte<EPawnMovementState> NewState);

	UFUNCTION(BlueprintPure)
	TEnumAsByte<EPawnMovementState> GetMovementState();

public:
	/** Name of the MovementComponent.  Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName MovementComponentName;

protected:
	/** DefaultPawn movement component */
	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UPawnMovementComponent* MovementComponent;

public:
	/** Name of the CollisionComponent. */
	static FName CollisionComponentName;

private:
	/** DefaultPawn collision component */
	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UCapsuleComponent* CollisionComponent;
public:

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName MeshComponentName;

private:
	/** The mesh associated with this Pawn. */
	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UStaticMeshComponent* MeshComponent;
public:

	/** If true, adds default input bindings for movement and camera look. */
	UPROPERTY(Category = Pawn, EditAnywhere, BlueprintReadOnly)
		uint32 bAddDefaultMovementBindings : 1;

	/** Returns CollisionComponent subobject **/
	UCapsuleComponent* GetCollisionComponent() const { return CollisionComponent; }
	/** Returns MeshComponent subobject **/
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};

