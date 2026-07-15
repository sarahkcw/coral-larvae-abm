#include "DataGrid/DebugControlSphere.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"

ADebugControlSphere::ADebugControlSphere()
{
	PrimaryActorTick.bCanEverTick = true;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	SphereComponent->InitSphereRadius(Radius);
	SphereComponent->SetCollisionProfileName(TEXT("Trigger"));
	SphereComponent->SetMobility(EComponentMobility::Movable);  // Set the component to be movable
	RootComponent = SphereComponent;
}

void ADebugControlSphere::SetRadius(float NewRadius)
{
	Radius = NewRadius;
	SphereComponent->SetSphereRadius(Radius);
}

void ADebugControlSphere::BeginPlay()
{
	Super::BeginPlay();
	SphereComponent->SetSphereRadius(Radius);
}

void ADebugControlSphere::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SphereComponent->SetSphereRadius(Radius);
}
