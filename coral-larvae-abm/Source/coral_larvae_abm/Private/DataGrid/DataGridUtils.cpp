#include "..\..\Public\DataGrid\DataGridUtils.h"
#include "Components/ActorComponent.h"

namespace
{
bool IsChunkStorageConsistent(const FDataChunk& DataChunk)
{
	const FDataConfig& Config = DataChunk.Config;
	if (Config.CellEdgeLength <= 0.0f || Config.CellCountX <= 0 || Config.CellCountY <= 0 || Config.CellCountZ <= 0)
		return false;

	const int64 ExpectedCells = static_cast<int64>(Config.CellCountX) * Config.CellCountY * Config.CellCountZ;
	return ExpectedCells > 0 && ExpectedCells == DataChunk.Data.Num();
}
}

int UDataGridUtils::GetMemoryIndexAt(const FDataIndex Index, const FDataConfig& Config)
{
	const int NumX = Config.CellCountX;
	const int NumY = Config.CellCountY;
	const int NumZ = Config.CellCountZ;
	const auto [X, Y, Z] = Index - Config.ChunkIndex;

	if (X < 0 || Y < 0 || Z < 0 || X >= NumX || Y >= NumY || Z >= NumZ)
		throw std::out_of_range("Out of range: Trying to access data outside the grid!");

	const int FlatIndex = (Z * NumX * NumY) + (Y * NumX) + X;
	if (FlatIndex >= NumX * NumY * NumZ)
		throw std::out_of_range("Flat index out of range of dataset memory!");
	
	return FlatIndex;
}

FVector UDataGridUtils::GetLocalCellOrigin(FDataIndex Idx, const FDataConfig& Config)
{
	return FVector(Idx.X * Config.CellEdgeLength, Idx.Y * Config.CellEdgeLength, Idx.Z * Config.CellEdgeLength) + FVector(Config.CellEdgeLength / 2);
}

FDataIndex UDataGridUtils::GetCellDataIndexFromWorldPosition(const FVector& Position, const FDataConfig& Config)
{
	FDataIndex Result;
	const auto ShiftedPoint = Position - Config.ChunkWorldOrigin;
	Result.X = floor(ShiftedPoint.X/Config.CellEdgeLength);
	Result.Y = floor(ShiftedPoint.Y/Config.CellEdgeLength);
	Result.Z = floor(ShiftedPoint.Z/Config.CellEdgeLength);
	return Result + Config.ChunkIndex;
}


bool UDataGridUtils::IsDataIndexInChunk(const FDataIndex& Index, const FDataConfig& Config)
{
	const auto [X, Y, Z] = Index - Config.ChunkIndex;
	return X >= 0 && X < Config.CellCountX &&
	   Y >= 0 && Y < Config.CellCountY &&
	   Z >= 0 && Z < Config.CellCountZ;
}

bool UDataGridUtils::IsInBounds(const FDataConfig& Config, const FVector& Position)
{
	FVector Corner = Config.ChunkWorldOrigin;
	const FVector MaxCorner = Config.ChunkWorldOrigin + Config.LocalBounds;
	bool bInBounds = Position.X <= MaxCorner.X && Position.X >= Corner.X &&
					 Position.Y <= MaxCorner.Y && Position.Y >= Corner.Y &&
					 Position.Z <= MaxCorner.Z && Position.Z >= Corner.Z;
	return bInBounds;
}

bool UDataGridUtils::IsInBoundsWithThreshold(const FDataConfig& Config, const FVector& Position, float Threshold)
{
	FVector Corner = Config.ChunkWorldOrigin;
	const FVector MaxCorner = Config.ChunkWorldOrigin + Config.LocalBounds;
	bool bInBounds = Position.X <= MaxCorner.X + Threshold && Position.X >= Corner.X - Threshold &&
					 Position.Y <= MaxCorner.Y + Threshold && Position.Y >= Corner.Y - Threshold &&
					 Position.Z <= MaxCorner.Z + Threshold && Position.Z >= Corner.Z - Threshold;
	return bInBounds;
}

bool UDataGridUtils::IsInReefCell(const FDataChunk& DataChunk, const FVector& Position)
{
	if (!IsChunkStorageConsistent(DataChunk))
	{
		return false;
	}

	if(!IsInBounds(DataChunk.Config, Position))
	{
		return false;
	}
	auto Cell = GetCellAtPoint(Position, DataChunk);
	return Cell.Data.bIsReefCell;
}

TArray<FVector> UDataGridUtils::GetReefCellPositions(const FDataChunk& DataChunk)
{
	TArray<FVector> ReefCellPositions;
	for (auto Cell : DataChunk.Data)
	{
		if (Cell.Data.bIsReefCell)
		{
			ReefCellPositions.Add(Cell.WorldPosition);
		}
	}
	return ReefCellPositions;
}

FIndexedCellData UDataGridUtils::GetCellAtPoint(FVector WorldPosition, const FDataChunk& DataChunk)
{
	if (!IsChunkStorageConsistent(DataChunk))
	{
		return FIndexedCellData::Invalid(FDataIndex{-1, -1, -1}, WorldPosition);
	}

	const auto DataConfig = DataChunk.Config;
	const auto Idx = GetCellDataIndexFromWorldPosition(WorldPosition, DataConfig);
	
	if (!IsDataIndexInChunk(Idx, DataConfig))
	{
		const auto CellLocation = DataConfig.ChunkWorldOrigin + FVector(Idx.X, Idx.Y, Idx.Z) * DataConfig.CellEdgeLength;
		return FIndexedCellData::Invalid(Idx, CellLocation);
	}

	const int CellIndex = GetMemoryIndexAt(Idx, DataConfig);
	if (!DataChunk.Data.IsValidIndex(CellIndex))
	{
		return FIndexedCellData::Invalid(Idx, WorldPosition);
	}

	return DataChunk.Data[CellIndex];
}
