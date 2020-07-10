// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "InteractionSystemLibrary.h"
#include "Interactable.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractableDelegate);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class BUILDING_ESCAPE_API UInteractable : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractable();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetIsFocused(bool bNewIsFocused);

	UPROPERTY(BlueprintAssignable)
	FInteractableDelegate InteractionStart;
	UPROPERTY(BlueprintAssignable)
	FInteractableDelegate InteractionEnd;
	UPROPERTY(BlueprintAssignable)
	FInteractableDelegate FocusStateChanged;
	UPROPERTY(EditAnywhere, Category = "Interaction", DisplayName = "Interaction Type")
	TEnumAsByte<EInteractionType> CurrentInteractionType = Use;
	UFUNCTION(BlueprintPure, Category = "Interaction", DisplayName = "Get Focus State")
	TEnumAsByte <EFocusState> GetFocusState() const;
	UFUNCTION(BlueprintPure, Category = "Interaction", DisplayName = "Get Interaction Type")
	TEnumAsByte<EInteractionType> GetInteractionType() const;

	UPrimitiveComponent * GetOwningPrimitive() const;

	void StartInteraction();
	void EndInteraction();
	void MarkInteractable();
	void UnmarkInteractable();

	UPROPERTY(EditAnywhere, Category = "Interaction", DisplayName = "Start activated")
	bool bIsActiveAtStart = true;
	UFUNCTION(BlueprintCallable, DisplayName = "Toggle Activity")
	void ToggleActivity(bool bNewIsActive);
	UFUNCTION(BlueprintCallable, DisplayName = "IS interactable active")
	bool GetIfInteractableIsActive();


private:
	TEnumAsByte <EFocusState> CurrentFocusState = InactiveState;
	UStaticMeshComponent* OwnersStaticMeshComponent;
	bool bIsMarked = false;
	bool bIsInteractableActive = true;
};
