// Copyright Roch Karwacki 2020

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MassTreshold.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMassChangedDelegate, bool, bNewState);

class UPrimitiveComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BUILDING_ESCAPE_API UMassTreshold : public USceneComponent
{
	GENERATED_BODY()


public:	
	// Sets default values for this component's properties
	UMassTreshold();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


private:
	UPrimitiveComponent* ParentComponent = nullptr;
	float TotalMass = 0;
	bool bIsAboveTreshold = false;

	void CheckMass();

	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UPROPERTY(EditAnywhere, DisplayName = "Treshold")
	float WeightTreshold;
	
	UFUNCTION()
	void OnEndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(BlueprintAssignable)
	FMassChangedDelegate ResultChanged;


};
