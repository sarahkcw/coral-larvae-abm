#include "DataGrid/DataGrid.h"
#include <stdexcept>
#include "DataGrid/DataGridUtils.h"
#include "GameFramework/Actor.h"

namespace
{
bool IsGridStorageConsistent(const FDataConfig& Config, int32 NumCells)
{
    if (Config.CellEdgeLength <= 0.0f || Config.CellCountX <= 0 || Config.CellCountY <= 0 || Config.CellCountZ <= 0)
        return false;

    const int64 ExpectedCells = static_cast<int64>(Config.CellCountX) * Config.CellCountY * Config.CellCountZ;
    return ExpectedCells > 0 && ExpectedCells == NumCells;
}

void ResetToEmptyGrid(FDataConfig& DataConfig, FDataChunk& Grid)
{
    Grid = FDataChunk::EmptyChunk();
    DataConfig = Grid.Config;
}
}

UDataGrid::UDataGrid()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UDataGrid::Init(const FDataConfig Config)
{
    DataConfig = Config;
    if (DataConfig.CellEdgeLength <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid DataGrid CellEdgeLength: %f"), DataConfig.CellEdgeLength);
        ResetToEmptyGrid(DataConfig, Grid);
        return;
    }

    DataConfig.CellCountX = FMath::CeilToInt( Config.LocalBounds.X / Config.CellEdgeLength);
    DataConfig.CellCountY = FMath::CeilToInt( Config.LocalBounds.Y / Config.CellEdgeLength);
    DataConfig.CellCountZ = FMath::CeilToInt( Config.LocalBounds.Z / Config.CellEdgeLength);
    DataConfig.ChunkWorldOrigin = Config.ChunkWorldOrigin;
    const int64 TotalCells = static_cast<int64>(DataConfig.CellCountX) * DataConfig.CellCountY * DataConfig.CellCountZ;
    constexpr int64 MaxSafeGridCells = 5000000;
    if (TotalCells <= 0 || TotalCells > MaxSafeGridCells)
    {
        UE_LOG(LogTemp, Error, TEXT("Refusing to create DataGrid with %lld cells. Check WorldBounds (%s) and CellEdgeLength (%f)."),
            TotalCells, *DataConfig.LocalBounds.ToString(), DataConfig.CellEdgeLength);
        ResetToEmptyGrid(DataConfig, Grid);
        return;
    }

    Grid.Data.Empty();
    Grid.Data.Reserve(static_cast<int32>(TotalCells));
    Grid.Config = DataConfig;

    for (int Z = 0; Z < DataConfig.CellCountZ; ++Z)
    {
        for (int Y = 0; Y < DataConfig.CellCountY; ++Y)
        {
            for (int X = 0; X < DataConfig.CellCountX; ++X)
            {
                // Create some random current values
                const auto Idx = FDataIndex{X, Y, Z};
                
                FIndexedCellData CellPackage;
                CellPackage.Index = Idx;
                CellPackage.WorldPosition = DataConfig.ChunkWorldOrigin + UDataGridUtils::GetLocalCellOrigin(Idx, DataConfig);
                auto& [WaterData, LightData, ReefData, bIsReefCell] = CellPackage.Data;
                WaterData.Current = FVector(FMath::FRandRange(-10.f, 10.f), FMath::FRandRange(-10.f, 10.f),
                                  FMath::FRandRange(-10.f, 10.f));
                WaterData.Pressure = 0;
                WaterData.Salinity = 0;
                WaterData.Temperature = 0;
                WaterData.Turbulence = 0;
                Grid.Data.Add(MoveTemp(CellPackage)); 
            }
        }
    }
}


void UDataGrid::SetDataAtWorldLocation(FVector Location, FCellData CellData)
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()))
        return;

    const auto DataIndex = UDataGridUtils::GetCellDataIndexFromWorldPosition(Location, DataConfig);
    SetDataAtCellIndex(DataIndex, CellData);
}

void UDataGrid::SetCellInvalidAtWorldLocation(FVector Location)
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()))
        return;

    const auto DataIndex = UDataGridUtils::GetCellDataIndexFromWorldPosition(Location, DataConfig);
    if (!UDataGridUtils::IsDataIndexInChunk(DataIndex, DataConfig))
        return;

    const int CellIndex = UDataGridUtils::GetMemoryIndexAt(DataIndex, DataConfig);
    if (Grid.Data.IsValidIndex(CellIndex))
        Grid.Data[CellIndex].bInvalid = true;
}

void UDataGrid::SetDataAtCellIndex(FDataIndex Index, FCellData CellData)
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()) || !UDataGridUtils::IsDataIndexInChunk(Index, DataConfig))
        return;

    const int CellIndex = UDataGridUtils::GetMemoryIndexAt(Index, DataConfig);
    if (Grid.Data.IsValidIndex(CellIndex))
        Grid.Data[CellIndex].Data = CellData;
}

FDataChunk UDataGrid::GetCellDataSphereAround(FVector WorldPosition, float Radius)
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()) || !UDataGridUtils::IsInBounds(DataConfig, WorldPosition))
        return FDataChunk::EmptyChunk();
    const int HalfHeightCells = FMath::CeilToInt(Radius / DataConfig.CellEdgeLength);
    return GetCellDataInRange(WorldPosition, HalfHeightCells, Radius);
}

FDataChunk UDataGrid::GetCellDataAsChunkAt(FVector WorldPosition, int HalfHeightCells)
{
    return GetCellDataInRange(WorldPosition, HalfHeightCells);
}

FIndexedCellData UDataGrid::GetCellAtPoint(FVector WorldPosition)
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()) || !UDataGridUtils::IsInBounds(DataConfig, WorldPosition))
        return FIndexedCellData::Invalid(FDataIndex{-1, -1, -1}, WorldPosition);
    return UDataGridUtils::GetCellAtPoint(WorldPosition, Grid);
}

const FDataChunk& UDataGrid::GetBulkData()
{
    return Grid;
}

FDataConfig UDataGrid::GetDataConfig()
{
    return DataConfig;
}

void UDataGrid::CopyDataFrom(const UDataGrid* OtherDataGrid)
{
    Grid = OtherDataGrid->Grid;
    DataConfig = OtherDataGrid->DataConfig;
}

FDataChunk UDataGrid::GetCellDataInRange(const FVector& Position, int HalfHeightCells, float Radius) const
{
    if (!IsGridStorageConsistent(DataConfig, Grid.Data.Num()) || !UDataGridUtils::IsInBounds(DataConfig, Position))
        return FDataChunk::EmptyChunk();
    const FDataIndex CenterIndex = UDataGridUtils::GetCellDataIndexFromWorldPosition(Position, DataConfig);
    
    FDataChunk ResultChunk;
    
    int MinX = FMath::Max(CenterIndex.X - HalfHeightCells, 0);
    int MaxX = FMath::Min(CenterIndex.X + HalfHeightCells, DataConfig.CellCountX - 1);
    int MinY = FMath::Max(CenterIndex.Y - HalfHeightCells, 0);
    int MaxY = FMath::Min(CenterIndex.Y + HalfHeightCells, DataConfig.CellCountY - 1);
    int MinZ = FMath::Max(CenterIndex.Z - HalfHeightCells, 0);
    int MaxZ = FMath::Min(CenterIndex.Z + HalfHeightCells, DataConfig.CellCountZ - 1);
    
    // DataChunk has no overlap with the grid. Returning a chunk with only one invalid cell
    if (MinX > MaxX || MinY > MaxY || MinZ > MaxZ)
        return FDataChunk::EmptyChunk();
    
    // Iterate over the range of indices and collect the cells
    for (int Z = MinZ; Z <= MaxZ; ++Z)
    {
        for (int Y = MinY; Y <= MaxY; ++Y)
        {
            for (int X = MinX; X <= MaxX; ++X)
            {
                auto Idx = FDataIndex{X, Y, Z};
                if (!UDataGridUtils::IsDataIndexInChunk(Idx, DataConfig))
                {
                    ResultChunk.Data.Add(FIndexedCellData::Invalid(Idx, DataConfig.ChunkWorldOrigin + FVector(X, Y, Z) * DataConfig.CellEdgeLength));
                    continue;
                }

                const int CellIndex = UDataGridUtils::GetMemoryIndexAt(Idx, DataConfig);
                if (!Grid.Data.IsValidIndex(CellIndex))
                    continue;

                if (Radius > 0)
                {
                    // If Radius is specified, include cells within the radius
                    FVector CellOrigin = Grid.Data[CellIndex].WorldPosition;
                    if (FVector::Distance(Position, CellOrigin) <= Radius)
                    {
                        ResultChunk.Data.Add(Grid.Data[CellIndex]);
                    }
                }
                else
                {
                    // If Radius is not specified, include all cells in the range
                    ResultChunk.Data.Add(Grid.Data[CellIndex]);
                }
            }
        }
    }

    // If the radius specified is too small, no cell will be included in the chunk.
    // We don't want this behavior. To avoid this, add the center cell if no cell is included in the chunk.
    if (ResultChunk.Data.Num() == 0)
    {
        const int CenterCellIndex = UDataGridUtils::GetMemoryIndexAt(CenterIndex, DataConfig);
        if (Grid.Data.IsValidIndex(CenterCellIndex))
            ResultChunk.Data.Add(Grid.Data[CenterCellIndex]);
    }

    if (ResultChunk.Data.Num() == 0)
        return FDataChunk::EmptyChunk();
    
    ResultChunk.Config.CellCountX = MaxX - MinX + 1;
    ResultChunk.Config.CellCountY = MaxY - MinY + 1;
    ResultChunk.Config.CellCountZ = MaxZ - MinZ + 1;
    ResultChunk.Config.LocalBounds = FVector(ResultChunk.Config.CellCountX, ResultChunk.Config.CellCountY, ResultChunk.Config.CellCountZ) * DataConfig.CellEdgeLength;
    ResultChunk.Config.CellEdgeLength = DataConfig.CellEdgeLength;
    ResultChunk.Config.ChunkIndex = FDataIndex{MinX, MinY, MinZ};
    
    const int MemoryIdx = UDataGridUtils::GetMemoryIndexAt(ResultChunk.Config.ChunkIndex, DataConfig); 
    if (!Grid.Data.IsValidIndex(MemoryIdx))
        return FDataChunk::EmptyChunk();

    const auto MinCell = Grid.Data[MemoryIdx];
    ResultChunk.Config.ChunkWorldOrigin = MinCell.WorldPosition - DataConfig.CellEdgeLength / 2.f;
    
    return ResultChunk;
}


