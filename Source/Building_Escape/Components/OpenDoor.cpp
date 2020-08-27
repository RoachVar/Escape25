// Copyright Roch Karwacki 2020

#include "OpenDoor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
//#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UOpenDoor::UOpenDoor()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UOpenDoor::BeginPlay()
{
	Super::BeginPlay();
	bDelayActive = false;
	InitialYaw = GetAttachParent()->GetComponentRotation().Yaw;
	
	
}
 

// Called every frame
void UOpenDoor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	
	if (bDelayActive && abs(GetWorld()->GetTimeSeconds() - DelayStartTime) > MinimumTimeBeingOpen)
	{
		bDelayActive = false;
		UpdateTargetRotation();
	}
	
	InterpolateRotation(DeltaTime);

	// ...
}

void UOpenDoor::InterpolateRotation(float& DeltaTime)
{

	FRotator CurrentRotation = GetAttachParent()->GetComponentRotation();
	float NewYaw = FMath::FInterpTo(CurrentRotation.Yaw, TargetYaw, DeltaTime, bShouldBeOpened ? OpeningVelocity : ClosingVelocity);
	GetAttachParent()->SetWorldRotation(FRotator(CurrentRotation.Pitch, NewYaw, CurrentRotation.Roll), false, nullptr, ETeleportType::TeleportPhysics);
	CurrentRotation = GetAttachParent()->GetComponentRotation();

}

void UOpenDoor::ToggleShouldBeOpened(bool bNewShouldBeOpened)
{
	bShouldBeOpened = bNewShouldBeOpened;
	UpdateTargetRotation();
}

void UOpenDoor::UpdateTargetRotation()
{
	if (bShouldBeOpened || bDelayActive)
	{
		TargetYaw = InitialYaw + MaxYaw;
		DelayStartTime = GetWorld()->GetTimeSeconds();
		bDelayActive = true;
	}
	else
	{

		TargetYaw = InitialYaw;
	}
}