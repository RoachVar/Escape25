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
	

}

// Called when the game starts or when spawned
void UParkourMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	MaxWallrunTime = 2;
	SetComponentTickEnabled(true);
	PawnsRootPrimitive = (UCapsuleComponent*)(GetOwner()->GetRootComponent());
	PawnsMovementComponent = (UPawnMovementComponent*)this;
	PawnsRootPrimitive->GetScaledCapsuleSize(OUT CapsuleRadius, OUT CapsuleHalfHeight);
	
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

// Called every frame
void UParkourMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentMovementState) {
	case MSE_Walk:
		UpdateBlockedDirections();
		break;
	case MSE_Jump:
		UpdateBlockedDirections();
		if (bHangingLocked) { return; }
		TryToHangInCurrentLocation();
		break;
	case MSE_Hang:
		if (CurrentHangingState != Hanging)
		{
			return;
		}
		else
		{

		}
		//Setting current velocity to the precisely the value of movement input, effectively nullifying outside influence without having to disable physics simulation(which causes bugs)
		//Velocity = GetLastInputVector();
		//Checking if the player is currently on either edge of the current plane. If so, a collider box that enforces appropriate behaviour(blocking or traversing a corner) will be spawned
		UpdateEdgeStatuses();
		break;
	case MSE_Wallrun:
		if (!IsFullfillingWallrunConditions())
		{
			SetMovementState(MSE_Jump);
		}
		else
		{
			bool bHeightBoostPossible = abs(JumpOffPoint.Z - LastUpdateLocation.Z) < 300;
			Velocity = GetOwner()->GetActorRotation().RotateVector(FVector(1200, BlockedDirections.Contains(TraceDirection_Left) ? -200 : 200, bHeightBoostPossible ? (JumpOffPoint.Z + JumpZVelocity) - LastUpdateLocation.Z : 0));
			UpdateBlockedDirections();
		}
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
		SetMovementState(MSE_Hang);
		ChangeHangingState(AdjustingLocation);
		AdjustLocationToHangPosition(HangLocation, HangRotation);
		return true;
	}
	else
	{
		ChangeHangingState(NotHanging);
		return false;
	}
}

void UParkourMovementComponent::SetParameters(FVector NewHandSize, float NewGrabHeight, float NewGrabbingReach, float NewAttachDistance, float NewAttachHeight)
{
	HandSize = NewHandSize;
	GrabHeight = NewGrabHeight;
	GrabbingReach = NewGrabbingReach;
	AttachDistance = NewAttachDistance;
	AttachHeight = NewAttachHeight;
}

void UParkourMovementComponent::AdjustLocationToHangPosition(FVector HangLocation, FRotator HangRotation)
{
	if (LocationAdjustment.IsBound())
	{
		LocationAdjustment.Broadcast(HangLocation, HangRotation);

	}
	else
	{
		GetOwner()->SetActorLocation(HangLocation);
		GetOwner()->SetActorRotation(HangRotation);
		AdjustmentEnded();
	}
}

void UParkourMovementComponent::AdjustLocationAroundCorner(FVector HangLocation, FRotator HangRotation)
{
	if (CornerAdjustment.IsBound())
	{
		CornerAdjustment.Broadcast(HangLocation, HangRotation);
	}
	else
	{
		GetOwner()->SetActorLocation(HangLocation);
		GetOwner()->SetActorRotation(HangRotation);
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
		GravityScale = 1;
		Reset();
		break;
	case AdjustingLocation:
		GetOwner()->DisableInput(GetWorld()->GetFirstPlayerController());
		GravityScale = 0;
		Velocity = FVector(0, 0, 0);
	case TraversingACorner:
		GetOwner()->SetActorEnableCollision(false);
		Reset();
		break;
	case Hanging:
		//if (IsValidHangPoint(OUT TargetLocation, OUT TargetRotation, LastUpdateLocation, LastUpdateRotation.Rotator()))
		//{
		//	GetOwner()->SetActorLocation(TargetLocation);
		//	GetOwner()->SetActorRotation(TargetRotation);
		//}
		//else
		//{
		//	SetMovementState(MSE_Jump);
		//}
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
		//PawnsRootPrimitive->SetConstraintMode(EDOFMode::XYPlane);
	}
	else
	{
		SetPlaneConstraintEnabled(false);
	}
}

void UParkourMovementComponent::FinishHang()
{
	SetMovementMode(MOVE_Falling);
	SetMovementState(MSE_Jump);
	ChangeHangingState(NotHanging);
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

	NewColliderObject->SetHiddenInGame(false);

}

void UParkourMovementComponent::EdgeOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//If the actor that collided with the collision box isn't the pawn that owns the LedgeHangingActor, this collision event is irrelevant; returning immediately.
	if (OtherActor != GetOwner()) { return; }
	
	//Declaring a pointer to the correct TargetTransform variable depending on which edge the collider that caused this event represents
	FTransform * TargetTransform = (OverlappedComp == LeftCollider) ? &LeftEdgeTargetTransform : &RightEdgeTargetTransform;
	
	//Beginning the transition around a corner
	ChangeHangingState(TraversingACorner);
	AdjustLocationAroundCorner(TargetTransform->GetLocation(), TargetTransform->GetRotation().Rotator());
}

void UParkourMovementComponent::Reset()
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

void UParkourMovementComponent::SetIsAirborn(bool bNewIsAirborn)
{
	bIsAirborn = bNewIsAirborn;
}

bool UParkourMovementComponent::GetIsAirborn()
{
	return bIsAirborn;
}

void UParkourMovementComponent::WallrunTimerStart()
{
	GetWorld()->GetTimerManager().SetTimer(WallrunTimerHandle, this, &UParkourMovementComponent::WallrunTimerEnd, MaxWallrunTime, false);
}

void UParkourMovementComponent::WallrunTimerEnd()
{
	GetWorld()->GetTimerManager().ClearTimer(WallrunTimerHandle);
	bCanWallrun = false;
	if (CurrentMovementState = MSE_Wallrun)
	{
		SetMovementState(MSE_Jump);
	}
}

void UParkourMovementComponent::SetMovementState(TEnumAsByte<EParkourMovementState> NewState)
{
	if (CurrentMovementState == NewState) { return; }
	CurrentMovementState = NewState;

	switch (NewState) {
	case MSE_Walk:
		bUseControllerDesiredRotation = true;
		GravityScale = 1;
		bCanWallrun = true;
		GetWorld()->GetTimerManager().ClearTimer(WallrunTimerHandle);
		break;
	case MSE_Jump:
		bUseControllerDesiredRotation = true;
		GravityScale = 1;
		if (!(GetWorld()->GetTimerManager().IsTimerActive(WallrunTimerHandle)) && !(GetWorld()->GetTimerManager().IsTimerPaused(WallrunTimerHandle)) && bCanWallrun)
		{
			WallrunTimerStart();
		}
		break;
	case MSE_Wallrun:
		bUseControllerDesiredRotation = false;
		GravityScale = 0;
		break;
	case MSE_Hang:
		bUseControllerDesiredRotation = false;
		bCanWallrun = true;
		SetMovementMode(MOVE_Flying);
		WallrunTimerHandle.Invalidate();
		break;
	default:
		bUseControllerDesiredRotation = true;
		GravityScale = 1;
		break;
	}

}

TEnumAsByte<EParkourMovementState> UParkourMovementComponent::GetMovementState()
{
	return CurrentMovementState;
}

void UParkourMovementComponent::StartNoHangTimer()
{
	bHangingLocked = true;
	GetWorld()->GetTimerManager().SetTimer(NoHangTimerHandle, this, &UParkourMovementComponent::EndNoHangTimer, 0.1f, false);
}

void UParkourMovementComponent::EndNoHangTimer()
{
	bHangingLocked = false;
}

void UParkourMovementComponent::StartNoWallrunTimer()
{
	GetWorld()->GetTimerManager().SetTimer(NoWallrunTimerHandle, this, &UParkourMovementComponent::EndNoWallrunTimer, 0.07, false);
}

void UParkourMovementComponent::EndNoWallrunTimer()
{
}

bool UParkourMovementComponent::TraceForBlockInDirection(TEnumAsByte<ETraceDirection> TraceDirection)
{
	FRotator TraceRotationOffset = TraceDirectionOffsets.FindRef(TraceDirection);
	float TraceLength = (TraceDirection == TraceDirection_Up || TraceDirection_Down ? CapsuleHalfHeight + CapsuleRadius : CapsuleRadius * 5);
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
	/*
	return GetWorld()->LineTraceTestByChannel(
		TraceStart,
		TraceStart + (GetOwner()->GetActorRotation() + TraceRotationOffset).RotateVector(FVector(TraceLength, 0, 0)),
		ECollisionChannel::ECC_WorldStatic,
		TraceParams
	); */
	

}

void UParkourMovementComponent::UpdateBlockedDirections()
{
	for (int DirectionIndex = 0; DirectionIndex < ETraceDirection::MAX; DirectionIndex++)
	{
		TEnumAsByte<ETraceDirection> TraceDirection = ETraceDirection(DirectionIndex);
		if (TraceForBlockInDirection(TraceDirection))
		{
			OnDirectionOverlap(TraceDirection);
			BlockedDirections.AddUnique(TraceDirection);
		}
		else
		{
			if (BlockedDirections.Contains(TraceDirection))
			{
				BlockedDirections.Remove(TraceDirection);
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
			if (CurrentMovementState == MSE_Wallrun)
			{
				SetMovementState(MSE_Jump);
			}
			break;
		case TraceDirection_Left:
		case TraceDirection_Right:
			if (CurrentMovementState == MSE_Jump && IsFullfillingWallrunConditions())
			{
				SetMovementState(MSE_Wallrun);
			}
				break;
		case TraceDirection_Down:
			if (CurrentMovementState == MSE_Wallrun)
			{
				SetMovementState(MSE_Jump);
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
		if (!BlockedDirections.Contains(TraceDirection_Left) && !BlockedDirections.Contains(TraceDirection_Right))
		{
			SetMovementState(MSE_Jump);
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
	case MSE_Walk:
		if (CharacterOwner && CharacterOwner->CanJump())
		{
			// Don't jump if we can't move up/down.
			if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
			{
				JumpOffPoint = LastUpdateLocation;
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
				SetMovementMode(MOVE_Falling);
				return true;
			}
		}
		return false;
	case MSE_Crawl:
		if (CharacterOwner && CharacterOwner->CanJump())
		{
			// Don't jump if we can't move up/down.
			if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
			{
				JumpOffPoint = LastUpdateLocation;
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * 2);
				SetMovementMode(MOVE_Falling);
				return true;
			}
		}
		return false;
	case MSE_Hang:
		if (CurrentHangingState != Hanging) { return false; }
		JumpOffPoint = LastUpdateLocation;
		FinishHang();
		SetMovementState(MSE_Jump);
		if (GetLastUpdateRotation().Equals(PawnOwner->GetControlRotation(), 90.f))
		{
			Velocity.X = 0;
			Velocity.Y = 0;
			Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * 1.5f);
		}
		else
		{
			Velocity = PawnOwner->GetControlRotation().RotateVector(FVector(FMath::Max(Velocity.X, (JumpZVelocity * 0.8f)), 0, FMath::Max(Velocity.X, (JumpZVelocity * -0.5f))));
		}
		SetMovementMode(MOVE_Falling);
		return true;
	case MSE_Wallrun:
		if (false && GetLastUpdateRotation().Equals(PawnOwner->GetControlRotation(), 50.f))
		{
			float JumpOffForce = BlockedDirections.Contains(TraceDirection_Left) ? JumpZVelocity : -JumpZVelocity;
			StartNoWallrunTimer();
			Velocity = PawnOwner->GetControlRotation().RotateVector(FVector(500, JumpOffForce * 3.8f, FMath::Max(Velocity.Z, (JumpZVelocity * 0.5f))));
		}
		else
		{
			float JumpOffForce = BlockedDirections.Contains(TraceDirection_Left) ? JumpZVelocity : -JumpZVelocity;
			StartNoWallrunTimer();
			Velocity = PawnOwner->GetControlRotation().RotateVector(FVector(1000.0, 0, FMath::Max(Velocity.Z, (JumpZVelocity * 1.5f))));
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

bool UParkourMovementComponent::IsFullfillingWallrunConditions()
{
	if (!bCanWallrun) {return false;}
	if (GetWorld()->GetTimerManager().IsTimerActive(NoWallrunTimerHandle)) { return false; }
	FVector CurrentInputVector = GetLastInputVector();
	bool bHasEnoughInput = abs(GetLastInputVector().X + GetLastInputVector().Y) > 0;
	if (!bHasEnoughInput) { return false; }
	TEnumAsByte<ETraceDirection> PotentialWallrunSide = BlockedDirections.Contains(TraceDirection_Left) ? TraceDirection_Left : TraceDirection_Right;
	FHitResult * PotentiallyRunnableWallHitResult = DirectionTraceHitResults.Find(PotentialWallrunSide);
	if (!PotentiallyRunnableWallHitResult) { return false; }
	FRotator CurrentRotation = (UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), PotentiallyRunnableWallHitResult->ImpactNormal)) + FRotator(0, (PotentialWallrunSide == TraceDirection_Left) ? -90 : 90, 0);

	bool bIsHorizontallySteady = (CurrentRotation.UnrotateVector(Velocity).Y * ((PotentialWallrunSide == TraceDirection_Left) ? 1 : -1)) <= 900;
	if (!bIsHorizontallySteady) { return false; }

	return true;
}

bool UParkourMovementComponent::IsMovingOnGround() const
{
	return true;
}

