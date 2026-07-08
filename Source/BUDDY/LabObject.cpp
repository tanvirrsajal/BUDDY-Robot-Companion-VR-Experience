// LabObject.cpp

#include "LabObject.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"

ALabObject::ALabObject()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Object mesh
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	// Highlight glow light — starts dim, brightens when player is near
	HighlightLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HighlightLight"));
	HighlightLight->SetupAttachment(Root);
	HighlightLight->SetRelativeLocation(FVector(0, 0, 60));
	HighlightLight->SetIntensity(0.0f); // Off by default
	HighlightLight->SetAttenuationRadius(200.0f);

	// Interaction zone
	InteractZone = CreateDefaultSubobject<USphereComponent>(TEXT("InteractZone"));
	InteractZone->SetupAttachment(Root);
	InteractZone->SetSphereRadius(220.0f);
	InteractZone->SetCollisionProfileName(TEXT("Trigger"));
}

void ALabObject::BeginPlay()
{
	Super::BeginPlay();

	// Set the highlight light color from the property
	HighlightLight->SetLightColor(HighlightColor);

	// Bind overlap events
	InteractZone->OnComponentBeginOverlap.AddDynamic(this, &ALabObject::OnOverlapBegin);
	InteractZone->OnComponentEndOverlap.AddDynamic(this, &ALabObject::OnOverlapEnd);
}

void ALabObject::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	// Check it's a pawn AND it's player-controlled (not BUDDY or other AI)
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (OtherPawn && OtherPawn->IsPlayerControlled())
	{
		bPlayerNearby = true;
		HighlightLight->SetIntensity(800.0f);
	}
}

void ALabObject::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;

	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (OtherPawn && OtherPawn->IsPlayerControlled())
	{
		bPlayerNearby = false;
		HighlightLight->SetIntensity(0.0f);
	}
}
