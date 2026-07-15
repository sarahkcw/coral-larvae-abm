#pragma once
#include <cassert>
#include <cmath>
#include "CoreMinimal.h"
#include "LarvalSensorBaseComponent.h"
#include "Sensors.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API UAgeSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		return FMath::Clamp(UpdateParams.Age / 10000.f, 0.0f, 1.0f);
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UEnergySensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		const float InitialEnergy = FMath::Max(SensorParams.InitialEnergyResources, 1e-6f);
		const float NormalizedEnergy = UpdateParams.Energy / InitialEnergy;
		return FMath::Clamp(NormalizedEnergy, 0.0f, 1.0f);
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UOscillatorSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		const auto Period = UpdateParams.OscillationPeriod;
		const float Phase = (UpdateParams.SimulationStep % Period) / static_cast<float>(Period);
		float Factor = -std::cos(Phase * 2.0f * 3.1415927f);
		assert(Factor >= -1.0f && Factor <= 1.0f);
		Factor += 1.0f;    // Convert to 0.0..2.0
		Factor /= 2.0;     // Convert to 0.0..1.0
		return FMath::Clamp(Factor, 0.0f, 1.0f);
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UStressSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		// Add other stress factors e.g. temperature, salinity ?
		const float NewStressLevel = (UpdateParams.Age * (1.0f - UpdateParams.Energy)) / 2.0f;
		return FMath::Clamp(NewStressLevel, 0.0f, 1.0f);
	}	
};
