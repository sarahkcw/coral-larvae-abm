#pragma once
#include "CoreMinimal.h"
#include "..\DataGrid\DataGridStructs.h"
#include <algorithm>
#include "GameFramework/Actor.h"

template <typename Func>
static float GetMaximumOf(FDataChunk& DataChunk, Func GetValue)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	float MaxValue = 0;
	for (const auto& [Index, WorldPosition, Data, _] : DataChunk.Data)
	{
		float Value = GetValue(Data);
		MaxValue = std::max(MaxValue, Value);
	}
	return MaxValue;
}

template <typename Func>
static float GetMinimumOf(FDataChunk& DataChunk, Func GetValue)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	float MinValue = 0;
	for (const auto& [Index, WorldPosition, Data, _] : DataChunk.Data)
	{
		float Value = GetValue(Data);
		MinValue = std::max(MinValue, Value);
	}
	return MinValue;
}

template <typename Func>
static float GetTotalAmountOf(FDataChunk& DataChunk, Func GetValue)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	float TotalAmount = 0;
	for (const auto& CellPackage  : DataChunk.Data)
	{
		TotalAmount += GetValue(CellPackage);
	}
	return TotalAmount;
}

template <typename Func>
static float GetAverageOf(FDataChunk& DataChunk, Func GetValue)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	
	float Average = 0;
	for (const auto& CellPackage : DataChunk.Data)
	{
		Average += GetValue(CellPackage);
	}
	Average /= DataChunk.Data.Num();
	return Average;
}

// Cells that are close have higher weight and vice versa or also with exponential distance decay
template <typename Func>
static float GetDistanceWeightedAverageOf(FDataChunk& DataChunk, Func GetValue, FVector AgentLocation)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	float WeightedAverage = 0;
	float TotalWeight = 0;
	for (const auto& Package : DataChunk.Data)
	{    
		float Distance = FVector::Dist(AgentLocation, Package.WorldPosition);
		float Weight = 1.0f / (Distance + 1.0f);
		WeightedAverage += GetValue(Package) * Weight;
		TotalWeight += Weight;
	}
	return WeightedAverage;
}

// Cells that are in angle alignment are weighted more / less energy for the agent
template <typename T, typename Func>
static float GetAngleWeightedAverageOf(FDataChunk& DataChunk, Func GetValue, float ConeAngle, FVector GridOwnerLocation, FRotator InitialAgentRotation)
{
	if (DataChunk.Data.Num() == 0) return 0.f;
	float WeightedAverage = 0;
	float TotalWeight = 0;

	for (const auto& Package : DataChunk.Data)
	{
		FVector CellPosition = FVector(0, 0, 0); 
		FVector DirectionToCell = (CellPosition - GridOwnerLocation).GetSafeNormal();
		float Distance = FVector::Dist(GridOwnerLocation, CellPosition);
		float WeightDistance = 1.0f / (Distance + 1.0f);

		FQuat QuatRotation = FQuat(InitialAgentRotation);
		FVector FacingDirection = QuatRotation.GetForwardVector().GetSafeNormal();

		float DotProduct = FVector::DotProduct(FacingDirection, DirectionToCell);
		float Angle = FMath::Acos(DotProduct);
		float WeightAngle = (Angle <= FMath::DegreesToRadians(ConeAngle / 2.0f)) ? FMath::Cos(Angle) : 0;

		float Weight = WeightDistance * WeightAngle;

		WeightedAverage += GetValue(Package) * Weight;
		TotalWeight += Weight;

		if (TotalWeight > 0)
		{
			WeightedAverage /= TotalWeight;
		}
		else
		{
			WeightedAverage = 0;
		}
	}
	return WeightedAverage;
}

static float ApplySigmoid(const float SensedValue, float SensitivityValue = 1.f)
{
	return 1.0f / (1.0f + FMath::Exp(-SensitivityValue * SensedValue));
}

static float NormalizeSensorValue(float CurrentValue, float MinValue, float MaxValue)
{
	if (MinValue == MaxValue) return 0.0f; 
	return (CurrentValue - MinValue) / (MaxValue - MinValue);
}
