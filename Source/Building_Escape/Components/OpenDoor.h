// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/TriggerVolume.h"
#include "OpenDoor.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BUILDING_ESCAPE_API UOpenDoor : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UOpenDoor();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void InterpolateRotation(float& DeltaTime);
	bool bShouldBeOpened = false;
	float TargetYaw;
	float InitialYaw;
	bool bDelayActive;
	float DelayStartTime = 0.f;

	void UpdateTargetRotation();
	UFUNCTION(BlueprintCallable)
	void ToggleShouldBeOpened(bool bNewShouldBeOpened);
	UPROPERTY(EditAnywhere, Category = "Range")
	float MaxYaw = 90.f;
	UPROPERTY(EditAnywhere, Category = "Speed")
	float OpeningVelocity = 1.f;
	UPROPERTY(EditAnywhere, Category = "Speed")
	float ClosingVelocity = 1.f;
	UPROPERTY(EditAnywhere, Category = "Speed")
	float MinimumTimeBeingOpen = 1.f;


		
};
