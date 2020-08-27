// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactor.generated.h"

class UInteractable;
class UBoxComponent;
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

	APlayerController * OwningPlayerController = nullptr;
	UInputComponent* InputComponent = nullptr;
	UBoxComponent* CollisionMesh = nullptr;
	UInteractable* FocusedInteractable = nullptr;
	float LowestScreenDistance;
	bool bIsAnInteractableFocused = false;

	void BindInputs();

	//Functions automatically bound to the collision dispatchers of the main testing collider.
	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	//Each time a new Interactable enters the testing collider, it is added to the OverlappedInteractables array
	TArray<UInteractable*> OverlappedInteractables;
	
	//Objects in the array are evaluated each tick
	void EvaluateInteractables(TArray<UInteractable*>InteractablePool, FVector& ViewpointLocation);
	//The following tests are applied to each object inside the array inside the EvaluateInteractables function
	float GetDistanceToViewportCentre(FVector EvaluatedLocation);
	bool CheckIfInteractrableCanBeTraced(UInteractable* EvaluatedInteractable, FVector& ViewpointLocation) const;
	//The interactable that passes all the test is focused
	void SetFocusedInteractable(UInteractable* InteractableToFocus);
	//All the others that are in range and traceable are marked
	TArray<UInteractable*> MarkedInteractables;
	void RefreshMarkedInteractables(TArray<UInteractable*>& NewMarkedInteractables);
	
	//These functions are triggered by player inputs and call functions on the focused objects if there is one
	void InitiateInteraction();
	void TerminateInteraction();

	// Calculating ViewportScale involves exponentiation, so a cached value will be used as long as the current X dimension is equal to CachedViewportX to avoid performance issues
	void UpdateViewportScale(int32 CurrentViewportX);
	int32 CachedViewportX = 0;
	float CachedViewportScale = 0;

	//Properties defined by the designer are squared in runtime(during Begin Play) to enable more optimal calculation. The designer-assigned variables remain intact to avoid potential confusion
	UPROPERTY(EditAnywhere, DisplayName = "Grab range", Category = "Grabber parameters")
	float Range = 190;
	float RangeSquared = 0;

	UPROPERTY(EditAnywhere, DisplayName = "Marking Range Extension", Category = "Grabber parameters")
	float MarkRange= 190;
	float MarkRangeSquared = 0;

	UPROPERTY(EditAnywhere, DisplayName = "Max distance from screen centre", Category = "Grabber parameters")
	float ScreenRange = 290;
	float ScreenRangeSquared = 0;

};
