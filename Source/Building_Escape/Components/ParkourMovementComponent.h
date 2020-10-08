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
	HangingState_NotHanging,
	HangingState_AdjustingLocation,
	HangingState_Hanging,
	HangingState_TraversingACorner
};

//Enumerator that signifies the current state of the edges on the left and right extremes of the curernt hanging plane; used only internally
enum EEdgeState
{
	EdgeState_Unknown,
	EdgeState_Block,
	EdgeState_Corner,
};

UENUM(BlueprintType)
enum EParkourMovementState
{
	ParkourState_Walk   UMETA(DisplayName = "Walk"),
	ParkourState_Crawl   UMETA(DisplayName = "Crawl"),
	ParkourState_Slide   UMETA(DisplayName = "Slide"),
	ParkourState_Jump   UMETA(DisplayName = "Jump"),
	ParkourState_TuckJump   UMETA(DisplayName = "TuckJump"),
	ParkourState_Hang   UMETA(DisplayName = "Hang"),
	ParkourState_Wallrun   UMETA(DisplayName = "Wallrun"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FParkourMovementStateChangedSignature, TEnumAsByte<EParkourMovementState>, PrevParkourState, TEnumAsByte<EParkourMovementState>, NewParkourState);

//Enumerator identify directions relative to the player; used only internally
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

	/*A function that is called when a transition to hanging or across a corner, etc occurs. The default implementation just teleport the player. 
	The default implementation will be skipped if the delegate that was passed in bound*/
	void AdjustHangLocation(FVector TargetLocation, FRotator TargetRotation, FHangingTransitionDelegate TransitionDelegate);
	//If overriding this function, the designer is responsible for calling AdjustmentedEnded() manually

	UFUNCTION(BlueprintPure)
	TEnumAsByte<EParkourMovementState> GetMovementState();

	UFUNCTION()
	void OnMovementModeChangedDelegate(class ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	//Functions triggered by the player when pressing/releasing the crouch input. AttemptCrouch decides if the player character should perform any special moves(depending on the current ParkourMovementState)
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

	// Dimensions of the player capsule; set in BeginPlay
	float CapsuleRadius;
	float CapsuleHalfHeight;

//Begin Hang system
//Internal variables
	// Enumerators that store the current status of horizontal edges(extremes of the wall that the player is currently hanging on) that decide in which way the edges will be interacted with(block or corner transition)
	TEnumAsByte<EEdgeState> LeftEdgeState = EdgeState_Unknown;
	TEnumAsByte<EEdgeState> RightEdgeState = EdgeState_Unknown;
	// Pointers to collider boxes that are responsible for edge interactions. The colliders are spawned(and destroyed) in runtime and act as blocking volumes or triggers depending on the edge status.
	UBoxComponent* LeftCollider = nullptr;
	UBoxComponent* RightCollider = nullptr;
	// Transform variables that store the location and rotation(Scale is always set to 1,1,1) the player will be blended to after traversing the respecitve corner. If the edge is non-traversable, the respective variable is not used.
	FTransform LeftEdgeTargetTransform;
	FTransform RightEdgeTargetTransform;
	// Signifies which particular procedure the hanging system is currently performing. It should be assigned only using the ChangeHangingState function
	TEnumAsByte<EHangingState> CurrentHangingState;

//Interntal functions
	// Assigns the value passed to CurrentHangingState in and then applies effects specific to the new state
	void ChangeHangingState(TEnumAsByte<EHangingState> NewHangingState);
	// Performs several traces that verify if the location and rotation passed can be projected to a fully valid hanging spot. The out parameters are only assigned when the function returns true
	bool IsValidHangPoint(OUT FVector& OutHangLocation, OUT FRotator& OutHangRotation, FVector InOriginLocation, FRotator InOriginRotation) const;
	// Locks the player movement to the plane to the pawns sides or reverts that lock, depending on the bool passed in
	void TogglePlaneLock(bool bNewIsLocked);
	// Tries to detect a corner or blockage in every direction(inner left, outer left, inner right, outer right) and sets the EdgeState variables accordingly, as well as calls SpawnEdgeCollider when necessary
	void UpdateEdgeStatuses();
	// Calls IsValidHangPoint on a location offset depending on the bools passed in. Assigns the out paramater only when returning true. Called several times by UpdateEdgeStatuses
	bool TestEdgeForCorner(bool bIsEdgeToTheRight, bool bTestForOuterEdge, OUT FTransform& OutTransform) const;
	// Spawnes a collider box that represents an interaction with an edge - it act as blocking volumes when the appropriate EdgeState edge is set to _Block and as triggers if it's set to _Corner
	void SpawnEdgeCollider(bool bIsRightEdge, FTransform SpawnTransform);
	// This function destroys any spawned colliders and resets the EdgeStatus variables to "Unknown". It is triggering when ceasing to hang altogether or when transitioning to a new plane
	void ResetHangingColliders();

	// Called when AdjustLocation functions have done their job; If any of them are overriden, it is the designers job to call it after they are finished
	UFUNCTION(BlueprintCallable)
	void AdjustmentEnded();

	// Calls IsValidHangPoint with the current location and rotation passed in and begins hanging at the returned transform if the check returned true; Call this to attempt hanging
	UFUNCTION(BlueprintCallable)
	bool TryToHangInCurrentLocation();

	// Ceases the hanging altogether. Call to exit hang
	UFUNCTION(BlueprintCallable)
	void FinishHang();

	//Delegates intended to be passed into the AdjustHangLocation function, used to differentiate between the various types of transition
	UPROPERTY(BlueprintAssignable)
	FGrabStateChangedDelegate HangingStateChanged;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate LocationAdjustment;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate CornerAdjustment;

	UPROPERTY(BlueprintAssignable)
	FHangingTransitionDelegate ClimbUpAdjustment;
//End hang system

	UPROPERTY(BlueprintAssignable)
	FParkourMovementStateChangedSignature ParkourMovementStateChangedDelegate;

	UPROPERTY(EditAnywhere)
	float MinSlideSpeed = 800.f;

	UPROPERTY(EditAnywhere)
	float MinTuckJumpSpeed = 200.f;

	UPROPERTY(EditAnywhere)
	float SlideForce = 800.f;

	UPROPERTY(EditAnywhere)
	float TuckJumpForwardForce = 1200.f;

	UPROPERTY(EditAnywhere)
	float SlideDuration = 1.f;

	//Spawned colliders that act as triggers for corner transitions are all bound to this function; Differentation between the left and right collider is handled inside the implementation
	UFUNCTION()
	void EdgeOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	TEnumAsByte<EParkourMovementState> CurrentMovementState;

	UFUNCTION()
	void SetParkourState(TEnumAsByte<EParkourMovementState> NewState);

	UFUNCTION(BlueprintPure, DisplayName = "Is touching left wall")
	bool GetIsTouchingLeftWall();

	UPROPERTY(EditAnywhere)
	float MaxWallrunTime; // Duration of WallrunTimerHandle

	//UCharacterMovementComponent overrides
	virtual void Crouch(bool bClientSimulation = false) override;
	virtual void UnCrouch(bool bClientSimulation = false) override;
	
	//Handles of timers used by the parkour mechanics
	FTimerHandle NoHangTimerHandle;
	FTimerHandle NoWallrunTimerHandle;
	FTimerHandle WallrunTimerHandle;
	FTimerHandle SlideTimerHandle;

	//Start and Stop functions for the timers
	void StartNoHangTimer();
	void EndNoHangTimer();
	void StartNoWallrunTimer();
	void EndNoWallrunTimer();
	void StartSlideTimer();
	void EndSlideTimer();
	void StartWallrunTimer();
	void EndWallrunTimer();

	//Checks for overlap for each value of ETraceDirection
	void UpdateBlockedDirections();
	//Returns if a trace in given direction results in overlap; used inside the function above
	bool TraceForBlockInDirection(TEnumAsByte<ETraceDirection> TraceDirection);
	//Map of rotation offsets, set once in BeginPlay
	TMap<TEnumAsByte<ETraceDirection>, FRotator> TraceDirectionOffsets;
	//Map containing the last result of traces done by UpdateBlockedDirections();
	TMap < TEnumAsByte <ETraceDirection> , FHitResult > DirectionTraceHitResults;
	//functions triggered when UpdateBlockedDirections() starts and stops overallping a direction
	void OnDirectionOverlap(TEnumAsByte<ETraceDirection> TraceDirection);
	void OnDirectionOverlapEnd(TEnumAsByte<ETraceDirection> TraceDirection);
	
	//A series of conditions that must al lreturn true if wallruning is to begin/continue
	bool IsFullfillingWallrunConditions();
	//Decides wherever performing a Tuck Jump should boost the players forward velocity(which should be possible only once per jump)
	bool bCanAirBoost = true;
	
	//Performs a jump with strong forward motion relative to control rotation
	void Lunge();

	//Player location before the last jump
	FVector JumpOffPoint;

	//Sets parkour movement state to Walk, Crawl or Jump according to MovementMode; used to transition out of special move states
	void ResetToBasicParkourState();

};
