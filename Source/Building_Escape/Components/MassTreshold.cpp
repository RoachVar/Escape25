// Copyright Roch Karwacki 2020



#include "MassTreshold.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

// Sets default values for this component's properties
UMassTreshold::UMassTreshold()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called every frame
void UMassTreshold::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

// Called when the game starts
void UMassTreshold::BeginPlay()
{
	Super::BeginPlay();
	
	if(!(GetAttachParent()->IsA(UPrimitiveComponent::StaticClass())))
	{;
		UE_LOG(LogTemp, Error, TEXT("The Weight Totaler Component on actor %s isn't attached to a primitive!"), *GetOwner()->GetName());
		return;
	}
	
	ParentComponent = (UPrimitiveComponent*)GetAttachParent();
	
	ParentComponent->OnComponentBeginOverlap.AddDynamic(this, &UMassTreshold::OnOverlapBegin);
	ParentComponent->OnComponentEndOverlap.AddDynamic(this, &UMassTreshold::OnEndOverlap);

	// ...
	
}



void UMassTreshold::OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherComp->IsSimulatingPhysics())
	{
	TotalMass += OtherComp->GetMass();
	CheckMass();
	}
}

void UMassTreshold::OnEndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherComp->IsSimulatingPhysics())
	{
	TotalMass -= OtherComp->GetMass();
	CheckMass();
	}
}

void UMassTreshold::CheckMass()
{
	bool bCurrentResult = TotalMass >= WeightTreshold;
	if (bIsAboveTreshold != bCurrentResult)
	{
		bIsAboveTreshold = bCurrentResult;
		ResultChanged.Broadcast(bIsAboveTreshold);
	}
}


