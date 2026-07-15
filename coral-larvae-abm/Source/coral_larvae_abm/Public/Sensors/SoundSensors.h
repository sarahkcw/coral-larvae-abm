#pragma once
#include "CoreMinimal.h"
#include "LarvalSensorBaseComponent.h"
#include "SensorFunctions.h"
#include "..\DataGrid\DataGridUtils.h"
#include "SoundSensors.generated.h"


UCLASS()
class CORAL_LARVAE_ABM_API UParticleMotionDirectionSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto ActorTransform = UpdateParams.ActorTransform;
		auto DataConfig = UpdateParams.DataChunk.Config;
		
		auto CurrentPosition = ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition))
			return 0;

		auto Direction = GetDirection(UpdateParams);
		auto ForwardPosition = CurrentPosition + Direction * DataConfig.CellEdgeLength;
		auto BackwardPosition = CurrentPosition - Direction * DataConfig.CellEdgeLength;

		auto ForwardCell = UDataGridUtils::GetCellAtPoint(ForwardPosition, UpdateParams.DataChunk);
		auto BackwardCell = UDataGridUtils::GetCellAtPoint(BackwardPosition, UpdateParams.DataChunk);
		
		auto FCellParticleMotion = ForwardCell.Data.WaterData.ParticleMotion;
		auto BCellParticleMotion = BackwardCell.Data.WaterData.ParticleMotion;
		
		float SumParticleMotion = FCellParticleMotion + BCellParticleMotion;
		if (SumParticleMotion == 0)
			return 0;

		// Return a gradient between -1 and 1 depending on the difference between the two cells
		return (FCellParticleMotion - BCellParticleMotion) / SumParticleMotion;
	}

	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) 	{ return FVector(0.f); }
};

UCLASS()
class CORAL_LARVAE_ABM_API UParticleMotionFwdBackSensor : public UParticleMotionDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override
	{
		return UpdateParams.ActorTransform.GetRotation().GetForwardVector().GetSafeNormal();
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UParticleMotionLRSensor : public UParticleMotionDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override
	{
		return UpdateParams.ActorTransform.GetRotation().GetRightVector().GetSafeNormal();
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UParticleMotionUDSensor : public UParticleMotionDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override 
	{
		return UpdateParams.ActorTransform.GetRotation().GetUpVector().GetSafeNormal();
	}	
};

UCLASS()
class CORAL_LARVAE_ABM_API UParticleMotionSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentParticleMotionValue = Cell.Data.WaterData.ParticleMotion;
		
		return FMath::Clamp(CurrentParticleMotionValue, 0.0, 1.0);
	}	
};



