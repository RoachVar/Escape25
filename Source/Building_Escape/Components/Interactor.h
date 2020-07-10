// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactor.generated.h"

class UInteractable;
class UBoxComponent;
class UPhysicsHandleComponent;
class UPhysicsConstraintComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class BUILDING_ESCAPE_API UInteractor : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractor();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	
	UPhysicsHandleComponent* PhysicsHandle = nullptr;
	
	APlayerController * OwningPlayerController = nullptr;
	UInputComponent* InputComponent = nullptr;
	UBoxComponent* CollisionMesh = nullptr;
	UInteractable* FocusedInteractable = nullptr;
	UInteractable* HeldGrabbable = nullptr;
	float LowestScreenDistance;
	bool bIsAnInteractableFocused = false;
	
	void FindAndAssignPhysicsHandle();
	void BindInputs();

	float GetDistanceToViewportCentre(FVector EvaluatedLocation);
	void EvaluateInteractables(TArray<UInteractable*>InteractablePool, FVector& ViewpointLocation);
	bool CheckIfInteractrableCanBeTraced(UInteractable* EvaluatedInteractable, FVector& ViewpointLocation) const;
	void SetFocusedInteractable(UInteractable* InteractableToFocus);
	void RefreshMarkedInteractables(TArray<UInteractable*>& NewMarkedInteractables);

	TArray<UInteractable*> OverlappedInteractables;
	TArray<UInteractable*> MarkedInteractables;
	
	void InitiateInteraction();
	void TerminateInteraction();
	void UpdateViewportScale(int32 CurrentViewportX);


	//Functions triggered by the
	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	

	//Properties defined by the designer are squared in runtime(during Begin Play) to enable more optimal calculation
	UPROPERTY(EditAnywhere, DisplayName = "Grab range", Category = "Grabber parameters")
	float Range = 190;
	float RangeSquared = 0;

	UPROPERTY(EditAnywhere, DisplayName = "Marking Range Extension", Category = "Grabber parameters")
	float MarkRange= 190;
	float MarkRangeSquared = 0;

	UPROPERTY(EditAnywhere, DisplayName = "Max distance from screen centre", Category = "Grabber parameters")
	float ScreenRange = 290;
	float ScreenRangeSquared = 0;

	int32 CachedViewportX = 0;
	float CachedViewportScale = 0;
};
