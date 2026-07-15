#pragma once
#include "CoreMinimal.h"
#include "LarvalSensorBaseComponent.h"
#include "..\DataGrid\DataGridUtils.h"
#include "LightSensors.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API ULightIntensitySensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentLightIntensityValue = Cell.Data.LightData.LightIntensity;
		return FMath::Clamp(CurrentLightIntensityValue, 0.0f, 1.0f);
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API ULightWavelengthSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentLightWavelengthValue = Cell.Data.LightData.LightWavelength;
		const float VioletEnd = 400.0f;
		const float BlueEnd = 475.0f;
		return FMath::GetMappedRangeValueClamped(
			FVector2D(VioletEnd, BlueEnd),
			FVector2D(0.0f, 1.0f),
			CurrentLightWavelengthValue);
	}	
};


