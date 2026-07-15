#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataGridStructs.generated.h"

USTRUCT(BlueprintType)
struct FReefData
{
	GENERATED_BODY()
	float CCA = 0.f;
	float Alteromonas = 0.f;
	float DeadCoral = 0.f;
	float SettledLarvae = 0.f;
};

USTRUCT()
struct FLightData
{
	GENERATED_BODY()
	float LightIntensity = 0.f;
	float LightWavelength = 0.f;
};

USTRUCT()
struct FWaterData
{
	GENERATED_BODY()
	float Salinity = 0.f;
	float Temperature = 0.f;
	float Pressure = 0.f;
	FVector Current = FVector(0.f);
	float CurrentForce = 1.f;
	float Turbulence = 0.f;
	float ParticleMotion = 0.f;
};

USTRUCT(BlueprintType)
struct FCellData
{
	GENERATED_BODY()
	FWaterData WaterData;
	FLightData LightData;
	FReefData ReefData;

	bool bIsReefCell = false;
};

USTRUCT(BlueprintType)
struct FDataIndex
{
	GENERATED_BODY()
	int X, Y, Z = 0;

	bool operator==(const FDataIndex& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	FDataIndex operator + (const FDataIndex& Other) const
	{
		return {X + Other.X, Y + Other.Y, Z + Other.Z};
	}

	FDataIndex operator - (const FDataIndex& Other) const
	{
		return {X - Other.X, Y - Other.Y, Z - Other.Z};
	}
};
uint32 GetTypeHash(const FDataIndex& DataIndex);

// Define GetTypeHash for FDataIndex
inline uint32 GetTypeHash(const FDataIndex& DataIndex)
{
	// Combine the X, Y, and Z hash values
	return HashCombine(HashCombine(GetTypeHash(DataIndex.X), GetTypeHash(DataIndex.Y)), GetTypeHash(DataIndex.Z));
}

USTRUCT(BlueprintType)
struct FDataConfig
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FVector LocalBounds = FVector(0.f, 0.f, 0.f);
	UPROPERTY(BlueprintReadOnly) int CellCountX = 0;
	UPROPERTY(BlueprintReadOnly) int CellCountY = 0;
	UPROPERTY(BlueprintReadOnly) int CellCountZ = 0;
	UPROPERTY(BlueprintReadWrite) float CellEdgeLength = 0.f;
	UPROPERTY(BlueprintReadWrite) FVector ChunkWorldOrigin = FVector(0.f, 0.f, 0.f);
	FDataIndex ChunkIndex = {0, 0, 0};
};

USTRUCT(BlueprintType)
struct FIndexedCellData 
{
	GENERATED_BODY()
	FDataIndex Index;
	FVector WorldPosition;
	FCellData Data;
	bool bInvalid;
	
	static FIndexedCellData Invalid(FDataIndex Idx, FVector Pos)
	{
		FIndexedCellData Package;
		Package.Index = Idx;
		Package.WorldPosition = Pos;
		Package.bInvalid = true;
		return Package;	
	}
	bool IsInvalid() const { return bInvalid; }
};

USTRUCT(BlueprintType)
struct FDataChunk
{
	GENERATED_BODY()
	TArray<FIndexedCellData> Data;
	FDataConfig Config;

	static FDataChunk EmptyChunk()
	{
		FDataChunk Chunk;
		Chunk.Config.CellCountX = 1;
		Chunk.Config.CellCountY = 1;
		Chunk.Config.CellCountZ = 1;
		Chunk.Config.CellEdgeLength = 1;
		Chunk.Config.LocalBounds = FVector(1, 1, 1);
		Chunk.Config.ChunkWorldOrigin = FVector(0, 0, 0);
		Chunk.Data.Add(FIndexedCellData::Invalid({0, 0, 0}, FVector(0, 0, 0)));
		return Chunk;
	}
};
