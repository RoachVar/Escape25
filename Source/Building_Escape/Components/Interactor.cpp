// Copyright Roch Karwacki 2020


//#include "GameFramework/PlayerController.h"
#include "Interactor.h"
#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "GameFramework/PlayerController.h"
#include "Interactable.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"



// Sets default values for this component's properties
UInteractor::UInteractor()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	//Creating the collision box so it lays flat on it's side. Setting up its visibility and collision parameters
	CollisionMesh = CreateDefaultSubobject<UBoxComponent>(FName("Collision Mesh"));
	CollisionMesh->AddLocalRotation(FRotator(90.f, 0, 0));
	CollisionMesh->SetVisibility(true); CollisionMesh->SetHiddenInGame(true);
	CollisionMesh->SetCollisionProfileName("Trigger"); CollisionMesh->UpdateCollisionProfile();
	CollisionMesh->SetGenerateOverlapEvents(true);

	// ...
}

// Called when the game starts
void UInteractor::BeginPlay()
{
	Super::BeginPlay();

	//Finding the player controller and assigning it to a member variable
	OwningPlayerController = (APlayerController*)((APawn*)GetOwner())->GetController();

	//Calculating internal, real values of distance varioables based on the designer-friendly variables

	RangeSquared = FMath::Pow(Range, 2);
	MarkRangeSquared = FMath::Pow(MarkRange, 2);
	ScreenRangeSquared = FMath::Pow(ScreenRange, 2);

	//Attaching the collision box to the actor that owns this component. Setting its size and offseting it so it is attached fully in front of the actor.
	CollisionMesh->AttachToComponent(this->GetAttachParent(), FAttachmentTransformRules::KeepRelativeTransform);
	CollisionMesh->SetBoxExtent(FVector(40, 80, (Range + MarkRange)/2));
	CollisionMesh->AddLocalOffset(FVector(0, 0, -(Range + MarkRange)/2));

	//Binding member functions to the collision box's overlap delegates
	CollisionMesh->OnComponentBeginOverlap.AddDynamic(this, &UInteractor::OnOverlapBegin);
	CollisionMesh->OnComponentEndOverlap.AddDynamic(this, &UInteractor::OnEndOverlap);
	
	//Finding  a physics handle that should be found on the actor and storing a pointer to it as a member variable. Binding input functions to delegates.
	FindAndAssignPhysicsHandle();
	BindInputs();
	// ...
	
}

void UInteractor::FindAndAssignPhysicsHandle()
{
	PhysicsHandle = GetOwner()->FindComponentByClass<UPhysicsHandleComponent>();
	if (!PhysicsHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("No Physics Handle component on actor %s!"), *GetOwner()->GetName())
	}
}

void UInteractor::BindInputs()
{

	InputComponent = GetOwner()->FindComponentByClass<UInputComponent>();
	if (InputComponent)
	{

		InputComponent->BindAction("Grab", IE_Pressed, this, &UInteractor::InitiateInteraction);
		InputComponent->BindAction("Grab", IE_Released, this, &UInteractor::TerminateInteraction);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No input component on actor %s!"), *GetOwner()->GetName());
	}
}


// Called every frame
void UInteractor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (HeldGrabbable)
	{
		if (MarkedInteractables.Num() > 0)
		{
			TArray<UInteractable*> Nullptr;
			RefreshMarkedInteractables(Nullptr);
		}

		PhysicsHandle->SetTargetLocation(GetComponentLocation());

		return;
	}
	

	FVector ViewpointLocation;
	FRotator ViewpointRotation;
	OwningPlayerController->GetPlayerViewPoint
		(
		OUT ViewpointLocation,
		OUT ViewpointRotation
		);
	EvaluateInteractables(OverlappedInteractables, ViewpointLocation);
	//}
}

void UInteractor::InitiateInteraction()
{
	if (FocusedInteractable)
	{
		FocusedInteractable->StartInteraction();

		if (false == true && FocusedInteractable->CurrentInteractionType == Hold)
		{
			if (!PhysicsHandle) { return; }
			HeldGrabbable = FocusedInteractable;
			PhysicsHandle->GrabComponentAtLocation
			(
				HeldGrabbable->GetOwningPrimitive(),
				NAME_None,
				GetOwner()->GetRootComponent()->GetComponentLocation()
			);
			PhysicsHandle->SetTargetLocation(GetComponentLocation());
			SetFocusedInteractable(nullptr);
		}
	}

	//UE_LOG(LogTemp, Error, TEXT("PICKED UP"));
}

void UInteractor::TerminateInteraction()
{
	if (HeldGrabbable)
	{
		PhysicsHandle->ReleaseComponent();
		HeldGrabbable = nullptr;
		return;
	}
	
	if (FocusedInteractable)
	{
		FocusedInteractable->EndInteraction();
	}
}

float UInteractor::GetDistanceToViewportCentre(FVector EvaluatedLocation)
{
	//Converting the given world position to screen position
	
	FVector2D ActorScreenLocation;
	OwningPlayerController->ProjectWorldLocationToScreen(EvaluatedLocation, OUT ActorScreenLocation, false);

	//Getting the size of the players viewport and finding it's middle
	int32 ViewportSizeX, ViewportSizeY;
	OwningPlayerController->GetViewportSize(OUT ViewportSizeX, ViewportSizeY);
	FVector2D ViewportSize(ViewportSizeX, ViewportSizeY);
	FVector2D MiddleOfViewport(FVector2D(ViewportSize.X / 2, ViewportSize.Y / 2));
	
	//Checking if  the viewport size changed since this function was last performed; if it did, a new value will be calculated for the CachedViewportScale member variable.
	UpdateViewportScale(ViewportSizeX);

	//Measuring the distance between the middle of the screen and the given screen position
	//The resulting vector magnitude is scaled by the Cached Viewport Scale variable to make the distance consistent regardless of the screen resolution
	return CachedViewportScale * (ActorScreenLocation - MiddleOfViewport).SizeSquared();
}

void UInteractor::SetFocusedInteractable(UInteractable* InteractableToFocus)
{
	//If an interactable is already focused but it isn't the one that was passed into this function, it is informed that it is losing it's focus
	if (bIsAnInteractableFocused && (FocusedInteractable != InteractableToFocus))
	{
		FocusedInteractable->SetIsFocused(false);
	}
	
	//The member variable that defines which interactable is currently focus is set to the value that was just passed in, even if it was null
	FocusedInteractable = InteractableToFocus;

	//If the value passed in wasn't null, the interactable it pointed to is informed of its focused status and the distance.
	//The bool that tells if an interactable is currently focused is set to false if a null pointer was passed in and to true if a valid pointer was passed in.
	if (FocusedInteractable)
	{
		FocusedInteractable->SetIsFocused(true);
		bIsAnInteractableFocused = true;
	}
	else
	{
		bIsAnInteractableFocused = false;
	}
	
}

void UInteractor::EvaluateInteractables(TArray<UInteractable*>InteractablePool, FVector& ViewpointLocation)
{
	//Declare and initialise the variable that temporary stores the interactable component that will be focused
	UInteractable* FocusCandidate = nullptr;

	//Declare an array for interactables that are are both in range and traceable
	TArray<UInteractable*> InteractablesToMark;

	//Declre a float that will store the currently smallest(in terms of 2D screen space) distance to valid interactable for the duration of the evaluation process
	float CandidateScreenDistance = LowestScreenDistance;
	
	//Iterate through all of the interactables passed into the function
	for (UInteractable* IteratedInteractable :InteractablePool)
	{
		//Skipping inactive interactables altogether
		if (!IteratedInteractable->GetIfInteractableIsActive()) {continue;}
		
		//A trace is performed from between the given viewpoint location and the currently iterated interactable. If it can't be traced it isn't evaluated further.
		bool bCanBeTraced = CheckIfInteractrableCanBeTraced(IteratedInteractable, ViewpointLocation);
		if (bCanBeTraced)
		{
			//If the interactable passed the tracing test, it is added to the array of interactables that will be marked
			InteractablesToMark.AddUnique(IteratedInteractable);

			//If distance between the given location and the interactable fits inside the desired Range, it is evaluated further as a candidate for focusing
			if ((ViewpointLocation - IteratedInteractable->GetComponentLocation()).SizeSquared() <= RangeSquared)
			{
				//Checking how far is the evaluated component from the centre of the players' screen
				float ScreenDistance = GetDistanceToViewportCentre(IteratedInteractable->GetComponentLocation());

				//If the object is within the predefined range, and closer to the centre than other actors tested so far, it is set as the current best candidate for getting focus
				if (ScreenDistance <= ScreenRangeSquared && (!FocusCandidate || !FocusedInteractable ||
					ScreenDistance < CandidateScreenDistance))
				{
					//The iterated interactable is set as a candidate for focusing and the 2D distance to it is set as current best.
					//Note that these variables might be overriden several times during a single evaluation
					CandidateScreenDistance = ScreenDistance;
					FocusCandidate = IteratedInteractable;
				}
			}

		}

	}

	//The marked interactables are updated; the interactables that are stored as marked inside the appropriate member variable but won't be passed in into the function below will be unmarked
	RefreshMarkedInteractables(InteractablesToMark);

	//If a viable candidate was chosen inside the for loop, it will be set as the new focused object. If there wasn't, the focused object is set to none
	if (FocusCandidate)
	{
		SetFocusedInteractable(FocusCandidate);
		LowestScreenDistance = CandidateScreenDistance;
	}
	else
	{
		SetFocusedInteractable(nullptr);
	}

}

void UInteractor::RefreshMarkedInteractables(TArray<UInteractable*>& InteractablesToMark)
{

	//An internal copy of the member variable is made to prevent modyfing the array while it is being iterated on or modified
	TArray<UInteractable*> PreviouslyMarkedInteractables = MarkedInteractables;
	
	//Each element that was previously marked but wasn't passed in to this function is unmarked and removec from the member variable
	for (UInteractable* IteratedInteractable : PreviouslyMarkedInteractables)
	{
		if (!InteractablesToMark.IsValidIndex(0) || !InteractablesToMark.Contains(IteratedInteractable))
		{
			IteratedInteractable->UnmarkInteractable();
			MarkedInteractables.Remove(IteratedInteractable);
		}
	}

	//The interactables in the array that was passed in are all set to marked and added to the appropriate member variable(if they weren't already in it
	for (UInteractable* IteratedInteractable : InteractablesToMark)
	{
		IteratedInteractable->MarkInteractable();
		MarkedInteractables.AddUnique(IteratedInteractable);
	}
}

bool UInteractor::CheckIfInteractrableCanBeTraced(UInteractable* EvaluatedInteractable, FVector& ViewpointLocation) const
{
	//Declaring variables needed for the trace
	FVector LineTraceEnd = EvaluatedInteractable->GetComponentLocation();
	FCollisionQueryParams TraceParams(FName(TEXT("")), false, GetOwner());
	FHitResult HitResult;
	
	//Performing the trace
	GetWorld()->LineTraceSingleByObjectType
	(
		OUT HitResult,
		ViewpointLocation,
		LineTraceEnd,
		FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic),
		TraceParams
	);

	//Returning true if trace didn't hit anything or if it hit the actor that owns the interactable that was passed it
	return (!HitResult.bBlockingHit || (HitResult.GetActor() && HitResult.GetActor() == EvaluatedInteractable->GetOwner()));
}

void UInteractor::OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//If the actor that entered the collision box has an interactable component, the component is added to the appropriate member variable array
	UInteractable* NewInteractable = OtherActor->FindComponentByClass<UInteractable>();
	if (OtherActor && NewInteractable)
	{
		OverlappedInteractables.AddUnique(NewInteractable);
	}
	return;
}

void UInteractor::OnEndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	//If the actor that left the collision box has an interactable component, the component is removed from the appropriate member variable array
	UInteractable* ExInteractable = OtherActor->FindComponentByClass<UInteractable>();
	if (OtherActor && ExInteractable)
	{
		ExInteractable->UnmarkInteractable();
		OverlappedInteractables.Remove(ExInteractable);
	}
	return;
}

void UInteractor::UpdateViewportScale(int32 CurrentViewportX)
{
	// If the variable that was passed in is equal to the CachedViewportX member variable, no update is needed as the resolution didn't change

	if (CachedViewportX == CurrentViewportX)
	{
		return;
	}

	//Cached viewport scale(multiplicator) is calculated based on dividing by the full HD horizontal dimensionl. The value that was passed in is set as the new Cached Viewport X
	CachedViewportScale = abs(FMath::Pow((float)CurrentViewportX / 1920, -1));
	CachedViewportX = CurrentViewportX;

	return;
}

