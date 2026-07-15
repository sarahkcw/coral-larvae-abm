#pragma once
#include "CoreMinimal.h"
#include "LarvalSensorBaseComponent.h"
#include "SensorFunctions.h"
#include "..\DataGrid\DataGridUtils.h"
#include "ChemicalSensors.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API UCCASensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;

		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentCCAValue = Cell.Data.ReefData.CCA;
		return FMath::Clamp(CurrentCCAValue, 0.0, 1.0);
	}
};


UCLASS()
class CORAL_LARVAE_ABM_API UCcaDirectionSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		// This sensor returns the "weight" of the CCA gradient between the cell in front and the cell behind the agent
		// So if there is no cca in front and only a little behind, the sensor will return 1 etc.
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
		
		auto FCellCca = ForwardCell.Data.ReefData.CCA;
		auto BCellCca = BackwardCell.Data.ReefData.CCA;
		
		float SumCca = FCellCca + BCellCca;
		if (SumCca == 0)
			return 0;

		// Return a gradient between -1 and 1 depending on the difference between the two cells
		return (FCellCca - BCellCca) / SumCca;
	}

	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) 	{ return FVector(0.f); }
};



UCLASS()
class CORAL_LARVAE_ABM_API UCcaFwdBackSensor : public UCcaDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override
	{
		return UpdateParams.ActorTransform.GetRotation().GetForwardVector().GetSafeNormal();
	}
};

UCLASS()
class CORAL_LARVAE_ABM_API UCcaLRSensor : public UCcaDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override
	{
		return UpdateParams.ActorTransform.GetRotation().GetRightVector().GetSafeNormal();
	}
};

UCLASS()
class CORAL_LARVAE_ABM_API UCcaUDSensor : public UCcaDirectionSensor
{
	GENERATED_BODY()
	virtual FVector GetDirection(FSensorUpdateParams UpdateParams) override
	{
		return UpdateParams.ActorTransform.GetRotation().GetUpVector().GetSafeNormal();
	}
};



UCLASS()
class CORAL_LARVAE_ABM_API UCCAForwardSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;
		
		// Get Cell in front of current position
		auto ForwardPosition = UpdateParams.ActorTransform.GetLocation() + UpdateParams.ActorTransform.GetRotation().GetForwardVector().GetSafeNormal() * UpdateParams.DataChunk.Config.CellEdgeLength;
		auto ForwardCell = UDataGridUtils::GetCellAtPoint(ForwardPosition, UpdateParams.DataChunk);
		
		float CurrentCCAValue = ForwardCell.Data.ReefData.CCA; // Cell.Data.ReefData.CCA;
		return FMath::Clamp(CurrentCCAValue, 0.0, 1.0);
	}
};

UCLASS()
class CORAL_LARVAE_ABM_API UCCARightSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;
		
		// Get Cell in front of current position
		auto SidePosition = UpdateParams.ActorTransform.GetLocation() + UpdateParams.ActorTransform.GetRotation().GetRightVector().GetSafeNormal() * UpdateParams.DataChunk.Config.CellEdgeLength;
		auto RightCell = UDataGridUtils::GetCellAtPoint(SidePosition, UpdateParams.DataChunk);
		
		float CurrentCCAValue = RightCell.Data.ReefData.CCA; 
		return FMath::Clamp(CurrentCCAValue, 0.f, 1.f);
	}
};

UCLASS()
class CORAL_LARVAE_ABM_API UCCALeftSensor : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;
		
		// Get Cell in front of current position
		auto SidePosition = UpdateParams.ActorTransform.GetLocation() - UpdateParams.ActorTransform.GetRotation().GetRightVector().GetSafeNormal() * UpdateParams.DataChunk.Config.CellEdgeLength;
		auto LeftCell = UDataGridUtils::GetCellAtPoint(SidePosition, UpdateParams.DataChunk);
		
		float CurrentCCAValue = LeftCell.Data.ReefData.CCA;
		return FMath::Clamp(CurrentCCAValue, 0.f, 1.f);
	}
};


UCLASS()
class CORAL_LARVAE_ABM_API UAlteromonasBioFilm : public ULarvalSensorBaseComponent
{
	GENERATED_BODY()
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) override
	{
		auto CurrentPosition = UpdateParams.ActorTransform.GetLocation();
		if(!UDataGridUtils::IsInBounds(UpdateParams.GlobalExperimentConfig, CurrentPosition)) return 0;
		
		auto Cell = UDataGridUtils::GetCellAtPoint(UpdateParams.ActorTransform.GetLocation(), UpdateParams.DataChunk);
		float CurrentAlteromonasValue = Cell.Data.ReefData.Alteromonas;
		return CurrentAlteromonasValue;
	}	
};
