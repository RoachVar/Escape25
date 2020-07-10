// Copyright Roch Karwacki 2020


#include "Interactable.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

// Sets default values for this component's properties
UInteractable::UInteractable()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UInteractable::BeginPlay()
{
	Super::BeginPlay();
	//Attempting to find the intended collision body Static Mesh; By design, it should be just above the component in the hierarchy;
	OwnersStaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
	if (!OwnersStaticMeshComponent)
	{
		OwnersStaticMeshComponent = (UStaticMeshComponent*)(GetOwner()->GetComponentByClass(UStaticMeshComponent::StaticClass()));
		if (!OwnersStaticMeshComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("Interactor found no static mesh on actor %s. The actor might not work properly!"), *GetOwner()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Interable component on %s isn't attached directly to a static mesh component! First found static mesh on the actor was chosen instead."), *GetOwner()->GetName());
		}
	}



	if (OwnersStaticMeshComponent->GetGenerateOverlapEvents())
	{
		//UE_LOG(LogTemp, Warning, TEXT("%s is already set to generate overlap events!"),*GetOwner()->GetName())
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("%s wasn't set to generate overlap events, setting now!"), *GetOwner()->GetName())
		OwnersStaticMeshComponent->SetGenerateOverlapEvents(true);
	}

	bIsInteractableActive = bIsActiveAtStart;
	// ...
	
}


// Called every frame
void UInteractable::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UInteractable::SetIsFocused(bool bNewIsFocused)
{
	if (bNewIsFocused && CurrentFocusState == FocusedState)
	{
		return;
	}

	if (!bNewIsFocused)
	{
		CurrentFocusState = bIsMarked ? MarkedState : InactiveState;
		FocusStateChanged.Broadcast();
		return;
	}

	CurrentFocusState = FocusedState;
	

	FocusStateChanged.Broadcast();
}

void UInteractable::StartInteraction()
{
	InteractionStart.Broadcast();
}

void UInteractable::EndInteraction()
{
	InteractionEnd.Broadcast();
}

UPrimitiveComponent * UInteractable::GetOwningPrimitive() const
{
	return OwnersStaticMeshComponent;
}

void UInteractable::MarkInteractable()
{
	if (CurrentFocusState == InactiveState)
	{
		bIsMarked = true;
		CurrentFocusState = MarkedState;
		FocusStateChanged.Broadcast();
	}
}

void UInteractable::UnmarkInteractable()
{
	if (CurrentFocusState != InactiveState)
	{
		bIsMarked = false;
		CurrentFocusState = InactiveState;
		FocusStateChanged.Broadcast();
	}
}

TEnumAsByte<EInteractionType> UInteractable::GetInteractionType() const
{
	return CurrentInteractionType;
}

TEnumAsByte <EFocusState> UInteractable::GetFocusState() const
{
	return CurrentFocusState;
}

void UInteractable::ToggleActivity(bool bNewIsActive)
{
	bIsInteractableActive = bNewIsActive;
	if (!bIsInteractableActive)
	{
		UnmarkInteractable();
	}
}

bool UInteractable::GetIfInteractableIsActive()
{
	return bIsInteractableActive;
}

