// Copyright Roch Karwacki 2020


#include "ParkourMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
//UParkourMovementComponent::UParkourMovementComponent()
//{
// 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
//	PrimaryComponentTick.bCanEverTick = true;
//	PrimaryComponentTick.bStartWithTickEnabled = true;
//}

// Called when the game starts or when spawned
void UParkourMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);
	PawnsRootPrimitive = (UCapsuleComponent*)(GetOwner()->GetRootComponent());
	PawnsMovementComponent = (UPawnMovementComponent*)GetOwner()->GetComponentByClass(UPawnMovementComponent::StaticClass());
	PawnsRootPrimitive->GetUnscaledCapsuleSize(OUT CapsuleRadius, OUT CapsuleHalfHeight);
}

// Called every frame
void UParkourMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentHangingState != Hanging) { return; }
	//Setting current velocity to the precisely the value of movement input, effectively nullifying outside influence without having to disable physics simulation(which causes bugs)
	PawnsRootPrimitive->SetAllPhysicsLinearVelocity(PawnsMovementComponent->GetLastInputVector());
	//Checking if the player is currently on either edge of the current plane. If so, a collider box that enforces appropriate behaviour(blocking or traversing a corner) will be spawned
	UpdateEdgeStatuses();

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
	float XOffset = (bTestForOuterEdge ? 3 : -1) * CapsuleRadius;
	float YOffset = bTestForOuterEdge ? (bIsEdgeToTheRight ? 1 : -1) * CapsuleRadius : 0;
	FVector OffsetLocation = GetOwner()->GetActorRotation().RotateVector(FVector(XOffset, YOffset, 0)) + GetActorLocation();

	FVector HangLocation(0, 0, 0);
	FRotator HangRotation(0, 0, 0);

	if (IsValidHangPoint(OUT HangLocation, OUT HangRotation, OffsetLocation, OffsetRotation))
	{
		OutTransform = FTransform(HangRotation, HangLocation, FVector(1, 1, 1));
		//FString DirectionString = bIsEdgeToTheRight ? TEXT("Right") : TEXT("Left");
		//FString EdgeTypeString = bTestForOuterEdge ? TEXT("Outer") : TEXT("Inner");
		//UE_LOG(LogTemp, Warning, TEXT("%s edge detected - %s edge"), *DirectionString, *EdgeTypeString);
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
	
	//The member variable that stores the current state is set to the value that was passed in
	CurrentHangingState = NewHangingState;
	
	//Futher processes are carried out depending on what the new state is
	switch (CurrentHangingState) {
	case NotHanging:
		TogglePlaneLock(false);
		PawnsRootPrimitive->SetEnableGravity(true);
		Reset();
		break;
	case AdjustingLocation:
		GetOwner()->DisableInput(GetWorld()->GetFirstPlayerController());
		PawnsRootPrimitive->SetEnableGravity(false);
		PawnsRootPrimitive->SetAllPhysicsLinearVelocity(FVector(0, 0, 0));
	case TraversingACorner:
		Reset();
		break;
	case Hanging:
		TogglePlaneLock(true);
		GetOwner()->EnableInput(GetWorld()->GetFirstPlayerController());
		break;
	
	}
}

void UParkourMovementComponent::AdjustmentEnded()
{
	ChangeHangingState(Hanging);
}

void UParkourMovementComponent::TogglePlaneLock(bool bNewIsLocked) const
{
	if (bNewIsLocked)
	{
		PawnsMovementComponent->SetPlaneConstraintFromVectors(FRotator(0,90,9).RotateVector(GetOwner()->GetActorForwardVector()), FVector(0, 0, 1));
		PawnsMovementComponent->SetPlaneConstraintEnabled(true);
		PawnsRootPrimitive->SetConstraintMode(EDOFMode::XYPlane);
	}
	else
	{
		PawnsMovementComponent->SetPlaneConstraintEnabled(false);
		PawnsRootPrimitive->SetConstraintMode(EDOFMode::None);
	}
}

void UParkourMovementComponent::FinishHang()
{
	ChangeHangingState(NotHanging);
}

void UParkourMovementComponent::SpawnEdgeCollider(bool bIsRightEdge, FTransform SpawnTransform)
{
	//Creating a collider and setting its extent
	UBoxComponent * NewColliderObject = NewObject<UBoxComponent>(this);
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
	
	//Registering the component and assigning it to the pointer depending on the bool that was passed into this function(that signifies which edge this collision will represent)
	NewColliderObject->RegisterComponentWithWorld(GetWorld());
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
		LeftCollider->UnregisterComponent();
		LeftCollider->DestroyComponent();
		LeftCollider = nullptr;
	}
	if (RightCollider)
	{
		RightCollider->UnregisterComponent();
		RightCollider->DestroyComponent();
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

void UParkourMovementComponent::SetMovementState(TEnumAsByte<EParkourMovementState> NewState)
{
	CurrentMovementState = NewState;
}

TEnumAsByte<EParkourMovementState> UParkourMovementComponent::GetMovementState()
{
	return CurrentMovementState;
}