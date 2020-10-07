// Copyright Roch Karwacki 2020


#include "ParkourMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
UParkourMovementComponent::UParkourMovementComponent()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	NavAgentProps.bCanCrouch = true;
	bCrouchMaintainsBaseLocation = false;
	bUseSeparateBrakingFriction = false;
	GroundFriction = 8.f;
	

}

// Called when the game starts or when spawned
void UParkourMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxWallrunTime = 2;
	SetComponentTickEnabled(true);
	((UCapsuleComponent*)(GetOwner()->GetRootComponent()))->GetScaledCapsuleSize(OUT CapsuleRadius, OUT CapsuleHalfHeight);

	HandSize = FVector(5, 15, 1);
	// Vertical distance from player pivot at which the test is performed
	GrabHeight = 65;
	// forward distance from player pivot at which the test is performed
	GrabbingReach = 100;
	// Vertical distance between player pivot and the edge that the player is hanging on
	AttachHeight = 65.0;
	// Forward distance between the player pivot and the wall the player is hanging on
	AttachDistance = CapsuleRadius + 6;

	TraceDirectionOffsets.Add(TraceDirection_Ahead, FRotator(0, 0, 0));
	TraceDirectionOffsets.Add(TraceDirection_Behind, FRotator(0, 180, 0));
	TraceDirectionOffsets.Add(TraceDirection_Left, FRotator(0, -90, 0));
	TraceDirectionOffsets.Add(TraceDirection_Right, FRotator(0, 90, 0));
	TraceDirectionOffsets.Add(TraceDirection_Up, FRotator(90, 0, 0));
	TraceDirectionOffsets.Add(TraceDirection_Down, FRotator(-90, 0, 0));
}

void UParkourMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentMovementState) {
	case ParkourState_Walk:
		UpdateBlockedDirections();
		break;
	case ParkourState_Jump:
		UpdateBlockedDirections();
		if (GetWorld()->GetTimerManager().IsTimerActive(NoHangTimerHandle)) { return; }
		TryToHangInCurrentLocation();
		break;
	case ParkourState_Hang:
		if (CurrentHangingState != Hanging)
		{
			return;
		}
		//Checking if the player is currently on either edge of the current plane. If so, a collider box that enforces appropriate behaviour(blocking or traversing a corner) will be spawned
		UpdateEdgeStatuses();
		break;
	case ParkourState_Wallrun:
		if (!IsFullfillingWallrunConditions())
		{
			ResetToBasicParkourState();
		}
		else
		{
			bool bHeightBoostPossible = abs(JumpOffPoint.Z - LastUpdateLocation.Z) < 300 && LastUpdateLocation.Z - JumpOffPoint.Z - 300 < 0;
			Velocity = GetOwner()->GetActorRotation().RotateVector(FVector(1200, DirectionTraceHitResults.Contains(TraceDirection_Left) ? -200 : 200, bHeightBoostPossible ? (JumpOffPoint.Z + JumpZVelocity) - LastUpdateLocation.Z : 0));
			UpdateBlockedDirections();
		}
		break;
	}

}

void UParkourMovementComponent::OnMovementModeChangedDelegate(class ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (CurrentMovementState == ParkourState_Hang && CurrentHangingState != NotHanging) { return; }
	ResetToBasicParkourState();
}

void UParkourMovementComponent::AttemptCrouch()
{
	switch (CurrentMovementState) {
	case ParkourState_Walk:
		bWantsToCrouch = true;
		if (LastUpdateRotation.UnrotateVector(Velocity).X >+ MinSlideSpeed)
		{
			SetParkourState(ParkourState_Slide);
		}
		break;
	case ParkourState_Jump:
		bWantsToCrouch = true;
		if (LastUpdateRotation.UnrotateVector(Velocity).X >+ MinTuckJumpSpeed)
		{
			SetParkourState(ParkourState_TuckJump);
		}
		break;
	case ParkourState_Hang:
		FinishHang();
		break;
	default:
		break;
	}
}

void UParkourMovementComponent::AttemptUnCrouch()
{
	bWantsToCrouch = false;
}

void UParkourMovementComponent::Crouch(bool bClientSimulation)
{
	Super::Crouch(bClientSimulation);
	ResetToBasicParkourState();
}

void UParkourMovementComponent::UnCrouch(bool bClientSimulation)
{
	Super::UnCrouch(bClientSimulation);
	ResetToBasicParkourState();
}

void UParkourMovementComponent::ResetToBasicParkourState()
{
	switch (MovementMode) {
	case MOVE_Walking:
		if (IsCrouching())
		{
			if (CurrentMovementState == ParkourState_Slide && GetWorld()->GetTimerManager().IsTimerActive(SlideTimerHandle)) { break; }
			SetParkourState(ParkourState_Crawl);
		}
		else
		{
			SetParkourState(ParkourState_Walk);
		}
		break;
	case MOVE_Flying:
	case MOVE_Falling:
		if (!(IsCrouching()))
		{
			SetParkourState(ParkourState_Jump);
		}
		break;
	default:
		break;
	}
}

void UParkourMovementComponent::UpdateEdgeStatuses()
{
	//The loop executes exactly two times (index 0 for left side, index 1 for right side)
	for (int Iteration = 0; Iteration <= 1; Iteration++)
	{
		//The first interaction(Interaction 0) will test the left side, while the subsequent iteration will test the right side
		bool bTestRight = (Iteration == 1);

		//Initialising a variable that refers to the pointer to the aproppriate Enumerator variable depending on the direction;
		TEnumAsByte<EEdgeState> * TestedEdgeState = &(bTestRight ? RightEdgeState : LeftEdgeState);
		//If the state of the given edge was already discovered, the current iteration is skipped
		if (*TestedEdgeState != Unknown) { continue; }
	
		//This float is used to flip a vector left or right depending on which direction is being tested in this iteration
		float Direction = bTestRight ? 1 : -1;

		/*Testing wherever the location offset by half the capsule radius in the tested direction is a valid spot for hanging there; 
		if so, it's not an edge at all so further testing is skipped and the edge is still considered "Unknown".
		The OUT variables below aren't actually necesarry in this case and won't be used.*/
		FVector OutVector;
		FRotator OutRotator;
		float HorizontalOffset = Direction * CapsuleRadius/2;
		FVector TestedHangLocation = GetActorLocation() + GetOwner()->GetActorRotation().RotateVector(FVector(0, HorizontalOffset, 0));
	
		if (IsValidHangPoint(OUT OutVector,OUT OutRotator, TestedHangLocation, GetOwner()->GetActorRotation()))
		{ continue;	}
		//Initialising a variable that refers to the pointer of the aproppriate transform variable depending on the direction;
		FTransform * TestedCornerTargetTransform = &(bTestRight ? RightEdgeTargetTransform : LeftEdgeTargetTransform);

		//Checking for an inner corner first, then an inner corner(if the first test failed)
		if ((TestEdgeForCorner(bTestRight, false, OUT *TestedCornerTargetTransform)) || (TestEdgeForCorner(bTestRight, true, OUT *TestedCornerTargetTransform)))
		{
			*TestedEdgeState = Corner;
		}
		//If the code executed this far and both edge tests returned false, it means the location is not fit for hanging in any way
		else
		{
			*TestedEdgeState = Block;
		}
		
		//A collider is spawned to the right or left of the player one capsule radius away
		FVector CollisionBoxLocation = GetActorLocation() + GetOwner()->GetActorRotation().RotateVector(FVector(0, Direction * CapsuleRadius, 0));
		SpawnEdgeCollider(bTestRight, FTransform(GetOwner()->GetActorRotation(), CollisionBoxLocation, FVector(1,1,1)));
	}
}

bool UParkourMovementComponent::IsValidHangPoint(OUT FVector& OutHangLocation, OUT FRotator& OutHangRotation, FVector InOriginLocation, FRotator InOriginRotation) const
{
	//Declaring variables needed for tracubg
	FVector AttachTraceStart = InOriginLocation + InOriginRotation.RotateVector(FVector(-CapsuleRadius, 0, GrabHeight - HandSize.Z - 3));
	FVector AttachTraceEnd = AttachTraceStart + InOriginRotation.RotateVector(FVector(GrabbingReach + 1 + CapsuleRadius, 0,0));
	FCollisionQueryParams TraceParams(FName(TEXT("")), false, GetOwner());
	TraceParams.bFindInitialOverlaps = false;
	TraceParams.AddIgnoredActor(GetOwner());
	FHitResult LineTraceHitResult;

	//Performing a trace that seeks for a surface that could support a hanging player
	bool bDidAttachTraceSucceed = GetWorld()->LineTraceSingleByChannel
	(
		OUT LineTraceHitResult,
		AttachTraceStart,
		AttachTraceEnd,
		ECollisionChannel::ECC_Visibility,
		TraceParams
	);

	if (!bDidAttachTraceSucceed)
	{
		//No space to attach
		return false;
	};

	if (LineTraceHitResult.GetComponent()->IsSimulatingPhysics())
	{
		//Component simulates physics - not suitable for attachment
		return false;
	}

	//Calculating hang location and rotation based on hit location and hit normal
	FRotator AdjustedRotation = (UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), LineTraceHitResult.ImpactNormal)) + FRotator(0,180,0);
	FVector AdjustedLocation = LineTraceHitResult.ImpactPoint + AdjustedRotation.RotateVector(FVector(-1 * AttachDistance, 0, 0));
	

	//Preparing variables for the incoming sweep
	FVector HandSpaceTraceStart = FVector(AdjustedLocation.X, AdjustedLocation.Y, AdjustedLocation.Z + GrabHeight + HandSize.Z/2);
	FVector HandSpaceTraceEnd = HandSpaceTraceStart + AdjustedRotation.RotateVector(FVector(AttachDistance + HandSize.X, 0, 0));
	FHitResult SweepResult;
	
	//Sweeping to tell if there is enough space for the players hands
	GetWorld()->SweepSingleByChannel
	(
		OUT SweepResult,
		HandSpaceTraceStart,
		HandSpaceTraceEnd,
		AdjustedRotation.Quaternion(),
		ECollisionChannel::ECC_Visibility,
		FCollisionShape::MakeBox(FVector(HandSize.X, HandSize.Y, HandSize.Z)),
		TraceParams
	);

	if (SweepResult.bBlockingHit)
	{
		//No space for hands
		return false;
	}

	//Tracing straight down to know how hight the should the player be atattached
	FVector HeightTraceStart = FVector(LineTraceHitResult.ImpactPoint.X, LineTraceHitResult.ImpactPoint.Y, LineTraceHitResult.ImpactPoint.Z + GrabHeight + 1) + AdjustedRotation.RotateVector(FVector(HandSize.X, 0, 0));
	FVector HeightTraceEnd = HeightTraceStart - FVector(0, 0, GrabHeight + 1);
	GetWorld()->LineTraceSingleByChannel
	(
		OUT LineTraceHitResult,
		HeightTraceStart,
		HeightTraceEnd,
		ECollisionChannel::ECC_Visibility,
		TraceParams
	);

	if (!LineTraceHitResult.bBlockingHit)
	{
		//Too high
		return false;
	}
	
	//Correcting the Z value of AdjustedLocation using the impact point, offset by AttachHeight
	AdjustedLocation = FVector(AdjustedLocation.X, AdjustedLocation.Y, LineTraceHitResult.ImpactPoint.Z - AttachHeight);

	//Tracing across the dimensions of a theoretical capsule - works better that sweeping with a capsule shape
	//Trace across X dimension
	GetWorld()->LineTraceSingleByChannel
	(
		OUT LineTraceHitResult,
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(-CapsuleRadius, 0, 0)),
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(CapsuleRadius, 0, 0)),
		ECollisionChannel::ECC_Visibility,
		TraceParams
	);
	
	//Trace across Y dimension
	GetWorld()->LineTraceSingleByChannel
	(
		OUT LineTraceHitResult,
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(0, -CapsuleRadius,0)),
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(0, CapsuleRadius, 0)),
		ECollisionChannel::ECC_Visibility,
		TraceParams
	);

	if (LineTraceHitResult.bBlockingHit)
	{

		//No space for body
		return false;
	}

	//Trace across Z dimension
	GetWorld()->LineTraceSingleByChannel
	(	
		OUT LineTraceHitResult,
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(0, 0, CapsuleHalfHeight)),
		AdjustedLocation + AdjustedRotation.RotateVector(FVector(CapsuleRadius, 0, -CapsuleHalfHeight)),
		ECollisionChannel::ECC_Visibility,
		TraceParams
	);

	if (LineTraceHitResult.bBlockingHit)
	{
		//No space for body
		return false;
	}

	//All tests passed - setting out parameters and returning true
	OutHangLocation = AdjustedLocation;
	OutHangRotation = FRotator(0, AdjustedRotation.Yaw, 0);
	return true;
}

bool UParkourMovementComponent::TestEdgeForCorner(bool bIsEdgeToTheRight, bool bTestForOuterEdge, OUT FTransform &OutTransform) const
{
	FRotator OffsetRotation = GetOwner()->GetActorRotation() + FRotator(0, bIsEdgeToTheRight != bTestForOuterEdge ? 90 : -90, 0);
	float XOffset = (bTestForOuterEdge ? 3.5f : -1) * CapsuleRadius;
	float YOffset = bTestForOuterEdge ? (bIsEdgeToTheRight ? 1 : -1) * CapsuleRadius : 0;
	FVector OffsetLocation = GetOwner()->GetActorRotation().RotateVector(FVector(XOffset, YOffset, 0)) + GetActorLocation();

	FVector HangLocation(0, 0, 0);
	FRotator HangRotation(0, 0, 0);

	if (IsValidHangPoint(OUT HangLocation, OUT HangRotation, OffsetLocation, OffsetRotation))
	{
		OutTransform = FTransform(HangRotation, HangLocation, FVector(1, 1, 1));
		return true;
	}

	return false;
}

bool UParkourMovementComponent::TryToHangInCurrentLocation()
{
	FVector HangLocation(0, 0, 0);
	FRotator HangRotation(0, 0, 0);
	bool bCanHang = IsValidHangPoint(OUT HangLocation, OUT HangRotation, GetActorLocation(), GetOwner()->GetActorRotation());

	if (bCanHang)
	{
		SetParkourState(ParkourState_Hang);
		ChangeHangingState(AdjustingLocation);
		AdjustHangLocation(HangLocation, HangRotation, LocationAdjustment);
		return true;
	}
	else
	{
		ChangeHangingState(NotHanging);
		return false;
	}
}

void UParkourMovementComponent::AdjustHangLocation(FVector TargetLocation, FRotator TargetRotation, FHangingTransitionDelegate TransitionDelegate)
{
	if (TransitionDelegate.IsBound())
	{
		TransitionDelegate.Broadcast(TargetLocation, TargetRotation);
	}
	else
	{
		GetOwner()->SetActorLocation(TargetLocation);
		GetOwner()->SetActorRotation(TargetRotation);
		AdjustmentEnded();
	}
}

void UParkourMovementComponent::ChangeHangingState(TEnumAsByte<EHangingState> NewHangingState)
{
	//If the current state is already equal to the one passed in, an early return is triggered; function execution ceases
	if (CurrentHangingState == NewHangingState) { return; }
	
	//Declaring a bool that tells wherever the new state is the NotHangingState
	bool bIsNewStateNotHanging = NewHangingState == NotHanging;
	
	//The delegate is triggered only if the state changed between NotHanging and any other state
	if ((CurrentHangingState == NotHanging) != bIsNewStateNotHanging)
	{
		HangingStateChanged.Broadcast(!bIsNewStateNotHanging);
	}
	
	FVector TargetLocation;
	FRotator TargetRotation;

	//The member variable that stores the current state is set to the value that was passed in
	CurrentHangingState = NewHangingState;
	
	//Futher processes are carried out depending on what the new state is
	switch (CurrentHangingState) {
	case NotHanging:
		StartNoHangTimer();
		TogglePlaneLock(false);
		SetMovementMode(MOVE_Falling);
		GravityScale = 1;
		ResetHangingColliders();
		break;
	case AdjustingLocation:
		GetOwner()->DisableInput(GetWorld()->GetFirstPlayerController());
		GravityScale = 0;
		Velocity = FVector(0, 0, 0);
	case TraversingACorner:
		GetOwner()->SetActorEnableCollision(false);
		ResetHangingColliders();
		break;
	case Hanging:
		TogglePlaneLock(true);
		GetOwner()->SetActorEnableCollision(true);
		GetOwner()->EnableInput(GetWorld()->GetFirstPlayerController());
		break;
	
	}
}

void UParkourMovementComponent::AdjustmentEnded()
{
	ChangeHangingState(Hanging);
}

void UParkourMovementComponent::TogglePlaneLock(bool bNewIsLocked) 
{

	if (bNewIsLocked)
	{
		SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting::Y);
		SetPlaneConstraintNormal(GetOwner()->GetActorRotation().RotateVector(FVector(1,0,0)));
		SetPlaneConstraintEnabled(true);
	}
	else
	{
		SetPlaneConstraintEnabled(false);
	}
}

void UParkourMovementComponent::FinishHang()
{
	ChangeHangingState(NotHanging);
	ResetToBasicParkourState();
}

void UParkourMovementComponent::SpawnEdgeCollider(bool bIsRightEdge, FTransform SpawnTransform)
{
	//Creating a collider and setting its extent
	AActor * NewActor = GetWorld()->SpawnActor<AActor>();
	UBoxComponent * NewColliderObject = NewObject<UBoxComponent>(NewActor);
	
	NewColliderObject->SetBoxExtent(FVector(64, 6, 64));
	
	//Setting the transform of the collider to the transform passed into this function
	NewColliderObject->SetRelativeTransform(SpawnTransform);
	
	//Either changing the collider to a blocking wall or binding it to EdgeOVerlapBegin depending if it represents and non-traversable edge or a corner transistion respectively
	if ((bIsRightEdge? RightEdgeState : LeftEdgeState) == Block)
	{
		NewColliderObject->SetCollisionProfileName("InvisibleWallDynamic"); NewColliderObject->UpdateCollisionProfile();
	}
	else
	{
		NewColliderObject->OnComponentBeginOverlap.AddDynamic(this, &UParkourMovementComponent::EdgeOverlapBegin);
	}
	
	//Registering the component 
	NewColliderObject->RegisterComponent();
	(bIsRightEdge ? RightCollider : LeftCollider) = NewColliderObject;

}

void UParkourMovementComponent::EdgeOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//If the actor that collided with the collision box isn't the pawn that owns the LedgeHangingActor, this collision event is irrelevant; returning immediately.
	if (OtherActor != GetOwner()) { return; }
	
	//Declaring a pointer to the correct TargetTransform variable depending on which edge the collider that caused this event represents
	FTransform * TargetTransform = (OverlappedComp == LeftCollider) ? &LeftEdgeTargetTransform : &RightEdgeTargetTransform;
	
	//Beginning the transition around a corner
	ChangeHangingState(TraversingACorner);
	AdjustHangLocation(TargetTransform->GetLocation(), TargetTransform->GetRotation().Rotator(), CornerAdjustment);
}

void UParkourMovementComponent::ResetHangingColliders()
{
	//Setting edge status enums to the default statE
	LeftEdgeState = Unknown;
	RightEdgeState = Unknown;
	
	//Destroying edge collliders if they exist
	if (LeftCollider)
	{
		LeftCollider->GetOwner()->Destroy();
		LeftCollider= nullptr;
	}
	if (RightCollider)
	{
		RightCollider->GetOwner()->Destroy();
		RightCollider = nullptr;
	}
}

void UParkourMovementComponent::StartWallrunTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(WallrunTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(WallrunTimerHandle, this, &UParkourMovementComponent::EndWallrunTimer, MaxWallrunTime, false);
}

void UParkourMovementComponent::EndWallrunTimer()
{
	WallrunTimerHandle.Invalidate();
	if (CurrentMovementState == ParkourState_Wallrun)
	{
		ResetToBasicParkourState();
	}
}

void UParkourMovementComponent::SetParkourState(TEnumAsByte<EParkourMovementState> NewState)
{
	if (CurrentMovementState == NewState) { return; }
	
	TEnumAsByte<EParkourMovementState> PrevState = CurrentMovementState;
	CurrentMovementState = NewState;
	
	switch (PrevState) {
	case ParkourState_Walk:
		JumpOffPoint = GetOwner()->GetActorLocation();
		break;
	default:
		break;
	}
	

	if (CurrentMovementState == ParkourState_Wallrun || CurrentMovementState == ParkourState_Hang || CurrentMovementState == ParkourState_Slide)
	{
		bUseControllerDesiredRotation = false;
		CharacterOwner->bUseControllerRotationYaw = false;
		GroundFriction = 0.f;
	}
	else
	{
		bUseControllerDesiredRotation = true;
		CharacterOwner->bUseControllerRotationYaw = true;
		GroundFriction = 8.f;
	}

	switch (NewState) {
	case ParkourState_Walk:
		GravityScale = 1;
		bCanAirBoost = true;
		break;
	case ParkourState_Jump:
		GravityScale = 1;
		if (PrevState != ParkourState_Wallrun)
		{
			StartWallrunTimer();
		}
		break;
	case ParkourState_Wallrun:
		GravityScale = 0;
		bCanAirBoost = true;
		break;
	case ParkourState_Hang:
		bWantsToCrouch = false;
		bCanAirBoost = true;
		SetMovementMode(MOVE_Flying);
		break;
	case ParkourState_Slide:
		Velocity += LastUpdateRotation.RotateVector(FVector(SlideForce, 0.f, 0.f));
		StartSlideTimer();
		break;
	case ParkourState_TuckJump:
		if (bCanAirBoost) 
		{
			bCanAirBoost = false;
			Velocity += LastUpdateRotation.RotateVector(FVector(TuckJumpForwardForce, 0.f, 0.f));
		}
		break;
	default:
		GravityScale = 1;
		break;
	}

	ParkourMovementStateChangedDelegate.Broadcast(NewState, CurrentMovementState);
}

TEnumAsByte<EParkourMovementState> UParkourMovementComponent::GetMovementState()
{
	return CurrentMovementState;
}

void UParkourMovementComponent::StartNoHangTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(NoHangTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(NoHangTimerHandle, this, &UParkourMovementComponent::EndNoHangTimer, 0.50f, false);
}

void UParkourMovementComponent::EndNoHangTimer()
{
 //Empty by design
}

void UParkourMovementComponent::StartSlideTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(SlideTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(SlideTimerHandle, this, &UParkourMovementComponent::EndSlideTimer, SlideDuration, false);
}

void UParkourMovementComponent::EndSlideTimer()
{
	if (CurrentMovementState == ParkourState_Slide)
	{
		ResetToBasicParkourState();
	}
}

void UParkourMovementComponent::StartNoWallrunTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(NoWallrunTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(NoWallrunTimerHandle, this, &UParkourMovementComponent::EndNoWallrunTimer, 0.01f, false);
}

void UParkourMovementComponent::EndNoWallrunTimer()
{
	//Empty by design
}

bool UParkourMovementComponent::TraceForBlockInDirection(TEnumAsByte<ETraceDirection> TraceDirection)
{
	FRotator TraceRotationOffset = TraceDirectionOffsets.FindRef(TraceDirection);
	float TraceLength = (TraceDirection == TraceDirection_Up || TraceDirection_Down ? CapsuleHalfHeight + CapsuleRadius : CapsuleRadius * 7);
	FVector TraceStart = GetOwner()->GetActorLocation();
	FCollisionQueryParams TraceParams(FName(TEXT("")), false, GetOwner());
	FHitResult OutputHitResult;
	
	GetWorld()->LineTraceSingleByObjectType(
		OUT OutputHitResult,
		TraceStart,
		TraceStart + (GetOwner()->GetActorRotation() + TraceRotationOffset).RotateVector(FVector(TraceLength, 0, 0)),
		ECollisionChannel::ECC_WorldStatic,
		TraceParams
	);

	DirectionTraceHitResults.Add(TraceDirection, OutputHitResult);

	return OutputHitResult.bBlockingHit;
	

}

void UParkourMovementComponent::UpdateBlockedDirections()
{
	for (int DirectionIndex = 0; DirectionIndex < ETraceDirection::MAX; DirectionIndex++)
	{
		TEnumAsByte<ETraceDirection> TraceDirection = ETraceDirection(DirectionIndex);
		if (TraceForBlockInDirection(TraceDirection))
		{
			OnDirectionOverlap(TraceDirection);
		}
		else
		{
			if (DirectionTraceHitResults.Contains(TraceDirection))
			{
				DirectionTraceHitResults.Remove(TraceDirection);
				OnDirectionOverlapEnd(TraceDirection);
			}
		}
	}
}

void UParkourMovementComponent::OnDirectionOverlap(TEnumAsByte<ETraceDirection> TraceDirection)
{
	switch (TraceDirection) {
		default:
			break;
		case TraceDirection_Ahead:
			if (CurrentMovementState == ParkourState_Wallrun)
			{
				ResetToBasicParkourState();
			}
			break;
		case TraceDirection_Left:
		case TraceDirection_Right:
			if (CurrentMovementState == ParkourState_Jump && IsFullfillingWallrunConditions())
			{
				SetParkourState(ParkourState_Wallrun);
			}
				break;
		case TraceDirection_Down:
			if (CurrentMovementState == ParkourState_Wallrun)
			{
				ResetToBasicParkourState();
			}
				break;
	}
}

void UParkourMovementComponent::OnDirectionOverlapEnd(TEnumAsByte<ETraceDirection> TraceDirection)
{
	switch (TraceDirection) {
	default:
		break;
	case TraceDirection_Left:
	case TraceDirection_Right:
		if (CurrentMovementState == ParkourState_Wallrun && !DirectionTraceHitResults.Contains(TraceDirection_Left) && !DirectionTraceHitResults.Contains(TraceDirection_Right))
		{
			ResetToBasicParkourState();
		}
			break;
	}
}

void UParkourMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
}

bool UParkourMovementComponent::DoJump(bool bReplayingMoves)
{
	switch (CurrentMovementState) {
	case ParkourState_Walk:
		if (CharacterOwner && CharacterOwner->CanJump())
		{
			// Don't jump if we can't move up/down.
			if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
			{
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
				SetMovementMode(MOVE_Falling);
				return true;
			}
		}
		return false;
	case ParkourState_Crawl:
		if (CharacterOwner && CharacterOwner->CanJump())
		{
			// Don't jump if we can't move up/down.
			if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
			{
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * 2);
				SetMovementMode(MOVE_Falling);
				return true;
			}
		}
		return false;
	case ParkourState_Hang: 
		if (CurrentHangingState != Hanging) { return false; }
		FinishHang();
		if (GetLastUpdateRotation().Equals(PawnOwner->GetControlRotation(), 90.f))
		{
			Velocity.X = 0;
			Velocity.Y = 0;
			Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * 1.5f);
		}
		else
		{
			Lunge();
		}
		SetMovementMode(MOVE_Falling);
		return true;
	case ParkourState_Wallrun:
		{
			StartNoWallrunTimer();
			Lunge();
		}
		SetMovementMode(MOVE_Falling);
		return true;
	default:
		return false;
	}

}

bool UParkourMovementComponent::CanAttemptJump() const
{
	return true;
}

void UParkourMovementComponent::Lunge()
{
	FRotator CurrentRotation = PawnOwner->GetControlRotation();
	FRotator ClampedRotation = FRotator(FMath::Clamp(CurrentRotation.Pitch, -30.f, 5.f), CurrentRotation.Yaw, CurrentRotation.Roll);
	Velocity = ClampedRotation.RotateVector(FVector(1000.0, 0, FMath::Max(Velocity.Z, (JumpZVelocity * 0.5f))));
}

bool UParkourMovementComponent::IsFullfillingWallrunConditions()
{
	if (!GetWorld()->GetTimerManager().IsTimerActive(WallrunTimerHandle)) { return false; }
	if (GetWorld()->GetTimerManager().IsTimerActive(NoWallrunTimerHandle)) { return false; }
	
	FVector CurrentInputVector = GetLastInputVector();
	
	bool bHasEnoughInput = abs(GetLastInputVector().X + GetLastInputVector().Y) > 0;
	if (!bHasEnoughInput) { return false; }

	TEnumAsByte<ETraceDirection> PotentialWallrunSide = DirectionTraceHitResults.Contains(TraceDirection_Left) ? TraceDirection_Left : TraceDirection_Right;
	FHitResult * PotentiallyRunnableWallHitResult = DirectionTraceHitResults.Find(PotentialWallrunSide);
	if (!PotentiallyRunnableWallHitResult) { return false; }
	
	FRotator CurrentRotation = (UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), PotentiallyRunnableWallHitResult->ImpactNormal)) + FRotator(0, (PotentialWallrunSide == TraceDirection_Left) ? -90 : 90, 0);
	FVector UnrotatedVelocity = CurrentRotation.UnrotateVector(Velocity);

	bool bIsHorizontallySteady = (UnrotatedVelocity.Y * ((PotentialWallrunSide == TraceDirection_Left) ? 1 : -1)) <= 900;
	if (!bIsHorizontallySteady) { return false; }
	
	bool bHasEnoughtForwardMotion = (UnrotatedVelocity.X) >= 1;
	if (!bHasEnoughtForwardMotion) { return false; }

	return true;
}

bool UParkourMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround();
}

bool UParkourMovementComponent::IsCrouching() const
{
	float CurrentHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	float NormalHeight = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	return abs(CurrentHeight - NormalHeight) > 1;
}

bool UParkourMovementComponent::GetIsTouchingLeftWall()
{
	return DirectionTraceHitResults.Contains(TraceDirection_Left);
}

bool UParkourMovementComponent::CanCrouchInCurrentState() const
{
	if (!CanEverCrouch())
	{
		return false;
	}

	return (IsFalling() || (CurrentMovementState != ParkourState_Hang) && (CurrentMovementState != ParkourState_Wallrun) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics());
}

