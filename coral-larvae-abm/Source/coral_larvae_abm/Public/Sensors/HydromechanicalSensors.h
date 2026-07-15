#pragma once
#include "CoreMinimal.h"
#include "LarvalSensorBaseComponent.h"
#include "..\DataGrid\DataGridUtils.h"
#include "HydromechanicalSensors.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API UPressureSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentPressure = Cell.Data.WaterData.Pressure;

		const float SurfacePressureAtm = 1.0f;
		const float Density = 1025.0f;
		const float Gravity = 9.81f;
		const float PascalToAtmospheres = 101325.0f;
		const float MaxDepthMeters = FMath::Max(
			(UpdateParams.GlobalExperimentConfig.LocalBounds.Z - UpdateParams.GlobalExperimentConfig.CellEdgeLength / 2.0f) / 100.0f,
			0.001f);
		const float MaxPressureAtm = SurfacePressureAtm + (Density * Gravity * MaxDepthMeters) / PascalToAtmospheres;

		return FMath::GetMappedRangeValueClamped(
			FVector2D(SurfacePressureAtm, MaxPressureAtm),
			FVector2D(0.0f, 1.0f),
			CurrentPressure);
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UTemperatureSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentTemperature = Cell.Data.WaterData.Temperature;

		const float MinTemperature = 20.0f;
		const float MaxTemperature = 40.0f;
		return FMath::GetMappedRangeValueClamped(
			FVector2D(MinTemperature, MaxTemperature),
			FVector2D(0.0f, 1.0f),
			CurrentTemperature);
	}	
};
