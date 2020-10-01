// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ParkourMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHangingTransitionDelegate, FVector, Location, FRotator, Rotation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrabStateChangedDelegate, bool, bNewState);

class UBoxComponent;
class UCapsuleComponent;

//Enumarator that signifies the current state of hanging; used only internally
enum EHangingState
{
	NotHanging,
	AdjustingLocation,
	Hanging,
	TraversingACorner
};

//Enumerator that signifies the current state of the edges on the left and right extremes of the curernt hanging plane; used only internally
enum EEdgeState
{
	Unknown,
	Block,
	Corner,
};

UENUM(BlueprintType)
enum EParkourMovementState
{
	MSE_Walk   UMETA(DisplayName = "Walk"),
	MSE_Crawl   UMETA(DisplayName = "Crawl"),
	MSE_Slide   UMETA(DisplayName = "Slide"),
	MSE_Jump   UMETA(DisplayName = "Jump"),
	MSE_Kong   UMETA(DisplayName = "Kong"),
	MSE_Hang   UMETA(DisplayName = "Hang"),
	MSE_Wallrun   UMETA(DisplayName = "Wallrun"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FParkourMovementStateChangedSignature, TEnumAsByte<EParkourMovementState>, PrevMovementState, TEnumAsByte<EParkourMovementState>, NewMovementState);

enum ETraceDirection
{
	TraceDirection_Ahead,
	TraceDirection_Behind,
	TraceDirection_Left,
	TraceDirection_Right,
	TraceDirection_Up,
	TraceDirection_Down,
	MAX
};

UCLASS(Blueprintable)
class BUILDING_ESCAPE_API UParkourMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	UParkourMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool DoJump(bool bReplayingMoves) override;
	virtual bool CanAttemptJump() const override;

	//BEGIN UCharacterMovementComponent Interface
	virtual bool IsMovingOnGround() const override;
	virtual bool IsCrouching() const override;
	virtual bool CanCrouchInCurrentState() const override;
	//END UCharacterMovementComponent Interface

// Overridable functions that are called when a transition to hanging or across a corner occurs. The default implementations just teleport the player
	void AdjustHangLocation(FVector TargetLocation, FRotator TargetRotation, FHangingTransitionDelegate TransitionDelegate);

// Setter and Getter functions for private variables
	UFUNCTION(BlueprintCallable)
	void SetIsAirborn(bool bNewIsAirborn);

	UFUNCTION(BlueprintPure)
	bool GetIsAirborn();

	UFUNCTION(BlueprintPure)
	TEnumAsByte<EParkourMovementState> GetMovementState();

	UFUNCTION()
	void OnMovementModeChangedDelegate(class ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	void AttemptCrouch();
	void AttemptUnCrouch();

private:

	// Parameters that define the rules of testing hangability and attachment. Values can be overriden from blueprint through an appropriate function
	
	// Siza of box trace that is performed just above the potential edge to check if there is sufficient empty space
	FVector HandSize;
	// Vertical distance from player pivot at which the test is performed
	float GrabHeight;
	// forward distance from player pivot at which the test is performed
	float GrabbingReach;
	// Vertical distance between player pivot and the edge that the player is hanging on
	float AttachHeight;
	// Forward distance between the player pivot and the wall the player is hanging on
	float AttachDistance;
//
	// Pointers to the owners root capsule and movement component; assigned in BeginPlay
	UCapsuleComponent* PawnsRootPrimitive = nullptr;
	UPawnMovementComponent* PawnsMovementComponent = nullptr;
	// Dimensions of the player capsule; set in BeginPlay
	float CapsuleRadius;
	float CapsuleHalfHeight;

	// Enumerators that stores the current status of horizontal edges(extremes of the wall that the player is currently hanging on) that decide in which way the edges will be interacted with(block or corner transition)
	TEnumAsByte<EEdgeState> LeftEdgeState = Unknown;
	TEnumAsByte<EEdgeState> RightEdgeState = Unknown;
	// Pointers to collider boxes responsible for edge interactions. The colliders are spawned(and destroyed) in runtime and act as blocking volumes or triggers depending on the edge status.
	UBoxComponent* LeftCollider = nullptr;
	UBoxComponent* RightCollider = nullptr;
	// Transform variables that store the location and rotation(Scale is always set to 1,1,1) the player will be blended to after traversing the respecitve corner. If the edge is non-traversable, the respective variable is not used.
	FTransform LeftEdgeTargetTransform;
	FTransform RightEdgeTargetTransform;

	// Performs several traces that verify if the location and rotation passed can be projected to a fully valid hanging spot. The out parameters are only assigned when the function returns true
	bool IsValidHangPoint(OUT FVector& OutHangLocation, OUT FRotator& OutHangRotation, FVector InOriginLocation, FRotator InOriginRotation) const;

	// Locks the player movement to the plane to the pawns sides or reverts that lock, depending on the bool passed in
	void TogglePlaneLock(bool bNewIsLocked);

	// This value signifies which procedure that forms the hanging system is currently underway. It should be assigned only using the function below
	TEnumAsByte<EHangingState> CurrentHangingState;

	// Assigns the value passed to CurrentHangingState in and then applies effects specific to the new state
	void ChangeHangingState(TEnumAsByte<EHangingState> NewHangingState);

	//Tries to detect a corner or blockage in every direction(inner left, outer left, inner right, outer right) and sets the EdgeState variables accordingly, as well as calls SpawnEdgeCollider when necessary
	void UpdateEdgeStatuses();

	//Triggers IsValidHangPoint on a location offset depending on the bools passed in. Assigns the out paramater only when returning true. Called several times by UpdateEdgeStatuses
	bool TestEdgeForCorner(bool bIsEdgeToTheRight, bool bTestForOuterEdge, OUT FTransform& OutTransform) const;

	// Collider boxes are spawned to represent an interaction with an edge - they act as blocking volumes when the edge is non-traversable and as triggers if a corner transition is expected
	void SpawnEdgeCollider(bool bIsRightEdge, FTransform SpawnTransform);

	// This function destroys any spawned colliders and resets the EdgeStatus variables to "Unknown". It is triggering when ceasing to hang altogether or when transitioning to a new plane
	void Reset();

	//Overrides default detection and attachment parameters with custom values
	UFUNCTION(BlueprintCallable)
	void SetParameters(FVector NewHandSize, float NewGrabHeight, float NewGrabbingReach, float NewAttachDistance, float NewAttachHeight);

	//Called when AdjustLocation functions have done their job; If any of them are overriden, it is the designers job to call it after they are finished
	UFUNCTION(BlueprintCallable)
	void AdjustmentEnded();

	//Triggers IsValidHangPoint with the current location and rotation passed in and begins hanging at the returned transform if the check returned true; Call this to attempt hanging
	UFUNCTION(BlueprintCallable)
	bool TryToHangInCurrentLocation();

	//Ceases the hanging altogether. Call to exit hang
	UFUNCTION(BlueprintCallable)
	void FinishHang();

	//A dispatcher that triggers wherever the CurrentHangingState changes from NotHanging to any other state or the reverse. It returns a bool that signifies wherever the new state is NotHanging or any other state.
	UPROPERTY(BlueprintAssignable)
	FGrabStateChangedDelegate HangingStateChanged;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate LocationAdjustment;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate CornerAdjustment;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate ClimbUpAdjustment;

	UPROPERTY(BlueprintAssignable)
	FParkourMovementStateChangedSignature ParkourMovementStateChangedDelegate;

	UPROPERTY(EditAnywhere)
	float MinSlideSpeed = 800.f;

	UPROPERTY(EditAnywhere)
	float MinMonkeyJumpSpeed = 200.f;

	UPROPERTY(EditAnywhere)
	float SlideForce = 800.f;

	UPROPERTY(EditAnywhere)
	float MonkeyJumpForce = 1200.f;

	UPROPERTY(EditAnywhere)
	float SlideDuration = 1.f;

	//Spawned colliders that act as triggers for corner transitions are all bound to this function; Differentation between the left and right collider is handled inside the functions implementation
	UFUNCTION()
	void EdgeOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	TEnumAsByte<EParkourMovementState> CurrentMovementState;

	UFUNCTION(BlueprintCallable)
	void SetMovementState(TEnumAsByte<EParkourMovementState> NewState);

	UFUNCTION(BlueprintPure, DisplayName = "Is touching left wall")
	bool GetIsTouchingLeftWall();

	UPROPERTY()
	bool bIsAirborn = false;

	bool bIsTouchingLeftWall = false;

	float MaxWallrunTime;



	//Handles of timers used by the parkour mechanics
	FTimerHandle NoHangTimerHandle;
	FTimerHandle NoWallrunTimerHandle;
	FTimerHandle WallrunTimerHandle;
	FTimerHandle SlideTimerHandle;
	bool bHangingLocked = false;
	void StartNoHangTimer();
	void EndNoHangTimer();
	void StartNoWallrunTimer();
	void EndNoWallrunTimer();
	void StartSlideTimer();
	void EndSlideTimer();
	bool TraceForBlockInDirection(TEnumAsByte<ETraceDirection> TraceDirection);
	TMap<TEnumAsByte<ETraceDirection>, FRotator> TraceDirectionOffsets;
	TMap < TEnumAsByte <ETraceDirection> , FHitResult > DirectionTraceHitResults;
	TArray<TEnumAsByte<ETraceDirection>> BlockedDirections;
	void UpdateBlockedDirections();
	void OnDirectionOverlap(TEnumAsByte<ETraceDirection> TraceDirection);
	void OnDirectionOverlapEnd(TEnumAsByte<ETraceDirection> TraceDirection);
	void WallrunTimerStart();
	void WallrunTimerEnd();
	void Lunge();
	bool bCanWallrun = true;
	FVector JumpOffPoint;
	bool IsFullfillingWallrunConditions();
	bool bCanMonkeyJump = true;
	void ResetToBasicState();

	virtual void Crouch(bool bClientSimulation = false) override;
	virtual void UnCrouch(bool bClientSimulation = false) override;
};
