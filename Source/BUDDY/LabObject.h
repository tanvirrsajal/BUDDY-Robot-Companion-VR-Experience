// LabObject.h
// Any interactive object in the lab that BUDDY can explain.
// Place BP_LabObject in the level, set its name and description,
// and BUDDY will explain it when the player approaches and presses trigger.
// After the introduction, the player can ask follow-up questions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LabObject.generated.h"

UCLASS()
class BUDDY_API ALabObject : public AActor
{
	GENERATED_BODY()

public:
	ALabObject();

	// The mesh of this object (assign in Blueprint Details)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UStaticMeshComponent* Mesh;

	// Glowing highlight ring around object when player is nearby
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UPointLightComponent* HighlightLight;

	// Sphere that detects when player is close enough to interact
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class USphereComponent* InteractZone;

	// -------------------------------------------------------
	// SET THESE IN THE BLUEPRINT DETAILS PANEL
	// for each object you place in the level
	// -------------------------------------------------------

	// Short name shown in the interaction prompt
	// Example: "Robot Arm", "LiDAR Sensor", "Drone Frame"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lab Object")
	FString ObjectName = TEXT("Lab Object");

	// Longer description giving BUDDY context
	// Example: "a 6-DOF industrial robot arm mounted on a workbench"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lab Object")
	FString ObjectDescription = TEXT("an interesting piece of robotics equipment");

	// Follow-up questions the player can ask about this specific object
	// These appear as buttons after BUDDY's initial explanation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lab Object")
	TArray<FString> FollowUpQuestions = {
		TEXT("How does it work?"),
		TEXT("Where is this used in real life?"),
		TEXT("How much does one cost?"),
		TEXT("What are its limitations?")
	};

	// Color of the highlight light (set in Details to match object type)
	// Robot arm = orange, Sensor = blue, Circuit = green, Drone = cyan
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lab Object")
	FLinearColor HighlightColor = FLinearColor(1.0f, 0.6f, 0.1f);

	// Whether player is currently in range
	UPROPERTY(BlueprintReadOnly, Category="Lab Object")
	bool bPlayerNearby = false;

	// Called from Blueprint when player is nearby and presses Interact
	// This triggers BUDDY to explain the object
	UFUNCTION(BlueprintImplementableEvent, Category="Lab Object")
	void OnPlayerInteract();

	// Called from Blueprint when player selects a follow-up question
	UFUNCTION(BlueprintImplementableEvent, Category="Lab Object")
	void OnFollowUpSelected(int32 Index);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
