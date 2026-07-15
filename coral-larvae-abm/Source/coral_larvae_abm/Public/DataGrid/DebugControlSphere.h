#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "DebugControlSphere.generated.h"

UCLASS(Blueprintable)
class CORAL_LARVAE_ABM_API ADebugControlSphere : public AActor
{
	GENERATED_BODY()

public:
	ADebugControlSphere();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	USphereComponent* SphereComponent;

	UFUNCTION(BlueprintCallable, Category = "Debugging")
	float GetRadius() const { return SphereComponent->GetUnscaledSphereRadius(); }

	UFUNCTION(BlueprintCallable, Category = "Debugging")
	void SetRadius(float NewRadius);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(EditAnywhere, Category = "Debugging")
	float Radius = 10.f;
};
